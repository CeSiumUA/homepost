#include "http_server.h"

#if CONFIG_HOMEPOST_OTA_ENABLED
#include "ota_update.h"
#endif

static const char *TAG = __FILE__;
static httpd_handle_t server = NULL;
static esp_timer_handle_t restart_timer;

static void http_server_restart_timer_callback(void *arg);

static const esp_timer_create_args_t restart_timer_args = {
        .callback = &http_server_restart_timer_callback,
};

static esp_err_t root_get_handler(httpd_req_t *req)
{
    extern const char rootpage_start[] asm("_binary_index_html_start");
    extern const char rootpage_end[]   asm("_binary_index_html_end");
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, rootpage_start, rootpage_end - rootpage_start);

    return ESP_OK;
}

static void http_server_restart_timer_callback(void *arg)
{
    ESP_LOGI(TAG, "Restarting device...");
    esp_restart();
}

static esp_err_t configure_mqtt_post_handler(httpd_req_t *req)
{
    char buff[250];
    int ret, remaining = req->content_len;
    if (remaining >= sizeof(buff)) {
        // Respond with 500 Internal Server Error
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Content too long");
        return ESP_FAIL;
    }

    while (remaining > 0) {
        ret = httpd_req_recv(req, buff, MIN(remaining, sizeof(buff)));
        if (ret <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                // Retry receiving if timeout occurred
                continue;
            }
            // Respond with 500 Internal Server Error
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive data");
            return ESP_FAIL;
        }
        remaining -= ret;
    }
    buff[req->content_len] = '\0';
    ESP_LOGI(TAG, "Received data: %s", buff);

    char *mqtt_server = strstr(buff, "mqtt-server=");
    char *mqtt_port = strstr(buff, "mqtt-port=");
    char *mqtt_client_id = strstr(buff, "mqtt-client-id=");
    char *mqtt_user = strstr(buff, "mqtt-username=");
    char *mqtt_password = strstr(buff, "mqtt-password=");
    char *mqtt_topic = strstr(buff, "mqtt-topic=");
    if(mqtt_server == NULL || mqtt_port == NULL || mqtt_client_id == NULL || mqtt_user == NULL || mqtt_password == NULL || mqtt_topic == NULL){
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid request");
        return ESP_FAIL;
    }

    mqtt_server += strlen("mqtt-server=");
    mqtt_port += strlen("mqtt-port=");
    mqtt_client_id += strlen("mqtt-client-id=");
    mqtt_user += strlen("mqtt-username=");
    mqtt_password += strlen("mqtt-password=");
    mqtt_topic += strlen("mqtt-topic=");

    char *mqtt_server_end = strstr(mqtt_server, "&");
    char *mqtt_port_end = strstr(mqtt_port, "&");
    char *mqtt_client_id_end = strstr(mqtt_client_id, "&");
    char *mqtt_user_end = strstr(mqtt_user, "&");
    char *mqtt_password_end = strstr(mqtt_password, "&");

    if (mqtt_server_end == NULL || mqtt_port_end == NULL || mqtt_client_id_end == NULL || mqtt_user_end == NULL || mqtt_password_end == NULL){
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid request");
        return ESP_FAIL;
    }

    *mqtt_server_end = '\0';
    *mqtt_port_end = '\0';
    *mqtt_client_id_end = '\0';
    *mqtt_user_end = '\0';
    *mqtt_password_end = '\0';

    // Validate MQTT topic: reject empty or containing invalid characters (+ and #)
    if (strlen(mqtt_topic) == 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "MQTT topic cannot be empty");
        return ESP_FAIL;
    }
    if (strchr(mqtt_topic, '+') != NULL || strchr(mqtt_topic, '#') != NULL) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "MQTT topic cannot contain wildcard characters (+ or #)");
        return ESP_FAIL;
    }

    ESP_ERROR_CHECK(internal_storage_save_mqtt_broker(mqtt_server));
    ESP_ERROR_CHECK(internal_storage_save_mqtt_port(atoi(mqtt_port)));
    ESP_ERROR_CHECK(internal_storage_save_mqtt_client_id(mqtt_client_id));
    ESP_ERROR_CHECK(internal_storage_save_mqtt_username(mqtt_user));
    ESP_ERROR_CHECK(internal_storage_save_mqtt_password(mqtt_password));
    ESP_ERROR_CHECK(internal_storage_save_mqtt_topic(mqtt_topic));

    mqtt_connection_start_task();
    ESP_LOGI(TAG, "MQTT connection started successfully");

    return ESP_OK;
}

