#include "rk_led.h"
#include "esp_log.h"
#include "freertos/timers.h"

static const char *TAG = "RK_LED";

// Zmienne globalne
static QueueHandle_t led_queue = NULL;
static TaskHandle_t led_task_handle = NULL;
static TimerHandle_t blink_timer_on = NULL;
static TimerHandle_t blink_timer_off = NULL;
static bool led_state = false;
static bool task_running = false;

// Funkcje pomocnicze
static void blink_timer_callback(TimerHandle_t xTimer);
static void asymmetric_on_callback(TimerHandle_t xTimer);
static void asymmetric_off_callback(TimerHandle_t xTimer);
static void led_task(void *pvParameters);

static void blink_timer_callback(TimerHandle_t xTimer)
{
    rk_led_toggle();
}

static void asymmetric_on_callback(TimerHandle_t xTimer)
{
    rk_led_on();
    if (blink_timer_off != NULL) {
        xTimerStart(blink_timer_off, 0);
    }
}

static void asymmetric_off_callback(TimerHandle_t xTimer)
{
    rk_led_off();
    if (blink_timer_on != NULL) {
        xTimerStart(blink_timer_on, 0);
    }
}

static void led_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Zadanie LED uruchomione");
    
    rk_led_message_t msg;
    
    // Sygnalizacja startu
    for(int i = 0; i < 3; i++) {
        rk_led_on();
        vTaskDelay(pdMS_TO_TICKS(200));
        rk_led_off();
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    
    while(task_running) {
        if(xQueueReceive(led_queue, &msg, pdMS_TO_TICKS(100)) == pdTRUE) {
            ESP_LOGI(TAG, "Otrzymano wiadomość typu: %d", msg.type);
            
            // Zatrzymaj poprzednie mruganie
            rk_led_blink_stop();
            
            switch(msg.type) {
                case RK_LED_MSG_STARTUP:
                    // Już wykonane na początku
                    break;
                    
                case RK_LED_MSG_WIFI_CONNECTING:
                    ESP_LOGI(TAG, "WiFi łączenie - symetryczne mruganie");
                    rk_led_blink_start(500);
                    break;
                    
                case RK_LED_MSG_WIFI_CONNECTED:
                    ESP_LOGI(TAG, "WiFi połączone - asymetryczne mruganie");
                    rk_led_blink_asymmetric_start(msg.on_time_ms, msg.off_time_ms);
                    break;
                    
                case RK_LED_MSG_WIFI_DISCONNECTED:
                    ESP_LOGI(TAG, "WiFi rozłączone - szybkie mruganie");
                    rk_led_blink_start(100);
                    break;
                    
                case RK_LED_MSG_OTA_START:
                    ESP_LOGI(TAG, "OTA rozpoczęte - bardzo szybkie mruganie");
                    rk_led_blink_start(50);
                    break;
                    
                case RK_LED_MSG_OTA_SUCCESS:
                    ESP_LOGI(TAG, "OTA sukces - stałe świecenie");
                    rk_led_on();
                    break;
                    
                case RK_LED_MSG_OTA_FAILED:
                    ESP_LOGI(TAG, "OTA błąd - powrót do normalnego trybu");
                    rk_led_blink_asymmetric_start(msg.on_time_ms, msg.off_time_ms);
                    break;
                    
                case RK_LED_MSG_CUSTOM_PATTERN:
                    ESP_LOGI(TAG, "Własny wzorzec: %lu/%lu ms", msg.on_time_ms, msg.off_time_ms);
                    rk_led_blink_asymmetric_start(msg.on_time_ms, msg.off_time_ms);
                    break;
                    
                case RK_LED_MSG_STOP:
                    ESP_LOGI(TAG, "Zatrzymanie LED");
                    rk_led_off();
                    task_running = false;
                    break;
                    
                default:
                    ESP_LOGW(TAG, "Nieznany typ wiadomości: %d", msg.type);
                    break;
            }
        }
    }
    
    ESP_LOGI(TAG, "Zadanie LED zakończone");
    vTaskDelete(NULL);
}

