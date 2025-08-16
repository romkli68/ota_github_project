#include "rk_ota.h"
#include "esp_log.h"
#include "esp_https_ota.h"
#include "esp_http_client.h"
#include "esp_app_format.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_crt_bundle.h"
#include "freertos/timers.h"
#include <string.h>

static const char *TAG = "RK_OTA";

// ===== KONFIGURACJA GITHUB TOKEN =====
// ZMIEŃ NA SWÓJ PERSONAL ACCESS TOKEN!
#define GITHUB_TOKEN "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"
// Jak utworzyć token:
// 1. Idź do: https://github.com/settings/tokens
// 2. Kliknij "Generate new token (classic)"
// 3. Zaznacz scope: "repo" (pełny dostęp do prywatnych repo)
// 4. Skopiuj token i wklej powyżej
// =====================================

// Zmienne globalne
static QueueHandle_t ota_queue = NULL;
static TaskHandle_t ota_task_handle = NULL;
static EventGroupHandle_t wifi_event_group = NULL;
static rk_ota_event_callback_t event_callback = NULL;
static bool task_running = false;

// WiFi event bits (importowane z rk_wifi)
#define RK_WIFI_CONNECTED_BIT BIT0

static esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
    case HTTP_EVENT_ERROR:
        ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        if (evt->data_len > 0) {
            ESP_LOGI(TAG, "Pobieranie firmware: %d bajtów", evt->data_len);
        }
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
        break;
    case HTTP_EVENT_REDIRECT:
        ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
        break;
    }
    return ESP_OK;
}

static void ota_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Zadanie OTA uruchomione");
    
    rk_ota_message_t msg;
    
    while(task_running) {
        // Sprawdź czy są wiadomości w kolejce
        if(xQueueReceive(ota_queue, &msg, pdMS_TO_TICKS(1000)) == pdTRUE) {
            ESP_LOGI(TAG, "Otrzymano wiadomość OTA typu: %d", msg.type);
            
            switch(msg.type) {
                case RK_OTA_MSG_CHECK_UPDATE:
                case RK_OTA_MSG_FORCE_UPDATE:
                    // Sprawdź czy WiFi jest połączone
                    if (wifi_event_group != NULL) {
                        EventBits_t bits = xEventGroupGetBits(wifi_event_group);
                        if (!(bits & RK_WIFI_CONNECTED_BIT)) {
                            ESP_LOGW(TAG, "WiFi nie jest połączone, pomijam OTA");
                            break;
                        }
                    }
                    
                    ESP_LOGI(TAG, "Rozpoczynanie sprawdzania OTA...");
                    
                    // Powiadom callback o rozpoczęciu OTA
                    if (event_callback) {
                        event_callback(true, false);
                    }
                    
                    esp_err_t ret = rk_ota_check_update(&msg.config);
                    
                    // Powiadom callback o wyniku
                    if (event_callback) {
                        event_callback(false, ret == ESP_OK);
                    }
                    
                    if (ret == ESP_OK) {
                        ESP_LOGI(TAG, "OTA zakończone pomyślnie - restart nastąpi automatycznie");
                        // Restart nastąpi w funkcji rk_ota_check_update
                    } else {
                        ESP_LOGE(TAG, "OTA nie powiodło się");
                    }
                    break;
                    
                case RK_OTA_MSG_STOP:
                    ESP_LOGI(TAG, "Zatrzymanie zadania OTA");
                    task_running = false;
                    break;
                    
                default:
                    ESP_LOGW(TAG, "Nieznany typ wiadomości OTA: %d", msg.type);
                    break;
            }
        }
        
        // Automatyczne sprawdzanie OTA co 5 minut (jeśli WiFi połączone)
        static uint32_t last_check = 0;
        uint32_t current_time = xTaskGetTickCount() / configTICK_RATE_HZ;
        
        if (current_time - last_check > 300) { // 5 minut
            last_check = current_time;
            
            if (wifi_event_group != NULL) {
                EventBits_t bits = xEventGroupGetBits(wifi_event_group);
                if (bits & RK_WIFI_CONNECTED_BIT) {
                    ESP_LOGI(TAG, "Automatyczne sprawdzanie OTA...");
                    // Można tutaj dodać automatyczne sprawdzanie
                }
            }
        }
    }
    
    ESP_LOGI(TAG, "Zadanie OTA zakończone");
    vTaskDelete(NULL);
}

