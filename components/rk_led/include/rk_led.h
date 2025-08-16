#ifndef RK_LED_H
#define RK_LED_H

#include "driver/gpio.h"

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
    #define LED_GPIO GPIO_NUM_2  // Domyślnie GPIO2
#endif

/**
 * @brief Inicjalizacja LED
 * @return ESP_OK w przypadku sukcesu
 */
esp_err_t rk_led_init(void);

/**
 * @brief Włączenie LED
 */
void rk_led_on(void);

/**
 * @brief Wyłączenie LED
 */
void rk_led_off(void);

/**
 * @brief Przełączenie stanu LED
 */
void rk_led_toggle(void);

/**
 * @brief Rozpoczęcie mrugania LED
 * @param period_ms Okres mrugania w milisekundach
 */
void rk_led_blink_start(uint32_t period_ms);

/**
 * @brief Zatrzymanie mrugania LED
 */
void rk_led_blink_stop(void);

#ifdef __cplusplus
}
#endif

#endif // RK_LED_H
