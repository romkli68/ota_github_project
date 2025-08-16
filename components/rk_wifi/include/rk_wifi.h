#ifndef RK_WIFI_H
#define RK_WIFI_H

#include "esp_err.h"
#include "esp_wifi.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Inicjalizacja WiFi
 * @return ESP_OK w przypadku sukcesu
 */
esp_err_t rk_wifi_init(void);

/**
 * @brief Połączenie z siecią WiFi
 * @param ssid Nazwa sieci
 * @param password Hasło do sieci
 * @return ESP_OK w przypadku sukcesu
 */
esp_err_t rk_wifi_connect(const char* ssid, const char* password);

/**
 * @brief Rozłączenie z WiFi
 * @return ESP_OK w przypadku sukcesu
 */
esp_err_t rk_wifi_disconnect(void);

/**
 * @brief Sprawdzenie czy WiFi jest połączone
 * @return true jeśli połączone
 */
bool rk_wifi_is_connected(void);

#ifdef __cplusplus
}
#endif

#endif // RK_WIFI_H
