#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"  // <-- DODANE
#include "nvs_flash.h"

#include "rk_wifi.h"
#include "rk_led.h"
#include "rk_ota.h"

#include "config.h"

static const char *TAG = "MAIN";

// Konfiguracja WiFi - zmień na swoje dane
#ifdef HOME
#define WIFI_SSID       "vodafoneBD2484"
#define WIFI_PASS       "Drogocenna10"
#endif
#ifdef PLOT297
#define WIFI_SSID       "Plot297"
#define WIFI_PASS       "Drogocenna10"
#endif
#ifdef PLOT301
#define WIFI_SSID       "Plot301"
#define WIFI_PASS       "Drogocenna10"
#endif
// Konfiguracja GitHub OTA - zmień na swoje repozytorium
#define GITHUB_USER     "romkli68"
#define GITHUB_REPO     "ota_github_project"
#define GITHUB_FILE     "firmware.bin"
#define GITHUB_BRANCH   "main"

// Parametry mrugania LED - zmień te wartości dla testowania OTA!
#define LED_ON_TIME_MS  500   // Czas świecenia - ZMIEŃ TO!
#define LED_OFF_TIME_MS 500   // Czas wyłączenia - ZMIEŃ TO!

// Callback dla zdarzeń WiFi
void wifi_event_callback(bool connected)
{
    rk_led_message_t led_msg;
    
    if (connected) {
        ESP_LOGI(TAG, "WiFi połączone - ustawiam asymetryczne mruganie LED");
        led_msg.type = RK_LED_MSG_WIFI_CONNECTED;
        led_msg.on_time_ms = LED_ON_TIME_MS;
        led_msg.off_time_ms = LED_OFF_TIME_MS;
    } else {
        ESP_LOGI(TAG, "WiFi rozłączone - szybkie mruganie LED");
        led_msg.type = RK_LED_MSG_WIFI_DISCONNECTED;
        led_msg.on_time_ms = 0;
        led_msg.off_time_ms = 0;
    }
    
    rk_led_send_message(&led_msg);
}

// Callback dla zdarzeń OTA
void ota_event_callback(bool ota_started, bool ota_success)
{
    rk_led_message_t led_msg;
    
    if (ota_started) {
        ESP_LOGI(TAG, "OTA rozpoczęte - bardzo szybkie mruganie LED");
        led_msg.type = RK_LED_MSG_OTA_START;
    } else if (ota_success) {
        ESP_LOGI(TAG, "OTA zakończone pomyślnie - stałe świecenie LED");
        led_msg.type = RK_LED_MSG_OTA_SUCCESS;
    } else {
        ESP_LOGI(TAG, "OTA nie powiodło się - powrót do normalnego trybu");
        led_msg.type = RK_LED_MSG_OTA_FAILED;
        led_msg.on_time_ms = LED_ON_TIME_MS;
        led_msg.off_time_ms = LED_OFF_TIME_MS;
    }
    
    rk_led_send_message(&led_msg);
}