esp_err_t rk_ota_init(void)
{
    ESP_LOGI(TAG, "Inicjalizacja komponentu OTA");
    
    // Sprawdź czy token jest ustawiony
    if (strlen(GITHUB_TOKEN) == 0 || strcmp(GITHUB_TOKEN, "ghp_TWÓJ_TOKEN_TUTAJ") == 0) {
        ESP_LOGW(TAG, "UWAGA: GitHub token nie jest ustawiony!");
        ESP_LOGW(TAG, "Dla prywatnych repo ustaw GITHUB_TOKEN w rk_ota.c");
        ESP_LOGW(TAG, "Lub zmień repo na publiczne");
    } else {
        ESP_LOGI(TAG, "GitHub token skonfigurowany (długość: %d)", strlen(GITHUB_TOKEN));
    }
    
    return ESP_OK;
}

esp_err_t rk_ota_start_task(EventGroupHandle_t wifi_event_group_handle, rk_ota_event_callback_t callback)
{
    if (task_running) {
        ESP_LOGW(TAG, "Zadanie OTA już działa");
        return ESP_OK;
    }
    
    wifi_event_group = wifi_event_group_handle;
    event_callback = callback;
    
    ota_queue = xQueueCreate(5, sizeof(rk_ota_message_t));
    if (ota_queue == NULL) {
        ESP_LOGE(TAG, "Nie można utworzyć kolejki OTA");
        return ESP_ERR_NO_MEM;
    }
    
    task_running = true;
    
    BaseType_t ret = xTaskCreate(ota_task, 
                                "ota_task", 
                                8192,  // 8KB stosu dla OTA
                                NULL, 
                                2,     // Niski priorytet
                                &ota_task_handle);
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Nie można utworzyć zadania OTA");
        vQueueDelete(ota_queue);
        ota_queue = NULL;
        task_running = false;
        return ESP_ERR_NO_MEM;
    }
    
    ESP_LOGI(TAG, "Zadanie OTA uruchomione");
    return ESP_OK;
}

