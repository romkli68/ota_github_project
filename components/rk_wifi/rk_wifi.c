#include "rk_wifi.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

static const char *TAG = "RK_WIFI";

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define WIFI_MAXIMUM_RETRY 5

static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;
static bool s_wifi_connected = false;
static bool s_wifi_initialized = false;

static void event_handler(void* arg, esp_event_base_t event_base,
                         int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WiFi STA uruchomione");
        // NIE wywołuj esp_wifi_connect() tutaj!
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < WIFI_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Ponowna próba połączenia z AP, próba %d", s_retry_num);
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        s_wifi_connected = false;
        ESP_LOGI(TAG, "Nie udało się połączyć z AP");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Otrzymano IP:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        s_wifi_connected = true;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

esp_err_t rk_wifi_init(void)
{
    if (s_wifi_initialized) {
        ESP_LOGW(TAG, "WiFi już zainicjalizowane");
        return ESP_OK;
    }

    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    
    s_wifi_initialized = true;
    ESP_LOGI(TAG, "WiFi zainicjalizowane");
    return ESP_OK;
}

esp_err_t rk_wifi_connect(const char* ssid, const char* password)
{
    if (!s_wifi_initialized) {
        ESP_LOGE(TAG, "WiFi nie zostało zainicjalizowane!");
        return ESP_ERR_INVALID_STATE;
    }

    // Zatrzymaj WiFi jeśli już działa
    esp_wifi_stop();
    vTaskDelay(pdMS_TO_TICKS(100)); // Krótka pauza

    wifi_config_t wifi_config = {0}; // Wyzeruj strukturę
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;
    
    // Bezpieczne kopiowanie stringów
    strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    wifi_config.sta.ssid[sizeof(wifi_config.sta.ssid) - 1] = '\0';
    
    strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
    wifi_config.sta.password[sizeof(wifi_config.sta.password) - 1] = '\0';

    // Wyczyść poprzednie bity eventów
    xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
    s_retry_num = 0;

    // Ustaw konfigurację PRZED uruchomieniem WiFi
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    
    // Uruchom WiFi
    ESP_ERROR_CHECK(esp_wifi_start());
    
    // Poczekaj chwilę i rozpocznij łączenie
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_ERROR_CHECK(esp_wifi_connect());

    ESP_LOGI(TAG, "Łączenie z AP SSID:%s", ssid);

    // Czekaj na rezultat
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            pdMS_TO_TICKS(20000)); // 20 sekund timeout

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Połączono z AP SSID:%s", ssid);
        return ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Nie udało się połączyć z AP SSID:%s", ssid);
        return ESP_FAIL;
    } else {
        ESP_LOGE(TAG, "Timeout połączenia z AP SSID:%s", ssid);
        return ESP_ERR_TIMEOUT;
    }
}

esp_err_t rk_wifi_disconnect(void)
{
    if (s_wifi_initialized) {
        return esp_wifi_disconnect();
    }
    return ESP_ERR_INVALID_STATE;
}

bool rk_wifi_is_connected(void)
{
    return s_wifi_connected;
}