static esp_err_t configure_wifi_post_handler(httpd_req_t *req)
{
    char buf[100];
    int ret, remaining = req->content_len;
    if (remaining >= sizeof(buf)) {
        // Respond with 500 Internal Server Error
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Content too long");
        return ESP_FAIL;
    }
    while (remaining > 0) {
        ret = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)));
        if (ret <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                // Retry receiving if timeout occurred
                continue;
            }
            // Respond with 500 Internal Server Error
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive data");
            return ESP_FAIL;
        }
        remaining -= ret;
    }
    buf[req->content_len] = '\0';
    ESP_LOGI(TAG, "Received data: %s", buf);

    char *ssid = strstr(buf, "ssid=");
    char *password = strstr(buf, "password=");
    if(ssid == NULL || password == NULL){
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid request");
        return ESP_FAIL;
    }

    ssid += strlen("ssid=");
    password += strlen("password=");

    char *ssid_end = strstr(ssid, "&");

    if(ssid_end == NULL){
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid request");
        return ESP_FAIL;
    }

    *ssid_end = '\0';

    ESP_LOGI(TAG, "SSID: %s, Password: %s", ssid, password);

    ESP_ERROR_CHECK(internal_storage_save_wifi_credentials(ssid, password));
    if(wifi_connect_sta(true)){
        ESP_LOGI(TAG, "WiFi connection succeeded");
        if(esp_timer_start_once(restart_timer, CONFIG_HOMEPOST_RESTART_DELAY_MICROSECONDS) == ESP_OK){
            ESP_LOGI(TAG, "Device will restart in a few seconds...");
            httpd_resp_set_status(req, "200 OK");
            httpd_resp_sendstr(req, "Device will restart in a few seconds...");
        }
        else{
            ESP_LOGE(TAG, "Failed to restart device");
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to restart device");
        }
    }
    else{
        ESP_LOGE(TAG, "WiFi connection failed");
        httpd_resp_set_status(req, "303 See Other");
        httpd_resp_set_hdr(req, "Location", "/");
        httpd_resp_sendstr(req, "WiFi connection failed. Redirecting to home page...");
    }

    return ESP_OK;
}

static const httpd_uri_t root = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = root_get_handler
};

static const httpd_uri_t configure_wifi = {
    .uri       = "/wifi-setup",
    .method    = HTTP_POST,
    .handler   = configure_wifi_post_handler
};

static const httpd_uri_t configure_mqtt = {
    .uri       = "/mqtt-setup",
    .method    = HTTP_POST,
    .handler   = configure_mqtt_post_handler
};

#if CONFIG_HOMEPOST_OTA_ENABLED
static esp_err_t check_update_get_handler(httpd_req_t *req)
{
    char response[256];
    
    // Trigger actual GitHub releases check
    esp_err_t ret = ota_update_check_for_update();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to check for updates: %s", esp_err_to_name(ret));
    }

    const char *current = ota_update_get_current_version();
    const char *available = ota_update_get_available_version();
    bool update_available = ota_update_is_available();

    snprintf(response, sizeof(response),
        "{\"current_version\":\"%s\",\"available_version\":\"%s\",\"update_available\":%s}",
        current ? current : "",
        available ? available : "",
        update_available ? "true" : "false");

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, response);
    return ESP_OK;
}

