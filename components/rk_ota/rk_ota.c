#include "rk_ota.h"
#include "esp_log.h"
#include "esp_https_ota.h"
#include "esp_http_client.h"
#include "esp_app_format.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include <string.h>

static const char *TAG = "RK_OTA";

// Certyfikat GitHub (uproszczony - w produkcji użyj pełnego)
extern const uint8_t server_cert_pem_start[] asm("_binary_github_cert_pem_start");
extern const uint8_t server_cert_pem_end[]   asm("_binary_github_cert_pem_end");

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
    case HTTP_EVENT_ERROR:
        ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
        break;
    case HTTP_EVENT_REDIRECT:
        ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
        break;
    }
    return ESP_OK;
}

esp_err_t rk_ota_check_update(const rk_ota_config_t *config)
{
    ESP_LOGI(TAG, "Rozpoczynanie OTA z GitHub...");
    
    // Budowanie URL do firmware
    char firmware_url[512];
    snprintf(firmware_url, sizeof(firmware_url),
             "https://github.com/%s/%s/raw/%s/%s",
             config->github_user,
             config->github_repo,
             config->github_branch,
             config->firmware_file);
    
    ESP_LOGI(TAG, "URL firmware: %s", firmware_url);
    
    esp_http_client_config_t http_config = {
        .url = firmware_url,
        .event_handler = _http_event_handler,
        .keep_alive_enable = true,
        // .cert_pem = (char *)server_cert_pem_start, // Odkomentuj dla weryfikacji SSL
    };
    
    esp_https_ota_config_t ota_config = {
        .http_config = &http_config,
    };
    
    ESP_LOGI(TAG, "Próba aktualizacji OTA...");
    esp_err_t ret = esp_https_ota(&ota_config);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "OTA zakończone pomyślnie! Restart...");
        esp_restart();
    } else {
        ESP_LOGE(TAG, "OTA nie powiodło się: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

const char* rk_ota_get_version(void)
{
    const esp_app_desc_t *app_desc = esp_app_get_description();
    return app_desc->version;
}
