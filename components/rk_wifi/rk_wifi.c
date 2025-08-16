#include "rk_wifi.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include <string.h>

static const char *TAG = "RK_WIFI";

#define WIFI_MAXIMUM_RETRY 5

// Zmienne globalne
static EventGroupHandle_t s_wifi_event_group = NULL;
static QueueHandle_t wifi_queue = NULL;
static TaskHandle_t wifi_task_handle = NULL;
static int s_retry_num = 0;
static bool s_wifi_connected = false;
static bool s_wifi_initialized = false;
static bool task_running = false;
static rk_wifi_event_callback_t event_callback = NULL;

// Deklaracja funkcji zadania
static void wifi_task(void *pvParameters);

static void event_handler(void* arg, esp_event_base_t event_base,
                         int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WiFi STA uruchomione");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < WIFI_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Ponowna próba połączenia z AP, próba %d", s_retry_num);
        } else {
            xEventGroupSetBits(s_wifi_event_group, RK_WIFI_FAIL_BIT);
        }
        s_wifi_connected = false;
        if (event_callback) {
            event_callback(false);
        }
        ESP_LOGI(TAG, "Nie udało się połączyć z AP");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Otrzymano IP:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        s_wifi_connected = true;
        if (event_callback) {
            event_callback(true);
        }
        xEventGroupSetBits(s_wifi_event_group, RK_WIFI_CONNECTED_BIT);
    }
}

static void wifi_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Zadanie WiFi uruchomione");
    
    rk_wifi_message_t msg;
    
    while(task_running) {
        if(xQueueReceive(wifi_queue, &msg, pdMS_TO_TICKS(1000)) == pdTRUE) {
            ESP_LOGI(TAG, "Otrzymano wiadomość WiFi typu: %d", msg.type);
            
            switch(msg.type) {
                case RK_WIFI_MSG_CONNECT:
                    ESP_LOGI(TAG, "Łączenie z WiFi: %s", msg.ssid);
                    
                    esp_wifi_stop();
                    vTaskDelay(pdMS_TO_TICKS(100));
                    
                    wifi_config_t wifi_config = {0};
                    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
                    wifi_config.sta.pmf_cfg.capable = true;
                    wifi_config.sta.pmf_cfg.required = false;
                    
                    strncpy((char*)wifi_config.sta.ssid, msg.ssid, sizeof(wifi_config.sta.ssid) - 1);
                    strncpy((char*)wifi_config.sta.password, msg.password, sizeof(wifi_config.sta.password) - 1);
                    
                    xEventGroupClearBits(s_wifi_event_group, RK_WIFI_CONNECTED_BIT | RK_WIFI_FAIL_BIT);
                    s_retry_num = 0;
                    
                    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
                    ESP_ERROR_CHECK(esp_wifi_start());
                    
                    vTaskDelay(pdMS_TO_TICKS(100));
                    ESP_ERROR_CHECK(esp_wifi_connect());
                    break;
                    
                case RK_WIFI_MSG_DISCONNECT:
                    ESP_LOGI(TAG, "Rozłączanie WiFi");
                    esp_wifi_disconnect();
                    break;
                    
                case RK_WIFI_MSG_RECONNECT:
                    ESP_LOGI(TAG, "Ponowne łączenie WiFi");
                    esp_wifi_connect();
                    break;
                    
                case RK_WIFI_MSG_STOP:
                    ESP_LOGI(TAG, "Zatrzymanie zadania WiFi");
                    task_running = false;
                    break;
                    
                default:
                    ESP_LOGW(TAG, "Nieznany typ wiadomości WiFi: %d", msg.type);
                    break;
            }
        }
        
        // Monitoruj połączenie
        if (s_wifi_connected && !rk_wifi_is_connected()) {
            ESP_LOGW(TAG, "Utracono połączenie WiFi");
            s_wifi_connected = false;
            if (event_callback) {
                event_callback(false);
            }
        }
    }
    
    ESP_LOGI(TAG, "Zadanie WiFi zakończone");
    vTaskDelete(NULL);
}

esp_err_t rk_wifi_init(void)
{
    if (s_wifi_initialized) {
        ESP_LOGW(TAG, "WiFi już zainicjalizowane");
        return ESP_OK;
    }

    s_wifi_event_group = xEventGroupCreate();
    if (s_wifi_event_group == NULL) {
        ESP_LOGE(TAG, "Nie można utworzyć Event Group");
        return ESP_ERR_NO_MEM;
    }

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

esp_err_t rk_wifi_start_task(rk_wifi_event_callback_t callback)
{
    if (task_running) {
        ESP_LOGW(TAG, "Zadanie WiFi już działa");
        return ESP_OK;
    }
    
    event_callback = callback;
    
    wifi_queue = xQueueCreate(5, sizeof(rk_wifi_message_t));
    if (wifi_queue == NULL) {
        ESP_LOGE(TAG, "Nie można utworzyć kolejki WiFi");
        return ESP_ERR_NO_MEM;
    }
    
    task_running = true;
    
    BaseType_t ret = xTaskCreate(wifi_task, 
                                "wifi_task", 
                                6144, 
                                NULL, 
                                4, 
                                &wifi_task_handle);
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Nie można utworzyć zadania WiFi");
        vQueueDelete(wifi_queue);
        wifi_queue = NULL;
        task_running = false;
        return ESP_ERR_NO_MEM;
    }
    
    ESP_LOGI(TAG, "Zadanie WiFi uruchomione");
    return ESP_OK;
}

esp_err_t rk_wifi_connect(const char* ssid, const char* password)
{
    if (!s_wifi_initialized || wifi_queue == NULL) {
        ESP_LOGE(TAG, "WiFi nie zostało zainicjalizowane!");
        return ESP_ERR_INVALID_STATE;
    }
    
    rk_wifi_message_t msg = {
        .type = RK_WIFI_MSG_CONNECT
    };
    
    strncpy(msg.ssid, ssid, sizeof(msg.ssid) - 1);
    strncpy(msg.password, password, sizeof(msg.password) - 1);
    
    if (xQueueSend(wifi_queue, &msg, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "Nie można wysłać wiadomości połączenia");
        return ESP_ERR_TIMEOUT;
    }
    
    return ESP_OK;
}

esp_err_t rk_wifi_disconnect(void)
{
    if (wifi_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    rk_wifi_message_t msg = {.type = RK_WIFI_MSG_DISCONNECT};
    xQueueSend(wifi_queue, &msg, pdMS_TO_TICKS(100));
    
    return ESP_OK;
}

bool rk_wifi_is_connected(void)
{
    return s_wifi_connected;
}

EventGroupHandle_t rk_wifi_get_event_group(void)
{
    return s_wifi_event_group;
}

void rk_wifi_stop_task(void)
{
    if (task_running && wifi_queue != NULL) {
        rk_wifi_message_t msg = {.type = RK_WIFI_MSG_STOP};
        xQueueSend(wifi_queue, &msg, pdMS_TO_TICKS(100));
        
        vTaskDelay(pdMS_TO_TICKS(500));
        
        if (wifi_queue != NULL) {
            vQueueDelete(wifi_queue);
            wifi_queue = NULL;
        }
        
        task_running = false;
        wifi_task_handle = NULL;
    }
}
