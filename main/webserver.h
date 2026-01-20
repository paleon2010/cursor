/**
 * @file webserver.h
 * @brief Módulo de servidor web HTTP
 * 
 * Este módulo implementa un servidor web HTTP que lista archivos disponibles
 * en la tarjeta MicroSD y permite su descarga. Utiliza el mutex de la tarjeta
 * SD para acceder de forma segura a los archivos.
 */

#ifndef WEBSERVER_H
#define WEBSERVER_H

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#ifdef __cplusplus
extern "C" {
#endif

// Puerto del servidor web
#define WEB_SERVER_PORT 80

/**
 * @brief Crea y configura la tarea del servidor web
 * @param wifi_event_group Grupo de eventos WiFi para esperar conexión
 * @param sd_mutex Mutex para acceder a la tarjeta SD
 * @note Esta función debe ser llamada desde app_main()
 */
void webserver_init_task(EventGroupHandle_t wifi_event_group, SemaphoreHandle_t sd_mutex);

#ifdef __cplusplus
}
#endif

#endif // WEBSERVER_H
