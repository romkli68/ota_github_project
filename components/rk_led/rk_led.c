#include "rk_led.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_log.h"

static const char *TAG = "RK_LED";
static TimerHandle_t blink_timer_on = NULL;
static TimerHandle_t blink_timer_off = NULL;
static bool led_state = false;
static bool asymmetric_mode = false;
static uint32_t on_time = 0;
static uint32_t off_time = 0;

static void blink_timer_callback(TimerHandle_t xTimer)
{
    rk_led_toggle();
}

static void asymmetric_on_callback(TimerHandle_t xTimer)
{
    rk_led_on();
    // Uruchom timer wyłączenia
    if (blink_timer_off != NULL) {
        xTimerStart(blink_timer_off, 0);
    }
}

static void asymmetric_off_callback(TimerHandle_t xTimer)
{
    rk_led_off();
    // Uruchom timer włączenia
    if (blink_timer_on != NULL) {
        xTimerStart(blink_timer_on, 0);
    }
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
    // Zatrzymaj wszystkie timery
    rk_led_blink_stop();
    
    asymmetric_mode = false;
    
    blink_timer_on = xTimerCreate("blink_timer",
                                  pdMS_TO_TICKS(period_ms),
                                  pdTRUE,
                                  NULL,
                                  blink_timer_callback);
    
    if (blink_timer_on != NULL) {
        xTimerStart(blink_timer_on, 0);
        ESP_LOGI(TAG, "Symetryczne mruganie LED rozpoczęte, okres: %lu ms", period_ms);
    }
}

void rk_led_blink_asymmetric_start(uint32_t on_time_ms, uint32_t off_time_ms)
{
    // Zatrzymaj wszystkie timery
    rk_led_blink_stop();
    
    asymmetric_mode = true;
    on_time = on_time_ms;
    off_time = off_time_ms;
    
    // Timer dla włączenia LED
    blink_timer_on = xTimerCreate("blink_on_timer",
                                  pdMS_TO_TICKS(on_time_ms + off_time_ms),
                                  pdTRUE,
                                  NULL,
                                  asymmetric_on_callback);
    
    // Timer dla wyłączenia LED
    blink_timer_off = xTimerCreate("blink_off_timer",
                                   pdMS_TO_TICKS(on_time_ms),
                                   pdFALSE,
                                   NULL,
                                   asymmetric_off_callback);
    
    if (blink_timer_on != NULL && blink_timer_off != NULL) {
        // Rozpocznij od włączenia LED
        rk_led_on();
        xTimerStart(blink_timer_off, 0);  // Zaplanuj wyłączenie
        xTimerStart(blink_timer_on, 0);   // Zaplanuj cykl
        
        ESP_LOGI(TAG, "Asymetryczne mruganie LED rozpoczęte: ON=%lu ms, OFF=%lu ms", 
                 on_time_ms, off_time_ms);
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
    
    asymmetric_mode = false;
    ESP_LOGI(TAG, "Mruganie LED zatrzymane");
}
