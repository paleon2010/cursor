/**
 * @file wifi.h
 * @brief Módulo de configuración WiFi para ESP32
 * 
 * Este módulo maneja la conexión WiFi del ESP32, incluyendo inicialización,
 * conexión automática y manejo de eventos de reconexión.
 */

#ifndef WIFI_H
#define WIFI_H

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#ifdef __cplusplus
extern "C" {
#endif

// Bits de eventos WiFi
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

/**
 * @brief Obtiene el grupo de eventos WiFi
 * @return Puntero al grupo de eventos WiFi
 */
EventGroupHandle_t wifi_get_event_group(void);

/**
 * @brief Crea y configura la tarea WiFi
 * @note Esta función debe ser llamada desde app_main()
 */
void wifi_init_task(void);

#ifdef __cplusplus
}
#endif

#endif // WIFI_H
