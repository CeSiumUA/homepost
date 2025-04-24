#include "http_server.h"

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
    .handler   = NULL // Not implemented yet
};

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