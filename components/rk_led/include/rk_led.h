#ifndef RK_LED_H
#define RK_LED_H

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

// Definicje pinów LED dla różnych modeli ESP32
#ifdef CONFIG_IDF_TARGET_ESP32
    #define LED_GPIO GPIO_NUM_2
#elif CONFIG_IDF_TARGET_ESP32S2
    #define LED_GPIO GPIO_NUM_2
#elif CONFIG_IDF_TARGET_ESP32S3
    #define LED_GPIO GPIO_NUM_48
#elif CONFIG_IDF_TARGET_ESP32C3
    #define LED_GPIO GPIO_NUM_8
#elif CONFIG_IDF_TARGET_ESP32C6
    #define LED_GPIO GPIO_NUM_8
#elif CONFIG_IDF_TARGET_ESP32H2
    #define LED_GPIO GPIO_NUM_8
#else
    #define LED_GPIO GPIO_NUM_2
#endif

// Typy wiadomości LED
typedef enum {
    RK_LED_MSG_STARTUP,
    RK_LED_MSG_WIFI_CONNECTING,
    RK_LED_MSG_WIFI_CONNECTED,
    RK_LED_MSG_WIFI_DISCONNECTED,
    RK_LED_MSG_OTA_START,
    RK_LED_MSG_OTA_SUCCESS,
    RK_LED_MSG_OTA_FAILED,
    RK_LED_MSG_CUSTOM_PATTERN,
    RK_LED_MSG_STOP
} rk_led_message_type_t;

typedef struct {
    rk_led_message_type_t type;
    uint32_t on_time_ms;
    uint32_t off_time_ms;
} rk_led_message_t;

/**
 * @brief Inicjalizacja komponentu LED
 * @return ESP_OK w przypadku sukcesu
 */
esp_err_t rk_led_init(void);

/**
 * @brief Uruchomienie zadania LED
 * @return ESP_OK w przypadku sukcesu
 */
esp_err_t rk_led_start_task(void);

/**
 * @brief Wysłanie wiadomości do zadania LED
 * @param msg Wiadomość do wysłania
 * @return ESP_OK w przypadku sukcesu
 */
esp_err_t rk_led_send_message(const rk_led_message_t *msg);

/**
 * @brief Zatrzymanie zadania LED
 */
void rk_led_stop_task(void);

// Podstawowe funkcje LED (dla kompatybilności)
void rk_led_on(void);
void rk_led_off(void);
void rk_led_toggle(void);
void rk_led_blink_start(uint32_t period_ms);
void rk_led_blink_asymmetric_start(uint32_t on_time_ms, uint32_t off_time_ms);
void rk_led_blink_stop(void);

#ifdef __cplusplus
}
#endif

#endif // RK_LED_H
