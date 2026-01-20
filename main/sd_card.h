/**
 * @file sd_card.h
 * @brief Módulo de gestión de tarjeta MicroSD
 * 
 * Este módulo maneja la inicialización y lectura de archivos de una
 * tarjeta MicroSD mediante interfaz SPI. Incluye funciones para obtener
 * el mutex de acceso compartido.
 */

#ifndef SD_CARD_H
#define SD_CARD_H

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#ifdef __cplusplus
extern "C" {
#endif

// Pines de configuración SPI para MicroSD
#define PIN_NUM_MISO   19
#define PIN_NUM_MOSI   23
#define PIN_NUM_CLK    18
#define PIN_NUM_CS     5

// Punto de montaje de la tarjeta
#define SD_MOUNT_POINT "/sdcard"

/**
 * @brief Obtiene el mutex para acceso a la tarjeta SD
 * @return Puntero al mutex de la tarjeta SD
 */
SemaphoreHandle_t sd_card_get_mutex(void);

/**
 * @brief Crea y configura la tarea de lectura de tarjeta MicroSD
 * @note Esta función debe ser llamada desde app_main()
 */
void sd_card_init_task(void);

#ifdef __cplusplus
}
#endif

#endif // SD_CARD_H
