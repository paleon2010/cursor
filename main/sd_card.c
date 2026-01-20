/**
 * @file sd_card.c
 * @brief Implementación del módulo de tarjeta MicroSD
 */

#include "sd_card.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "driver/spi_common.h"
#include "driver/sdspi_host.h"
#include "sdmmc_cmd.h"
#include "freertos/task.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>

static const char *TAG = "SD_CARD";

// Mutex para controlar acceso a la tarjeta MicroSD
static SemaphoreHandle_t sd_mutex = NULL;

// Referencia a la tarjeta montada
static sdmmc_card_t* card = NULL;

/**
 * @brief Tarea para inicializar y leer datos de la tarjeta MicroSD
 */
static void sd_card_task(void *pvParameters) {
    ESP_LOGI(TAG, "Inicializando tarjeta MicroSD...");

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI2_HOST;

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    esp_err_t ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error inicializando bus SPI: %s", esp_err_to_name(ret));
        vTaskDelete(NULL);
        return;
    }

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = host.slot;

    ESP_LOGI(TAG, "Montando tarjeta SD en %s", SD_MOUNT_POINT);

    ret = esp_vfs_fat_sdspi_mount(SD_MOUNT_POINT, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Error al montar el sistema de archivos. "
                     "Formatear la tarjeta si es necesario.");
        } else {
            ESP_LOGE(TAG, "Error al inicializar la tarjeta (%s). "
                     "Comprobar cableado.", esp_err_to_name(ret));
        }
        vTaskDelete(NULL);
        return;
    }

    sdmmc_card_print_info(stdout, card);
    ESP_LOGI(TAG, "Tarjeta MicroSD inicializada correctamente");

    // Leer y listar archivos periódicamente
    while (1) {
        if (xSemaphoreTake(sd_mutex, portMAX_DELAY) == pdTRUE) {
            DIR *dir = opendir(SD_MOUNT_POINT);
            if (dir != NULL) {
                struct dirent *entry;
                int file_count = 0;
                ESP_LOGI(TAG, "=== Archivos en la tarjeta ===");
                while ((entry = readdir(dir)) != NULL) {
                    if (entry->d_type == DT_REG) {
                        file_count++;
                        char full_path[256];
                        snprintf(full_path, sizeof(full_path), "%s/%s", SD_MOUNT_POINT, entry->d_name);
                        struct stat file_stat;
                        if (stat(full_path, &file_stat) == 0) {
                            ESP_LOGI(TAG, "Archivo: %s (%ld bytes)", entry->d_name, file_stat.st_size);
                        }
                    }
                }
                ESP_LOGI(TAG, "Total archivos: %d", file_count);
                closedir(dir);
            } else {
                ESP_LOGE(TAG, "Error abriendo directorio");
            }
            xSemaphoreGive(sd_mutex);
        }
        
        vTaskDelay(pdMS_TO_TICKS(10000)); // Leer cada 10 segundos
    }
}

/**
 * @brief Obtiene el mutex para acceso a la tarjeta SD
 */
SemaphoreHandle_t sd_card_get_mutex(void) {
    return sd_mutex;
}

/**
 * @brief Crea y configura la tarea de lectura de tarjeta MicroSD
 */
void sd_card_init_task(void) {
    // Crear mutex si no existe
    if (sd_mutex == NULL) {
        sd_mutex = xSemaphoreCreateMutex();
        if (sd_mutex == NULL) {
            ESP_LOGE(TAG, "Error creando mutex para SD");
            return;
        }
    }
    
    xTaskCreate(&sd_card_task, "sd_card_task", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "Tarea SD card creada");
}