esp_err_t rk_led_init(void)
{
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << LED_GPIO),
        .pull_down_en = 0,
        .pull_up_en = 0,
    };
    
    esp_err_t ret = gpio_config(&io_conf);
    if (ret == ESP_OK) {
        rk_led_off();
        ESP_LOGI(TAG, "LED zainicjalizowany na GPIO %d", LED_GPIO);
    }
    
    return ret;
}

esp_err_t rk_led_start_task(void)
{
    if (task_running) {
        ESP_LOGW(TAG, "Zadanie LED już działa");
        return ESP_OK;
    }
    
    led_queue = xQueueCreate(10, sizeof(rk_led_message_t));
    if (led_queue == NULL) {
        ESP_LOGE(TAG, "Nie można utworzyć kolejki LED");
        return ESP_ERR_NO_MEM;
    }
    
    task_running = true;
    
    BaseType_t ret = xTaskCreate(led_task, 
                                "led_task", 
                                4096, 
                                NULL, 
                                3, 
                                &led_task_handle);
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Nie można utworzyć zadania LED");
        vQueueDelete(led_queue);
        led_queue = NULL;
        task_running = false;
        return ESP_ERR_NO_MEM;
    }
    
    ESP_LOGI(TAG, "Zadanie LED uruchomione");
    return ESP_OK;
}

esp_err_t rk_led_send_message(const rk_led_message_t *msg)
{
    if (led_queue == NULL) {
        ESP_LOGE(TAG, "Kolejka LED nie została utworzona");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xQueueSend(led_queue, msg, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "Nie można wysłać wiadomości do kolejki LED");
        return ESP_ERR_TIMEOUT;
    }
    
    return ESP_OK;
}

void rk_led_stop_task(void)
{
    if (task_running) {
        rk_led_message_t msg = {.type = RK_LED_MSG_STOP};
        rk_led_send_message(&msg);
        
        // Poczekaj na zakończenie zadania
        if (led_task_handle != NULL) {
            vTaskDelay(pdMS_TO_TICKS(500));
            led_task_handle = NULL;
        }
        
        if (led_queue != NULL) {
            vQueueDelete(led_queue);
            led_queue = NULL;
        }
        
        task_running = false;
    }
}

// Podstawowe funkcje LED
void rk_led_on(void)
{
    gpio_set_level(LED_GPIO, 1);
    led_state = true;
}

void rk_led_off(void)
{
    gpio_set_level(LED_GPIO, 0);
    led_state = false;
}

void rk_led_toggle(void)
{
    led_state = !led_state;
    gpio_set_level(LED_GPIO, led_state ? 1 : 0);
}

void rk_led_blink_start(uint32_t period_ms)
{
    rk_led_blink_stop();
    
    blink_timer_on = xTimerCreate("blink_timer",
                                  pdMS_TO_TICKS(period_ms),
                                  pdTRUE,
                                  NULL,
                                  blink_timer_callback);
    
    if (blink_timer_on != NULL) {
        xTimerStart(blink_timer_on, 0);
    }
}

void rk_led_blink_asymmetric_start(uint32_t on_time_ms, uint32_t off_time_ms)
{
    rk_led_blink_stop();
    
    blink_timer_on = xTimerCreate("blink_on_timer",
                                  pdMS_TO_TICKS(on_time_ms + off_time_ms),
                                  pdTRUE,
                                  NULL,
                                  asymmetric_on_callback);
    
    blink_timer_off = xTimerCreate("blink_off_timer",
                                   pdMS_TO_TICKS(on_time_ms),
                                   pdFALSE,
                                   NULL,
                                   asymmetric_off_callback);
    
    if (blink_timer_on != NULL && blink_timer_off != NULL) {
        rk_led_on();
        xTimerStart(blink_timer_off, 0);
        xTimerStart(blink_timer_on, 0);
    }
}

void rk_led_blink_stop(void)
{
    if (blink_timer_on != NULL) {
        xTimerStop(blink_timer_on, 0);
        xTimerDelete(blink_timer_on, 0);
        blink_timer_on = NULL;
    }
    
    if (blink_timer_off != NULL) {
        xTimerStop(blink_timer_off, 0);
        xTimerDelete(blink_timer_off, 0);
        blink_timer_off = NULL;
    }
}
