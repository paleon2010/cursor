/**
 * @file oled.h
 * @brief Módulo de visualización OLED SSD1306 128x64
 * 
 * Este módulo maneja la inicialización y control de una pantalla OLED
 * SSD1306 de 128x64 píxeles mediante interfaz I2C.
 */

#ifndef OLED_H
#define OLED_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Configuración I2C para OLED 128x64
#define I2C_MASTER_SCL_IO    22    /* GPIO 22 para SCL */
#define I2C_MASTER_SDA_IO    21    /* GPIO 21 para SDA */
#define I2C_MASTER_NUM       I2C_NUM_0
#define I2C_MASTER_FREQ_HZ   100000

// Dirección del OLED SSD1306
#define SSD1306_ADDRESS      0x3C

/**
 * @brief Inicializa el display OLED
 * @return ESP_OK si la inicialización fue exitosa
 */
esp_err_t oled_init(void);

/**
 * @brief Limpia el display OLED
 */
void oled_clear(void);

/**
 * @brief Muestra texto en el OLED (implementación simplificada)
 * @param text Texto a mostrar
 */
void oled_display_text(const char *text);

/**
 * @brief Tipo de vista en el OLED
 */
typedef enum {
    OLED_VIEW_SD_CARD,      // Ver datos de la tarjeta SD
    OLED_VIEW_TEMPERATURE   // Ver datos del sensor DHT22
} oled_view_t;

/**
 * @brief Cambia la vista actual del OLED
 * @param view Tipo de vista a mostrar
 */
void oled_set_view(oled_view_t view);

/**
 * @brief Obtiene la vista actual del OLED
 * @return Tipo de vista actual
 */
oled_view_t oled_get_view(void);

/**
 * @brief Crea y configura la tarea de visualización OLED
 * @param sd_mutex Mutex para acceder a la tarjeta SD (puede ser NULL)
 * @note Esta función debe ser llamada desde app_main()
 */
void oled_init_task(SemaphoreHandle_t sd_mutex);

#ifdef __cplusplus
}
#endif

#endif // OLED_H
