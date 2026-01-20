/**
 * @file dht22.h
 * @brief Módulo de sensor de temperatura y humedad DHT22
 * 
 * Este módulo maneja la lectura del sensor DHT22 mediante protocolo
 * de un solo cable. Proporciona funciones para obtener temperatura
 * y humedad, y mantiene los valores actualizados en una tarea separada.
 */

#ifndef DHT22_H
#define DHT22_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#ifdef __cplusplus
extern "C" {
#endif

// Pin GPIO para el sensor DHT22
#define DHT22_GPIO_PIN       4

// Estructura para almacenar datos del sensor
typedef struct {
    float temperature;   // Temperatura en grados Celsius
    float humidity;      // Humedad relativa en porcentaje
    bool valid;          // Indica si los datos son válidos
} dht22_data_t;

/**
 * @brief Obtiene los últimos datos leídos del sensor
 * @param data Puntero a estructura donde se almacenarán los datos
 * @return ESP_OK si los datos son válidos, ESP_FAIL en caso contrario
 */
esp_err_t dht22_get_data(dht22_data_t *data);

/**
 * @brief Crea y configura la tarea de lectura del sensor DHT22
 * @note Esta función debe ser llamada desde app_main()
 */
void dht22_init_task(void);

#ifdef __cplusplus
}
#endif

#endif // DHT22_H
