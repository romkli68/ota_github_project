#ifndef RK_OTA_H
#define RK_OTA_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char github_user[64];
    char github_repo[64];
    char github_branch[32];
    char firmware_file[64];
} rk_ota_config_t;

// Typy wiadomości OTA
typedef enum {
    RK_OTA_MSG_CHECK_UPDATE,
    RK_OTA_MSG_FORCE_UPDATE,
    RK_OTA_MSG_STOP
} rk_ota_message_type_t;

typedef struct {
    rk_ota_message_type_t type;
    rk_ota_config_t config;
} rk_ota_message_t;

// Callback dla zdarzeń OTA
typedef void (*rk_ota_event_callback_t)(bool ota_started, bool ota_success);

/**
 * @brief Inicjalizacja komponentu OTA
 * @return ESP_OK w przypadku sukcesu
 */
esp_err_t rk_ota_init(void);

/**
 * @brief Uruchomienie zadania OTA
 * @param wifi_event_group Event Group WiFi do monitorowania połączenia
 * @param callback Funkcja callback dla zdarzeń OTA
 * @return ESP_OK w przypadku sukcesu
 */
esp_err_t rk_ota_start_task(EventGroupHandle_t wifi_event_group, rk_ota_event_callback_t callback);

/**
 * @brief Sprawdzenie i wykonanie aktualizacji OTA z GitHub
 * @param config Konfiguracja OTA
 * @return ESP_OK w przypadku sukcesu
 */
esp_err_t rk_ota_check_update(const rk_ota_config_t *config);

/**
 * @brief Wysłanie wiadomości do zadania OTA
 * @param msg Wiadomość do wysłania
 * @return ESP_OK w przypadku sukcesu
 */
esp_err_t rk_ota_send_message(const rk_ota_message_t *msg);

/**
 * @brief Pobranie wersji firmware
 * @return String z wersją firmware
 */
const char* rk_ota_get_version(void);

/**
 * @brief Zatrzymanie zadania OTA
 */
void rk_ota_stop_task(void);

#ifdef __cplusplus
}
#endif

#endif // RK_OTA_H
