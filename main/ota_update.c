#include "ota_update.h"
#include "wifi.h"
#include "tracker_scanner.h"

#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"

#define OTA_UPDATE_TASK_PRIORITY        5
#define OTA_UPDATE_TASK_STACK_SIZE      8192
#define OTA_UPDATE_TASK_NAME            "ota_update"
#define OTA_UPDATE_CHECK_NOW_BIT        BIT0

#define GITHUB_API_URL_FORMAT           "https://api.github.com/repos/%s/%s/releases/latest"
#define GITHUB_API_MAX_URL_LEN          256
#define GITHUB_API_RESPONSE_BUFFER_SIZE 4096
#define FIRMWARE_ASSET_PREFIX           "homepost-"
#define FIRMWARE_ASSET_SUFFIX           ".bin"
#define TAG_PREFIX                      "release-v"
#define VERSION_STRING_MAX_LEN          32
#define FIRMWARE_URL_MAX_LEN            512

static const char *TAG = __FILE__;
static TaskHandle_t ota_task_handle = NULL;
static EventGroupHandle_t ota_event_group = NULL;
static char available_version[VERSION_STRING_MAX_LEN] = {0};
static char firmware_download_url[FIRMWARE_URL_MAX_LEN] = {0};
static bool update_available = false;

/**
 * @brief Compare two semver version strings
 * 
 * @param v1 First version string (e.g., "1.2.0")
 * @param v2 Second version string (e.g., "1.2.1")
 * @return >0 if v1 > v2, <0 if v1 < v2, 0 if equal
 */
static int compare_versions(const char *v1, const char *v2)
{
    int major1 = 0, minor1 = 0, patch1 = 0;
    int major2 = 0, minor2 = 0, patch2 = 0;

    sscanf(v1, "%d.%d.%d", &major1, &minor1, &patch1);
    sscanf(v2, "%d.%d.%d", &major2, &minor2, &patch2);

    if (major1 != major2) return major1 - major2;
    if (minor1 != minor2) return minor1 - minor2;
    return patch1 - patch2;
}

/**
 * @brief Parse version from GitHub tag_name (e.g., "release-v1.2.0" -> "1.2.0")
 * 
 * @param tag_name The GitHub tag name
 * @param version_out Buffer to store parsed version
 * @param version_out_len Length of output buffer
 * @return ESP_OK on success, ESP_FAIL if tag format is invalid
 */
static esp_err_t parse_version_from_tag(const char *tag_name, char *version_out, size_t version_out_len)
{
    const char *prefix = TAG_PREFIX;
    size_t prefix_len = strlen(prefix);

    if (strncmp(tag_name, prefix, prefix_len) != 0) {
        ESP_LOGE(TAG, "Invalid tag format: %s (expected prefix: %s)", tag_name, prefix);
        return ESP_FAIL;
    }

    const char *version_start = tag_name + prefix_len;
    if (strlen(version_start) >= version_out_len) {
        ESP_LOGE(TAG, "Version string too long");
        return ESP_FAIL;
    }

    strncpy(version_out, version_start, version_out_len - 1);
    version_out[version_out_len - 1] = '\0';
    return ESP_OK;
}

/**
 * @brief Find firmware download URL from GitHub release assets
 * 
 * Looks for asset matching pattern "homepost-{version}.bin"
 * 
 * @param assets cJSON array of release assets
 * @param version Version string to match
 * @param url_out Buffer to store download URL
 * @param url_out_len Length of output buffer
 * @return ESP_OK on success, ESP_FAIL if asset not found
 */
static esp_err_t find_firmware_asset_url(cJSON *assets, const char *version, char *url_out, size_t url_out_len)
{
    char expected_filename[64];
    snprintf(expected_filename, sizeof(expected_filename), "%s%s%s", FIRMWARE_ASSET_PREFIX, version, FIRMWARE_ASSET_SUFFIX);

    cJSON *asset;
    cJSON_ArrayForEach(asset, assets) {
        cJSON *name = cJSON_GetObjectItem(asset, "name");
        cJSON *browser_download_url = cJSON_GetObjectItem(asset, "browser_download_url");

        if (cJSON_IsString(name) && cJSON_IsString(browser_download_url)) {
            if (strcmp(name->valuestring, expected_filename) == 0) {
                if (strlen(browser_download_url->valuestring) >= url_out_len) {
                    ESP_LOGE(TAG, "Download URL too long");
                    return ESP_FAIL;
                }
                strncpy(url_out, browser_download_url->valuestring, url_out_len - 1);
                url_out[url_out_len - 1] = '\0';
                ESP_LOGI(TAG, "Found firmware asset: %s", expected_filename);
                return ESP_OK;
            }
        }
    }

    ESP_LOGE(TAG, "Firmware asset not found: %s", expected_filename);
    return ESP_FAIL;
}