static esp_err_t trigger_update_post_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Manual OTA update triggered via HTTP");
    
    if (ota_update_check_now() == ESP_OK) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"status\":\"ok\",\"message\":\"Update check triggered\"}");
    } else {
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"status\":\"error\",\"message\":\"Failed to trigger update check\"}");
    }
    return ESP_OK;
}

static const httpd_uri_t check_update = {
    .uri       = "/check-update",
    .method    = HTTP_GET,
    .handler   = check_update_get_handler
};

static const httpd_uri_t trigger_update = {
    .uri       = "/trigger-update",
    .method    = HTTP_POST,
    .handler   = trigger_update_post_handler
};
#endif

static httpd_handle_t start_webserver(void)
{
    httpd_handle_t http_server = NULL;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server");

#ifdef CONFIG_LIGHTWATCHER_HTTP_SERVER_IS_SECURE
    httpd_ssl_config_t conf = HTTPD_SSL_CONFIG_DEFAULT();

    extern const unsigned char servercert_start[] asm("_binary_servercert_pem_start");
    extern const unsigned char servercert_end[]   asm("_binary_servercert_pem_end");
    conf.servercert = servercert_start;
    conf.servercert_len = servercert_end - servercert_start;

    extern const unsigned char prvtkey_pem_start[] asm("_binary_prvtkey_pem_start");
    extern const unsigned char prvtkey_pem_end[]   asm("_binary_prvtkey_pem_end");
    conf.prvtkey_pem = prvtkey_pem_start;
    conf.prvtkey_len = prvtkey_pem_end - prvtkey_pem_start;

    esp_err_t ret = httpd_ssl_start(&http_server, &conf);
#else
    httpd_config_t conf = HTTPD_DEFAULT_CONFIG();
    esp_err_t ret = httpd_start(&http_server, &conf);
#endif
    if (ESP_OK != ret) {
        ESP_LOGI(TAG, "Error starting server!");
        return NULL;
    }

    // Set URI handlers
    ESP_LOGI(TAG, "Registering URI handlers");
    httpd_register_uri_handler(http_server, &root);
    httpd_register_uri_handler(http_server, &configure_wifi);
    httpd_register_uri_handler(http_server, &configure_mqtt);
#if CONFIG_HOMEPOST_OTA_ENABLED
    httpd_register_uri_handler(http_server, &check_update);
    httpd_register_uri_handler(http_server, &trigger_update);
#endif
    return http_server;
}

static void server_event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data)
{
#ifdef CONFIG_LIGHTWATCHER_HTTP_SERVER_IS_SECURE
    if (event_base == ESP_HTTPS_SERVER_EVENT) {
        if (event_id == HTTPS_SERVER_EVENT_ERROR) {
            esp_https_server_last_error_t *last_error = (esp_tls_last_error_t *) event_data;
            ESP_LOGE(TAG, "Error event triggered: last_error = %s, last_tls_err = %d, tls_flag = %d", esp_err_to_name(last_error->last_error), last_error->esp_tls_error_code, last_error->esp_tls_flags);
        }
    }
#else
    if (event_base == ESP_HTTP_SERVER_EVENT) {
        if (event_id == HTTP_SERVER_EVENT_ERROR) {
            ESP_LOGE(TAG, "Error event triggered");
        }
    }
#endif
}

void http_server_init(void){
#ifdef CONFIG_LIGHTWATCHER_HTTP_SERVER_IS_SECURE
    ESP_ERROR_CHECK(esp_event_handler_register(ESP_HTTPS_SERVER_EVENT, ESP_EVENT_ANY_ID, &server_event_handler, NULL));
#else
    ESP_ERROR_CHECK(esp_event_handler_register(ESP_HTTP_SERVER_EVENT, ESP_EVENT_ANY_ID, &server_event_handler, NULL));
#endif

    ESP_ERROR_CHECK(esp_timer_create(&restart_timer_args, &restart_timer));
}

void http_server_start(void){
    server = start_webserver();
}