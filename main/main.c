#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "rk_wifi.h"
#include "rk_led.h"
#include "rk_ota.h"

static const char *TAG = "MAIN";

// Konfiguracja WiFi - zmień na swoje dane
#define WIFI_SSID       "vodafoneBD2484"
#define WIFI_PASS       "Drogocenna10"

// Konfiguracja GitHub OTA - zmień na swoje repozytorium
#define GITHUB_USER     "romkli68"
#define GITHUB_REPO     "ota_github_project"
#define GITHUB_FILE     "firmware.bin"
#define GITHUB_BRANCH   "main"

// Parametry mrugania LED - zmień te wartości dla testowania OTA!
#define LED_ON_TIME_MS  900   // Czas świecenia - ZMIEŃ TO!
#define LED_OFF_TIME_MS 100   // Czas wyłączenia - ZMIEŃ TO!

void app_main(void)
{
    ESP_LOGI(TAG, "Uruchamianie aplikacji OTA GitHub");
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
    
    // Inicjalizacja LED
    rk_led_init();
    
    // Sygnalizacja startu - 3 szybkie mrugnięcia
    for(int i = 0; i < 3; i++) {
        rk_led_on();
        vTaskDelay(pdMS_TO_TICKS(200));
        rk_led_off();
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    
    // Inicjalizacja WiFi
    rk_wifi_init();
    
    // Połączenie z WiFi
    ESP_LOGI(TAG, "Łączenie z WiFi...");
    rk_led_blink_start(500); // Symetryczne mruganie podczas łączenia
    
    if (rk_wifi_connect(WIFI_SSID, WIFI_PASS) == ESP_OK) {
        ESP_LOGI(TAG, "Połączono z WiFi");
        rk_led_blink_stop();
        
        // ASYMETRYCZNE MRUGANIE PO POŁĄCZENIU Z WIFI
        rk_led_blink_asymmetric_start(LED_ON_TIME_MS, LED_OFF_TIME_MS);
        
        // Sprawdzenie aktualizacji OTA
        ESP_LOGI(TAG, "Sprawdzanie aktualizacji OTA...");
        rk_ota_config_t ota_config = {
            .github_user = GITHUB_USER,
            .github_repo = GITHUB_REPO,
            .github_branch = GITHUB_BRANCH,
            .firmware_file = GITHUB_FILE
        };
        
        rk_ota_check_update(&ota_config);
        
    } else {
        ESP_LOGE(TAG, "Nie udało się połączyć z WiFi");
        rk_led_blink_stop();
        // Bardzo szybkie mruganie w przypadku błędu
        rk_led_blink_start(100);
    }
    
    // Główna pętla aplikacji
    while(1) {
        ESP_LOGI(TAG, "Aplikacja działa... LED: ON=%dms, OFF=%dms", 
                 LED_ON_TIME_MS, LED_OFF_TIME_MS);
        vTaskDelay(pdMS_TO_TICKS(10000)); // 10 sekund
        
        // Można tutaj dodać sprawdzanie OTA co jakiś czas
        // rk_ota_check_update(&ota_config);
    }
}