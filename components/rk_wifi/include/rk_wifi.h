#ifndef RK_WIFI_H
#define RK_WIFI_H

#include "esp_err.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#ifdef __cplusplus
extern "C" {
#endif

// Event bits
#define RK_WIFI_CONNECTED_BIT BIT0
#define RK_WIFI_FAIL_BIT      BIT1

// Typy wiadomości WiFi
typedef enum {
    RK_WIFI_MSG_CONNECT,
    RK_WIFI_MSG_DISCONNECT,
    RK_WIFI_MSG_RECONNECT,
    RK_WIFI_MSG_STOP
} rk_wifi_message_type_t;

typedef struct {
    rk_wifi_message_type_t type;
    char ssid[32];
    char password[64];
} rk_wifi_message_t;

// Callback dla zdarzeń WiFi
typedef void (*rk_wifi_event_callback_t)(bool connected);

/**
 * @brief Inicjalizacja komponentu WiFi
 * @return ESP_OK w przypadku sukcesu
 */
esp_err_t rk_wifi_init(void);

/**
 * @brief Uruchomienie zadania WiFi
 * @param callback Funkcja callback dla zdarzeń WiFi
 * @return ESP_OK w przypadku sukcesu
 */
esp_err_t rk_wifi_start_task(rk_wifi_event_callback_t callback);

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

/**
 * @brief Pobranie Event Group handle
 * @return Handle do Event Group
 */
EventGroupHandle_t rk_wifi_get_event_group(void);

/**
 * @brief Zatrzymanie zadania WiFi
 */
void rk_wifi_stop_task(void);

#ifdef __cplusplus
}
#endif

#endif // RK_WIFI_H
