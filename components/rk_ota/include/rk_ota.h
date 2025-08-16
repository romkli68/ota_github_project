#ifndef RK_OTA_H
#define RK_OTA_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char github_user[64];
    char github_repo[64];
    char github_branch[32];
    char firmware_file[64];
} rk_ota_config_t;

/**
 * @brief Sprawdzenie i wykonanie aktualizacji OTA z GitHub
 * @param config Konfiguracja OTA
 * @return ESP_OK w przypadku sukcesu
 */
esp_err_t rk_ota_check_update(const rk_ota_config_t *config);

/**
 * @brief Pobranie wersji firmware z partycji
 * @return Wskaźnik do stringa z wersją
 */
const char* rk_ota_get_version(void);

#ifdef __cplusplus
}
#endif

#endif // RK_OTA_H