/**
 * @brief HTTP event handler for GitHub API requests
 */
static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    static int output_len = 0;

    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            if (!esp_http_client_is_chunked_response(evt->client)) {
                if (evt->user_data) {
                    int copy_len = evt->data_len;
                    if (output_len + copy_len >= GITHUB_API_RESPONSE_BUFFER_SIZE) {
                        copy_len = GITHUB_API_RESPONSE_BUFFER_SIZE - output_len - 1;
                    }
                    if (copy_len > 0) {
                        memcpy((char*)evt->user_data + output_len, evt->data, copy_len);
                        output_len += copy_len;
                        ((char*)evt->user_data)[output_len] = '\0';
                    }
                }
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            output_len = 0;
            break;
        case HTTP_EVENT_DISCONNECTED:
            output_len = 0;
            break;
        default:
            break;
    }
    return ESP_OK;
}

/**
 * @brief Check GitHub for the latest release and compare versions
 * 
 * @return ESP_OK if check successful (regardless of update availability)
 */
static esp_err_t check_for_update(void)
{
    esp_err_t ret = ESP_FAIL;
    char *response_buffer = NULL;
    char api_url[GITHUB_API_MAX_URL_LEN];
    cJSON *json = NULL;

    // Build GitHub API URL
    snprintf(api_url, sizeof(api_url), GITHUB_API_URL_FORMAT,
             CONFIG_HOMEPOST_OTA_GITHUB_OWNER, CONFIG_HOMEPOST_OTA_GITHUB_REPO);
    ESP_LOGI(TAG, "Checking for updates at: %s", api_url);

    // Allocate response buffer
    response_buffer = calloc(1, GITHUB_API_RESPONSE_BUFFER_SIZE);
    if (response_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate response buffer");
        return ESP_ERR_NO_MEM;
    }

    // Configure HTTP client for GitHub API
    esp_http_client_config_t config = {
        .url = api_url,
        .event_handler = http_event_handler,
        .user_data = response_buffer,
        .timeout_ms = 10000,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        free(response_buffer);
        return ESP_FAIL;
    }

    // Set headers required by GitHub API
    esp_http_client_set_header(client, "Accept", "application/vnd.github.v3+json");
    esp_http_client_set_header(client, "User-Agent", "ESP32-Homepost");

    // Perform request
    ret = esp_http_client_perform(client);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(ret));
        goto cleanup;
    }

    int status_code = esp_http_client_get_status_code(client);
    if (status_code != 200) {
        ESP_LOGE(TAG, "GitHub API returned status %d", status_code);
        ret = ESP_FAIL;
        goto cleanup;
    }

    ESP_LOGD(TAG, "GitHub API response: %s", response_buffer);

    // Parse JSON response
    json = cJSON_Parse(response_buffer);
    if (json == NULL) {
        ESP_LOGE(TAG, "Failed to parse JSON response");
        ret = ESP_FAIL;
        goto cleanup;
    }

    // Extract tag_name
    cJSON *tag_name = cJSON_GetObjectItem(json, "tag_name");
    if (!cJSON_IsString(tag_name)) {
        ESP_LOGE(TAG, "tag_name not found in response");
        ret = ESP_FAIL;
        goto cleanup;
    }

    // Parse version from tag
    char remote_version[VERSION_STRING_MAX_LEN];
    ret = parse_version_from_tag(tag_name->valuestring, remote_version, sizeof(remote_version));
    if (ret != ESP_OK) {
        goto cleanup;
    }

    ESP_LOGI(TAG, "Current version: %s, Latest version: %s", CONFIG_APP_PROJECT_VER, remote_version);

    // Compare versions
    if (compare_versions(remote_version, CONFIG_APP_PROJECT_VER) > 0) {
        ESP_LOGI(TAG, "New version available: %s", remote_version);

        // Find firmware download URL
        cJSON *assets = cJSON_GetObjectItem(json, "assets");
        if (!cJSON_IsArray(assets)) {
            ESP_LOGE(TAG, "assets array not found in response");
            ret = ESP_FAIL;
            goto cleanup;
        }

        ret = find_firmware_asset_url(assets, remote_version, firmware_download_url, sizeof(firmware_download_url));
        if (ret != ESP_OK) {
            goto cleanup;
        }

        strncpy(available_version, remote_version, sizeof(available_version) - 1);
        update_available = true;
    } else {
        ESP_LOGI(TAG, "Firmware is up to date");
        update_available = false;
        available_version[0] = '\0';
    }

    ret = ESP_OK;

