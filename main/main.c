/**
 * @file main.c
 * @brief Función principal de la aplicación ESP32 con FreeRTOS
 * 
 * Este archivo inicializa los módulos del sistema:
 * - WiFi: Conexión a red inalámbrica
 * - MicroSD: Lectura de archivos desde tarjeta
 * - OLED: Visualización de información
 * - Web Server: Servidor HTTP para acceso remoto
 */

#include <stdio.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// Módulos del sistema
#include "wifi.h"
#include "sd_card.h"
#include "oled.h"
#include "webserver.h"
#include "dht22.h"

static const char *TAG = "MAIN";

/**
 * @brief Función principal de la aplicación
 * 
 * Inicializa NVS, crea los mutex necesarios y lanza las tareas
 * de cada módulo del sistema.
 */
void app_main(void) {
    ESP_LOGI(TAG, "Iniciando aplicación ESP32 con FreeRTOS...");

    // Inicializar NVS (necesario para WiFi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "NVS inicializado correctamente");

    // Inicializar módulo de tarjeta SD (crea el mutex)
    sd_card_init_task();
    
    // Obtener el mutex de SD card para compartirlo con otros módulos
    SemaphoreHandle_t sd_mutex = sd_card_get_mutex();
    if (sd_mutex == NULL) {
        ESP_LOGE(TAG, "Error: No se pudo obtener el mutex de SD");
        return;
    }

    // Inicializar módulo WiFi
    wifi_init_task();
    
    // Obtener el grupo de eventos WiFi para sincronización
    EventGroupHandle_t wifi_event_group = wifi_get_event_group();
    if (wifi_event_group == NULL) {
        ESP_LOGW(TAG, "Grupo de eventos WiFi no disponible todavía");
        // Esperar un poco a que se inicialice
        vTaskDelay(pdMS_TO_TICKS(1000));
        wifi_event_group = wifi_get_event_group();
    }

    // Inicializar módulo DHT22 (sensor de temperatura)
    dht22_init_task();

    // Inicializar módulo OLED (pasa el mutex para acceso a SD)
    oled_init_task(sd_mutex);

    // Inicializar módulo servidor web (espera a que WiFi esté conectado)
    webserver_init_task(wifi_event_group, sd_mutex);

    ESP_LOGI(TAG, "Todas las tareas creadas. Sistema operativo.");
    ESP_LOGI(TAG, "Módulos activos:");
    ESP_LOGI(TAG, "  - WiFi: Conexión a red inalámbrica");
    ESP_LOGI(TAG, "  - MicroSD: Lectura de archivos");
    ESP_LOGI(TAG, "  - DHT22: Sensor de temperatura y humedad");
    ESP_LOGI(TAG, "  - OLED: Visualización de información con menú");
    ESP_LOGI(TAG, "  - Web Server: Servidor HTTP en puerto 80");
}