// Zadanie monitorowania systemu
void system_monitor_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Zadanie monitora systemu uruchomione");
    
    // Poczekaj na inicjalizację innych komponentów
    vTaskDelay(pdMS_TO_TICKS(5000));
    
    // Uruchom pierwsze sprawdzenie OTA po 10 sekundach
    rk_ota_message_t ota_msg = {
        .type = RK_OTA_MSG_CHECK_UPDATE
    };
    
    // Konfiguracja OTA
    strncpy(ota_msg.config.github_user, GITHUB_USER, sizeof(ota_msg.config.github_user) - 1);
    strncpy(ota_msg.config.github_repo, GITHUB_REPO, sizeof(ota_msg.config.github_repo) - 1);
    strncpy(ota_msg.config.github_branch, GITHUB_BRANCH, sizeof(ota_msg.config.github_branch) - 1);
    strncpy(ota_msg.config.firmware_file, GITHUB_FILE, sizeof(ota_msg.config.firmware_file) - 1);
    
    ESP_LOGI(TAG, "Pierwsze sprawdzenie OTA za 10 sekund...");
    vTaskDelay(pdMS_TO_TICKS(10000));
    
    rk_ota_send_message(&ota_msg);
    
    while(1) {
        // Informacje o systemie co minutę
        ESP_LOGI(TAG, "=== STATUS SYSTEMU ===");
        ESP_LOGI(TAG, "Wersja firmware: %s", rk_ota_get_version());
        ESP_LOGI(TAG, "Wolna pamięć: %lu bytes", esp_get_free_heap_size());
        ESP_LOGI(TAG, "WiFi: %s", rk_wifi_is_connected() ? "Połączone" : "Rozłączone");
        ESP_LOGI(TAG, "LED: ON=%dms, OFF=%dms", LED_ON_TIME_MS, LED_OFF_TIME_MS);
        ESP_LOGI(TAG, "Uptime: %llu sekund", esp_timer_get_time() / 1000000);
        ESP_LOGI(TAG, "Liczba zadań: %d", uxTaskGetNumberOfTasks());
        
        // Sprawdź OTA co 5 minut
        static int ota_counter = 0;
        ota_counter++;
        
        if (ota_counter >= 5) { // 5 minut (5 * 1 minuta)
            ota_counter = 0;
            if (rk_wifi_is_connected()) {
                ESP_LOGI(TAG, "Automatyczne sprawdzanie OTA...");
                rk_ota_send_message(&ota_msg);
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(60000)); // Co minutę
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== URUCHAMIANIE APLIKACJI OTA GITHUB ===");
    ESP_LOGI(TAG, "Wersja firmware: %s", rk_ota_get_version());
    ESP_LOGI(TAG, "Parametry LED: ON=%d ms, OFF=%d ms", LED_ON_TIME_MS, LED_OFF_TIME_MS);
    
    // Sprawdź czy dane WiFi są ustawione
    if (strlen(WIFI_SSID) == 0 || strcmp(WIFI_SSID, "TwojeWiFi") == 0) {
        ESP_LOGE(TAG, "UWAGA: Nie ustawiono prawidłowych danych WiFi!");
        ESP_LOGE(TAG, "Zmień WIFI_SSID i WIFI_PASS w main.c");
    }
    
    // Inicjalizacja NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Inicjalizacja komponentów
    ESP_LOGI(TAG, "Inicjalizacja komponentów...");
    
    // 1. Inicjalizacja LED
    ESP_ERROR_CHECK(rk_led_init());
    ESP_ERROR_CHECK(rk_led_start_task());
    
    // 2. Inicjalizacja WiFi
    ESP_ERROR_CHECK(rk_wifi_init());
    ESP_ERROR_CHECK(rk_wifi_start_task(wifi_event_callback));
    
    // 3. Inicjalizacja OTA
    ESP_ERROR_CHECK(rk_ota_init());
    ESP_ERROR_CHECK(rk_ota_start_task(rk_wifi_get_event_group(), ota_event_callback));
    
    ESP_LOGI(TAG, "Wszystkie komponenty zainicjalizowane!");
    
    // Sygnalizacja startu LED
    rk_led_message_t led_msg = {.type = RK_LED_MSG_STARTUP};
    rk_led_send_message(&led_msg);
    
    // Połącz z WiFi
    ESP_LOGI(TAG, "Łączenie z WiFi: %s", WIFI_SSID);
    led_msg.type = RK_LED_MSG_WIFI_CONNECTING;
    rk_led_send_message(&led_msg);
    
    rk_wifi_connect(WIFI_SSID, WIFI_PASS);
    
    // Uruchom zadanie monitorowania systemu
    xTaskCreate(system_monitor_task, 
               "monitor_task", 
               4096,        // 4KB stosu
               NULL, 
               1,           // Najniższy priorytet
               NULL);
    
    ESP_LOGI(TAG, "Aplikacja uruchomiona - wszystkie zadania działają!");
    
    // Główne zadanie może się zakończyć - inne zadania będą działać
    vTaskDelete(NULL);
}