cleanup:
    if (json) cJSON_Delete(json);
    esp_http_client_cleanup(client);
    free(response_buffer);
    return ret;
}

/**
 * @brief Perform the OTA update
 * 
 * Downloads firmware from GitHub and flashes it
 */
static esp_err_t perform_ota_update(void)
{
    if (!update_available || firmware_download_url[0] == '\0') {
        ESP_LOGE(TAG, "No update available or download URL not set");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Starting OTA update from: %s", firmware_download_url);

    ESP_LOGI(TAG, "Stopping tracker scanner and MQTT connection to free memory...");
    tracker_scanner_stop_task();
    mqtt_connection_stop_task();
    vTaskDelay(pdMS_TO_TICKS(3000));

    esp_http_client_config_t config = {
        .url = firmware_download_url,
        .timeout_ms = 60000,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .keep_alive_enable = true,
        .buffer_size = 8 * 1024,
        .buffer_size_tx = 8 * 1024,
    };

    esp_https_ota_config_t ota_config = {
        .http_config = &config,
    };

    ESP_LOGI(TAG, "Attempting OTA update...");
    esp_err_t ret = esp_https_ota(&ota_config);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "OTA update successful, restarting...");
        esp_restart();
    } else {
        ESP_LOGE(TAG, "OTA update failed: %s", esp_err_to_name(ret));
        // Restart tracker scanner since update failed
        tracker_scanner_start_task();
        mqtt_connection_start_task();
    }

    return ret;
}

/**
 * @brief OTA update task
 */
static void ota_update_task(void *arg)
{
    // Initial delay for network stability
    ESP_LOGI(TAG, "OTA task started, waiting %d seconds for initial delay...", 
             CONFIG_HOMEPOST_OTA_INITIAL_DELAY_SECONDS);
    vTaskDelay(pdMS_TO_TICKS(CONFIG_HOMEPOST_OTA_INITIAL_DELAY_SECONDS * 1000));

    while (true) {
        // Wait for WiFi connection
        while (!wifi_is_connected_to_internet()) {
            ESP_LOGD(TAG, "Waiting for WiFi connection...");
            vTaskDelay(pdMS_TO_TICKS(5000));
        }

        // Check for updates
        esp_err_t ret = check_for_update();
        if (ret == ESP_OK && update_available) {
            // Perform OTA if update is available
            perform_ota_update();
        }

        // Wait for next check interval or manual trigger
        TickType_t wait_time = pdMS_TO_TICKS(CONFIG_HOMEPOST_OTA_CHECK_INTERVAL_HOURS * 60 * 60 * 1000);
        EventBits_t bits = xEventGroupWaitBits(ota_event_group, OTA_UPDATE_CHECK_NOW_BIT, 
                                                pdTRUE, pdFALSE, wait_time);
        
        if (bits & OTA_UPDATE_CHECK_NOW_BIT) {
            ESP_LOGI(TAG, "Manual update check triggered");
        }
    }
}

void ota_update_start_task(void)
{
    if (ota_task_handle != NULL) {
        ESP_LOGW(TAG, "OTA task already running");
        return;
    }

    ota_event_group = xEventGroupCreate();
    configASSERT(ota_event_group);

    xTaskCreate(ota_update_task, OTA_UPDATE_TASK_NAME, OTA_UPDATE_TASK_STACK_SIZE, 
                NULL, OTA_UPDATE_TASK_PRIORITY, &ota_task_handle);
    configASSERT(ota_task_handle);

    ESP_LOGI(TAG, "OTA update task started");
}

void ota_update_stop_task(void)
{
    if (ota_task_handle != NULL) {
        vTaskDelete(ota_task_handle);
        ota_task_handle = NULL;
    }
    if (ota_event_group != NULL) {
        vEventGroupDelete(ota_event_group);
        ota_event_group = NULL;
    }
    ESP_LOGI(TAG, "OTA update task stopped");
}

esp_err_t ota_update_check_now(void)
{
    if (ota_event_group == NULL) {
        return ESP_FAIL;
    }
    xEventGroupSetBits(ota_event_group, OTA_UPDATE_CHECK_NOW_BIT);
    return ESP_OK;
}

bool ota_update_is_available(void)
{
    return update_available;
}

const char* ota_update_get_current_version(void)
{
    return CONFIG_APP_PROJECT_VER;
}

const char* ota_update_get_available_version(void)
{
    if (available_version[0] == '\0') {
        return NULL;
    }
    return available_version;
}

esp_err_t ota_update_check_for_update(void)
{
    return check_for_update();
}