esp_err_t rk_ota_check_update(const rk_ota_config_t *config)
{
    ESP_LOGI(TAG, "Rozpoczynanie OTA z GitHub...");
    
    // Budowanie URL do firmware
    char firmware_url[512];
    
    // Sprawdź czy mamy token - jeśli tak, użyj go
    bool use_token = (strlen(GITHUB_TOKEN) > 0 && strcmp(GITHUB_TOKEN, "ghp_TWÓJ_TOKEN_TUTAJ") != 0);
    
    if (use_token) {
        // Dla prywatnych repo z tokenem
        snprintf(firmware_url, sizeof(firmware_url),
                 "https://raw.githubusercontent.com/%s/%s/%s/%s",
                 config->github_user,
                 config->github_repo,
                 config->github_branch,
                 config->firmware_file);
        ESP_LOGI(TAG, "Używam tokenu GitHub dla prywatnego repo");
    } else {
        // Dla publicznych repo bez tokenu
        snprintf(firmware_url, sizeof(firmware_url),
                 "https://github.com/%s/%s/raw/%s/%s",
                 config->github_user,
                 config->github_repo,
                 config->github_branch,
                 config->firmware_file);
        ESP_LOGI(TAG, "Używam publicznego dostępu (bez tokenu)");
    }
    
    ESP_LOGI(TAG, "URL firmware: %s", firmware_url);
    
    // Konfiguracja HTTP
    esp_http_client_config_t http_config = {
        .url = firmware_url,
        .event_handler = _http_event_handler,
        .keep_alive_enable = true,
        .timeout_ms = 30000,                    // 30 sekund timeout
        .skip_cert_common_name_check = true,    // Pomiń weryfikację nazwy
        .crt_bundle_attach = esp_crt_bundle_attach, // Użyj wbudowanych certyfikatów
    };
    
    // Utwórz klienta HTTP
    esp_http_client_handle_t client = esp_http_client_init(&http_config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Nie można utworzyć klienta HTTP");
        return ESP_ERR_NO_MEM;
    }
    
    // Dodaj token do nagłówka jeśli dostępny
    if (use_token) {
        char auth_header[256];
        snprintf(auth_header, sizeof(auth_header), "token %s", GITHUB_TOKEN);
        esp_http_client_set_header(client, "Authorization", auth_header);
        ESP_LOGI(TAG, "Dodano nagłówek Authorization");
    }
    
    // Dodaj User-Agent (GitHub tego wymaga)
    esp_http_client_set_header(client, "User-Agent", "ESP32-OTA-Client/1.0");
    
    esp_https_ota_config_t ota_config = {
        .http_config = &http_config,
        .http_client_init_cb = NULL,
        .bulk_flash_erase = false,
        .partial_http_download = false,
    };
    
    ESP_LOGI(TAG, "Próba aktualizacji OTA...");
    
    // Sprawdź dostępną przestrzeń OTA
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition == NULL) {
        ESP_LOGE(TAG, "Brak dostępnej partycji OTA");
        esp_http_client_cleanup(client);
        return ESP_ERR_NOT_FOUND;
    }
    
    ESP_LOGI(TAG, "Partycja OTA: %s, rozmiar: %lu bytes", 
             update_partition->label, update_partition->size);
    
    // Najpierw sprawdź czy plik istnieje
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Nie można otworzyć połączenia HTTP: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }
    
    int content_length = esp_http_client_fetch_headers(client);
    int status_code = esp_http_client_get_status_code(client);
    
    ESP_LOGI(TAG, "Status HTTP: %d, Content-Length: %d", status_code, content_length);
    
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    
    if (status_code == 404) {
        ESP_LOGE(TAG, "Plik firmware.bin nie został znaleziony (404)");
        ESP_LOGE(TAG, "Sprawdź czy plik istnieje w repo: %s", firmware_url);
        return ESP_ERR_NOT_FOUND;
    } else if (status_code == 401 || status_code == 403) {
        ESP_LOGE(TAG, "Brak autoryzacji (%d) - sprawdź token GitHub", status_code);
        return ESP_ERR_INVALID_ARG;
    } else if (status_code != 200) {
        ESP_LOGE(TAG, "Nieoczekiwany kod HTTP: %d", status_code);
        return ESP_FAIL;
    }
    
    if (content_length <= 0) {
        ESP_LOGE(TAG, "Nieprawidłowy rozmiar pliku: %d", content_length);
        return ESP_ERR_INVALID_SIZE;
    }
    
    ESP_LOGI(TAG, "Plik firmware znaleziony, rozmiar: %d bajtów", content_length);
    
    // Teraz wykonaj właściwe OTA
    esp_err_t ret = esp_https_ota(&ota_config);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "OTA zakończone pomyślnie! Restart za 3 sekundy...");
        vTaskDelay(pdMS_TO_TICKS(3000));
        esp_restart();
    } else {
        ESP_LOGE(TAG, "OTA nie powiodło się: %s (0x%x)", esp_err_to_name(ret), ret);
        
        // Uproszczona obsługa błędów
        switch(ret) {
            case ESP_ERR_INVALID_ARG:
                ESP_LOGE(TAG, "Nieprawidłowe argumenty - sprawdź konfigurację HTTP");
                break;
            case ESP_ERR_OTA_VALIDATE_FAILED:
                ESP_LOGE(TAG, "Walidacja firmware nie powiodła się");
                break;
            case ESP_ERR_NO_MEM:
                ESP_LOGE(TAG, "Brak pamięci");
                break;
            case ESP_ERR_INVALID_SIZE:
                ESP_LOGE(TAG, "Nieprawidłowy rozmiar firmware");
                break;
            case ESP_ERR_TIMEOUT:
                ESP_LOGE(TAG, "Timeout połączenia");
                break;
            case ESP_FAIL:
                ESP_LOGE(TAG, "Ogólny błąd OTA");
                break;
            default:
                ESP_LOGE(TAG, "Nieznany błąd OTA: 0x%x", ret);
                break;
        }
    }
    
    return ret;
}

esp_err_t rk_ota_send_message(const rk_ota_message_t *msg)
{
    if (ota_queue == NULL) {
        ESP_LOGE(TAG, "Kolejka OTA nie została utworzona");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xQueueSend(ota_queue, msg, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGW(TAG, "Nie można wysłać wiadomości do kolejki OTA");
        return ESP_ERR_TIMEOUT;
    }
    
    return ESP_OK;
}

const char* rk_ota_get_version(void)
{
    const esp_app_desc_t *app_desc = esp_app_get_description();
    return app_desc->version;
}

void rk_ota_stop_task(void)
{
    if (task_running && ota_queue != NULL) {
        rk_ota_message_t msg = {.type = RK_OTA_MSG_STOP};
        rk_ota_send_message(&msg);
        
        vTaskDelay(pdMS_TO_TICKS(500));
        
        if (ota_queue != NULL) {
            vQueueDelete(ota_queue);
            ota_queue = NULL;
        }
        
        task_running = false;
        ota_task_handle = NULL;
    }
}
