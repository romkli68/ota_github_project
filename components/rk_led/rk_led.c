#include "rk_led.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_log.h"

static const char *TAG = "RK_LED";
static TimerHandle_t blink_timer = NULL;
static bool led_state = false;

static void blink_timer_callback(TimerHandle_t xTimer)
{
    rk_led_toggle();
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
    if (blink_timer != NULL) {
        xTimerStop(blink_timer, 0);
        xTimerDelete(blink_timer, 0);
    }
    
    blink_timer = xTimerCreate("blink_timer",
                               pdMS_TO_TICKS(period_ms),
                               pdTRUE,
                               NULL,
                               blink_timer_callback);
    
    if (blink_timer != NULL) {
        xTimerStart(blink_timer, 0);
        ESP_LOGI(TAG, "Mruganie LED rozpoczęte, okres: %lu ms", period_ms);
    }
}

void rk_led_blink_stop(void)
{
    if (blink_timer != NULL) {
        xTimerStop(blink_timer, 0);
        xTimerDelete(blink_timer, 0);
        blink_timer = NULL;
        ESP_LOGI(TAG, "Mruganie LED zatrzymane");
    }
}
