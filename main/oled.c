/**
 * @file oled.c
 * @brief Implementación del módulo OLED
 */

#include "oled.h"
#include "sd_card.h"
#include "dht22.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "freertos/task.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <stdio.h>
#include <dirent.h>
#include <string.h>

static const char *TAG = "OLED";

#define SSD1306_CMD_MODE     0x00
#define SSD1306_DATA_MODE    0x40

// Buffer para mostrar texto
static char display_buffer[17];  // 16 caracteres + null

// Vista actual del OLED
static oled_view_t current_view = OLED_VIEW_SD_CARD;

/**
 * @brief Inicializa el bus I2C para el OLED
 */
static esp_err_t i2c_master_init(void) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    
    esp_err_t err = i2c_param_config(I2C_MASTER_NUM, &conf);
    if (err != ESP_OK) {
        return err;
    }
    
    return i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
}

/**
 * @brief Escribe un comando al OLED
 */
static void ssd1306_write_cmd(uint8_t cmd) {
    i2c_cmd_handle_t i2c_cmd = i2c_cmd_link_create();
    i2c_master_start(i2c_cmd);
    i2c_master_write_byte(i2c_cmd, (SSD1306_ADDRESS << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(i2c_cmd, SSD1306_CMD_MODE, true);
    i2c_master_write_byte(i2c_cmd, cmd, true);
    i2c_master_stop(i2c_cmd);
    i2c_master_cmd_begin(I2C_MASTER_NUM, i2c_cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(i2c_cmd);
}

/**
 * @brief Inicializa el display OLED SSD1306 128x64
 */
esp_err_t oled_init(void) {
    esp_err_t ret = i2c_master_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error inicializando I2C: %s", esp_err_to_name(ret));
        return ret;
    }
    
    vTaskDelay(pdMS_TO_TICKS(100)); // Esperar a que el display se inicialice
    
    ssd1306_write_cmd(0xAE); // Display OFF
    ssd1306_write_cmd(0xD5); // Set Display Clock Divide Ratio / OSC Frequency
    ssd1306_write_cmd(0x80); // Display Clock Divide Ratio / OSC Frequency
    ssd1306_write_cmd(0xA8); // Set Multiplex Ratio
    ssd1306_write_cmd(0x3F); // Multiplex ratio for 128x64 (64-1)
    ssd1306_write_cmd(0xD3); // Set Display Offset
    ssd1306_write_cmd(0x00); // Display Offset
    ssd1306_write_cmd(0x40); // Set Display Start Line
    ssd1306_write_cmd(0x8D); // Set Charge Pump
    ssd1306_write_cmd(0x14); // Charge Pump (0x10 External, 0x14 Internal DC/DC)
    ssd1306_write_cmd(0x20); // Set Memory Addressing Mode
    ssd1306_write_cmd(0x00); // Horizontal Addressing Mode
    ssd1306_write_cmd(0xA1); // Set Segment Re-map (A0/A1)
    ssd1306_write_cmd(0xC8); // Set COM Output Scan Direction
    ssd1306_write_cmd(0xDA); // Set COM Pins Hardware Configuration
    ssd1306_write_cmd(0x12); // Alternative COM Pin Configuration
    ssd1306_write_cmd(0x81); // Set Contrast Control
    ssd1306_write_cmd(0xCF); // Contrast Value
    ssd1306_write_cmd(0xD9); // Set Pre-charge Period
    ssd1306_write_cmd(0xF1); // Set Pre-charge Period (0x22 External, 0xF1 Internal)
    ssd1306_write_cmd(0xDB); // Set VCOMH Deselect Level
    ssd1306_write_cmd(0x40); // VCOMH Deselect Level
    ssd1306_write_cmd(0xA4); // Set Entire Display ON/OFF
    ssd1306_write_cmd(0xA6); // Set Normal/Inverse Display
    ssd1306_write_cmd(0xAF); // Display ON
    
    return ESP_OK;
}

/**
 * @brief Limpia el display OLED
 */
void oled_clear(void) {
    ssd1306_write_cmd(0x21); // Set Column Address
    ssd1306_write_cmd(0x00);
    ssd1306_write_cmd(0x7F);
    ssd1306_write_cmd(0x22); // Set Page Address
    ssd1306_write_cmd(0x00);
    ssd1306_write_cmd(0x07);
    
    i2c_cmd_handle_t i2c_cmd;
    for (int i = 0; i < 8; i++) {
        i2c_cmd = i2c_cmd_link_create();
        i2c_master_start(i2c_cmd);
        i2c_master_write_byte(i2c_cmd, (SSD1306_ADDRESS << 1) | I2C_MASTER_WRITE, true);
        i2c_master_write_byte(i2c_cmd, SSD1306_DATA_MODE, true);
        for (int j = 0; j < 128; j++) {
            i2c_master_write_byte(i2c_cmd, 0x00, true);
        }
        i2c_master_stop(i2c_cmd);
        i2c_master_cmd_begin(I2C_MASTER_NUM, i2c_cmd, pdMS_TO_TICKS(100));
        i2c_cmd_link_delete(i2c_cmd);
    }
}

/**
 * @brief Muestra texto en el OLED (implementación simplificada)
 */
void oled_display_text(const char *text) {
    // Implementación simplificada - en producción usarías una fuente bitmap
    oled_clear();
    ESP_LOGI(TAG, "OLED: %s", text);
}

/**
 * @brief Tarea para mostrar contenido en el OLED con menú de selección
 */
static void oled_display_task(void *pvParameters) {
    SemaphoreHandle_t sd_mutex = (SemaphoreHandle_t) pvParameters;
    int view_counter = 0;
    
    ESP_LOGI(TAG, "Inicializando display OLED...");
    
    esp_err_t ret = oled_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error inicializando OLED: %s", esp_err_to_name(ret));
        vTaskDelete(NULL);
        return;
    }
    
    oled_clear();
    oled_display_text("ESP32 Ready");
    
    ESP_LOGI(TAG, "Display OLED inicializado");
    
    while (1) {
        oled_view_t view = current_view;  // Leer vista actual
        
        if (view == OLED_VIEW_SD_CARD) {
            // Mostrar datos de la tarjeta SD
            if (sd_mutex != NULL && xSemaphoreTake(sd_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
                DIR *dir = opendir(SD_MOUNT_POINT);
                if (dir != NULL) {
                    struct dirent *entry;
                    int file_count = 0;
                    
                    // Contar archivos
                    while ((entry = readdir(dir)) != NULL) {
                        if (entry->d_type == DT_REG) {
                            file_count++;
                        }
                    }
                    closedir(dir);
                    
                    // Mostrar en OLED
                    snprintf(display_buffer, sizeof(display_buffer), "SD: %d archivos", file_count);
                    oled_display_text(display_buffer);
                    ESP_LOGI(TAG, "OLED actualizado: %s", display_buffer);
                } else {
                    oled_display_text("Error SD");
                    ESP_LOGE(TAG, "Error accediendo a la tarjeta desde OLED");
                }
                xSemaphoreGive(sd_mutex);
            } else if (sd_mutex != NULL) {
                ESP_LOGW(TAG, "Timeout al tomar mutex en tarea OLED");
            }
        } else if (view == OLED_VIEW_TEMPERATURE) {
            // Mostrar datos del sensor DHT22
            dht22_data_t sensor_data;
            if (dht22_get_data(&sensor_data) == ESP_OK && sensor_data.valid) {
                snprintf(display_buffer, sizeof(display_buffer), "T:%.1fC H:%.1f%%", 
                        sensor_data.temperature, sensor_data.humidity);
                oled_display_text(display_buffer);
                ESP_LOGI(TAG, "OLED actualizado: %s", display_buffer);
            } else {
                oled_display_text("Sensor Error");
                ESP_LOGW(TAG, "Error leyendo sensor DHT22");
            }
        }
        
        // Cambiar automáticamente de vista cada 10 ciclos (50 segundos)
        view_counter++;
        if (view_counter >= 10) {
            view_counter = 0;
            current_view = (current_view == OLED_VIEW_SD_CARD) ? 
                          OLED_VIEW_TEMPERATURE : OLED_VIEW_SD_CARD;
            ESP_LOGI(TAG, "Cambiando vista OLED a: %s", 
                    (current_view == OLED_VIEW_SD_CARD) ? "SD_CARD" : "TEMPERATURE");
        }
        
        vTaskDelay(pdMS_TO_TICKS(5000)); // Actualizar cada 5 segundos
    }
}

/**
 * @brief Cambia la vista actual del OLED
 */
void oled_set_view(oled_view_t view) {
    current_view = view;
    ESP_LOGI(TAG, "Vista OLED cambiada a: %d", view);
}

/**
 * @brief Obtiene la vista actual del OLED
 */
oled_view_t oled_get_view(void) {
    return current_view;
}

/**
 * @brief Crea y configura la tarea de visualización OLED
 */
void oled_init_task(SemaphoreHandle_t sd_mutex) {
    xTaskCreate(&oled_display_task, "oled_task", 4096, sd_mutex, 5, NULL);
    ESP_LOGI(TAG, "Tarea OLED creada");
}
