/**
 * @file webserver.c
 * @brief Implementación del módulo de servidor web
 */

#include "webserver.h"
#include "sd_card.h"
#include "wifi.h"
#include "dht22.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "freertos/task.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>

static const char *TAG = "WEBSERVER";

static httpd_handle_t server = NULL;

// Mutex para acceso a la tarjeta SD (se pasa desde main)
static SemaphoreHandle_t sd_mutex = NULL;

/**
 * @brief Handler para la página principal que lista archivos
 */
static esp_err_t index_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    
    // Iniciar HTML
    httpd_resp_sendstr_chunk(req, "<!DOCTYPE html><html><head>");
    httpd_resp_sendstr_chunk(req, "<meta name='viewport' content='width=device-width, initial-scale=1'>");
    httpd_resp_sendstr_chunk(req, "<title>ESP32 - Archivos MicroSD</title>");
    httpd_resp_sendstr_chunk(req, "<style>body{font-family:Arial;margin:20px;}");
    httpd_resp_sendstr_chunk(req, "a{display:inline-block;padding:10px;margin:5px;background:#4CAF50;");
    httpd_resp_sendstr_chunk(req, "color:white;text-decoration:none;border-radius:5px;}</style>");
    httpd_resp_sendstr_chunk(req, "</head><body><h1>Archivos en MicroSD</h1>");
    httpd_resp_sendstr_chunk(req, "<a href='/sensor'>Ver Sensor DHT22</a>");
    
    if (sd_mutex != NULL && xSemaphoreTake(sd_mutex, pdMS_TO_TICKS(5000)) == pdTRUE) {
        DIR *dir = opendir(SD_MOUNT_POINT);
        if (dir != NULL) {
            struct dirent *entry;
            int file_count = 0;
            
            while ((entry = readdir(dir)) != NULL) {
                if (entry->d_type == DT_REG) {
                    file_count++;
                    char file_link[512];
                    char file_path[256];
                    snprintf(file_path, sizeof(file_path), "%s/%s", SD_MOUNT_POINT, entry->d_name);
                    
                    struct stat file_stat;
                    if (stat(file_path, &file_stat) == 0) {
                        snprintf(file_link, sizeof(file_link),
                                "<a href='/download?file=%s'>%s (%.2f KB)</a>",
                                entry->d_name, entry->d_name, file_stat.st_size / 1024.0);
                        httpd_resp_sendstr_chunk(req, file_link);
                    }
                }
            }
            
            if (file_count == 0) {
                httpd_resp_sendstr_chunk(req, "<p>No hay archivos en la tarjeta.</p>");
            }
            
            closedir(dir);
        } else {
            httpd_resp_sendstr_chunk(req, "<p>Error al leer el directorio.</p>");
        }
        xSemaphoreGive(sd_mutex);
    } else {
        httpd_resp_sendstr_chunk(req, "<p>Error: No se pudo acceder a la tarjeta (timeout).</p>");
    }
    
    httpd_resp_sendstr_chunk(req, "</body></html>");
    httpd_resp_sendstr_chunk(req, NULL);
    
    return ESP_OK;
}

/**
 * @brief Handler para descargar archivos
 */
static esp_err_t download_handler(httpd_req_t *req) {
    char filepath[256];
    char filename[128];
    
    if (httpd_req_get_url_query_str(req, filename, sizeof(filename)) == ESP_OK) {
        char *param = strstr(filename, "file=");
        if (param) {
            param += 5; // Saltar "file="
            snprintf(filepath, sizeof(filepath), "%s/%s", SD_MOUNT_POINT, param);
            
            if (sd_mutex != NULL && xSemaphoreTake(sd_mutex, pdMS_TO_TICKS(5000)) == pdTRUE) {
                FILE *f = fopen(filepath, "r");
                if (f == NULL) {
                    xSemaphoreGive(sd_mutex);
                    httpd_resp_send_404(req);
                    return ESP_FAIL;
                }
                
                // Obtener tamaño del archivo
                fseek(f, 0, SEEK_END);
                long fsize = ftell(f);
                fseek(f, 0, SEEK_SET);
                
                // Configurar headers
                httpd_resp_set_type(req, "application/octet-stream");
                char content_disp[128];
                snprintf(content_disp, sizeof(content_disp), "attachment; filename=\"%s\"", param);
                httpd_resp_set_hdr(req, "Content-Disposition", content_disp);
                // Nota: No establecemos Content-Length porque usamos transfer-encoding: chunked
                
                // Enviar archivo en bloques
                char *buf = malloc(1024);
                if (buf == NULL) {
                    fclose(f);
                    xSemaphoreGive(sd_mutex);
                    return ESP_ERR_NO_MEM;
                }
                
                size_t bytes_read;
                do {
                    bytes_read = fread(buf, 1, 1024, f);
                    if (bytes_read > 0) {
                        httpd_resp_send_chunk(req, buf, bytes_read);
                    }
                } while (bytes_read == 1024);
                
                free(buf);
                fclose(f);
                httpd_resp_send_chunk(req, NULL, 0);
                xSemaphoreGive(sd_mutex);
                
                ESP_LOGI(TAG, "Archivo descargado: %s (%ld bytes)", param, fsize);
                return ESP_OK;
            } else {
                httpd_resp_send_500(req);
                return ESP_FAIL;
            }
        }
    }
    
    httpd_resp_send_404(req);
    return ESP_FAIL;
}

/**
 * @brief Handler para mostrar datos del sensor DHT22
 */
static esp_err_t sensor_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    
    dht22_data_t sensor_data;
    esp_err_t ret = dht22_get_data(&sensor_data);
    
    // Iniciar HTML
    httpd_resp_sendstr_chunk(req, "<!DOCTYPE html><html><head>");
    httpd_resp_sendstr_chunk(req, "<meta name='viewport' content='width=device-width, initial-scale=1'>");
    httpd_resp_sendstr_chunk(req, "<meta http-equiv='refresh' content='5'>");
    httpd_resp_sendstr_chunk(req, "<title>ESP32 - Sensor DHT22</title>");
    httpd_resp_sendstr_chunk(req, "<style>body{font-family:Arial;margin:20px;background:#f0f0f0;}");
    httpd_resp_sendstr_chunk(req, ".container{background:white;padding:20px;border-radius:10px;max-width:600px;margin:0 auto;}");
    httpd_resp_sendstr_chunk(req, "h1{color:#333;}");
    httpd_resp_sendstr_chunk(req, ".sensor-data{font-size:24px;margin:20px 0;padding:15px;background:#e3f2fd;border-radius:5px;}");
    httpd_resp_sendstr_chunk(req, ".value{font-weight:bold;color:#1976d2;}");
    httpd_resp_sendstr_chunk(req, "a{display:inline-block;padding:10px;margin:10px 5px;background:#4CAF50;");
    httpd_resp_sendstr_chunk(req, "color:white;text-decoration:none;border-radius:5px;}</style>");
    httpd_resp_sendstr_chunk(req, "</head><body><div class='container'>");
    httpd_resp_sendstr_chunk(req, "<h1>Sensor DHT22</h1>");
    
    if (ret == ESP_OK && sensor_data.valid) {
        char temp_str[64];
        char hum_str[64];
        snprintf(temp_str, sizeof(temp_str), "<div class='sensor-data'>Temperatura: <span class='value'>%.1f°C</span></div>", 
                 sensor_data.temperature);
        snprintf(hum_str, sizeof(hum_str), "<div class='sensor-data'>Humedad: <span class='value'>%.1f%%</span></div>", 
                 sensor_data.humidity);
        
        httpd_resp_sendstr_chunk(req, temp_str);
        httpd_resp_sendstr_chunk(req, hum_str);
    } else {
        httpd_resp_sendstr_chunk(req, "<div class='sensor-data' style='color:red;'>Error: No se pudieron leer los datos del sensor</div>");
    }
    
    httpd_resp_sendstr_chunk(req, "<a href='/'>Ver Archivos</a>");
    httpd_resp_sendstr_chunk(req, "<a href='/sensor'>Actualizar</a>");
    httpd_resp_sendstr_chunk(req, "</div></body></html>");
    httpd_resp_sendstr_chunk(req, NULL);
    
    return ESP_OK;
}

/**
 * @brief Inicia el servidor web HTTP
 */
static httpd_handle_t start_webserver(void) {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = WEB_SERVER_PORT;

    ESP_LOGI(TAG, "Iniciando servidor web en puerto: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Registrar handlers
        httpd_uri_t index_uri = {
            .uri       = "/",
            .method    = HTTP_GET,
            .handler   = index_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &index_uri);

        httpd_uri_t download_uri = {
            .uri       = "/download",
            .method    = HTTP_GET,
            .handler   = download_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &download_uri);

        httpd_uri_t sensor_uri = {
            .uri       = "/sensor",
            .method    = HTTP_GET,
            .handler   = sensor_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &sensor_uri);

        ESP_LOGI(TAG, "Servidor web iniciado");
        return server;
    }

    ESP_LOGI(TAG, "Error iniciando servidor web!");
    return NULL;
}

/**
 * @brief Tarea para iniciar el servidor web (solo cuando WiFi esté conectado)
 */
static void webserver_task(void *pvParameters) {
    struct {
        EventGroupHandle_t wifi_event_group;
        SemaphoreHandle_t sd_mutex;
    } *params = (typeof(params)) pvParameters;
    
    // Esperar a que WiFi esté conectado
    EventBits_t bits = xEventGroupWaitBits(params->wifi_event_group,
                                           WIFI_CONNECTED_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        sd_mutex = params->sd_mutex;  // Guardar referencia al mutex
        server = start_webserver();
        if (server == NULL) {
            ESP_LOGE(TAG, "Error iniciando servidor web");
            free(params);
            vTaskDelete(NULL);
            return;
        }
        
        ESP_LOGI(TAG, "Servidor web listo. Accede a http://<IP_ESP32>/");
        
        free(params);
        
        // Mantener la tarea activa
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(10000));
        }
    }
    
    free(params);
    vTaskDelete(NULL);
}

/**
 * @brief Crea y configura la tarea del servidor web
 */
void webserver_init_task(EventGroupHandle_t wifi_event_group, SemaphoreHandle_t sd_mutex_param) {
    // Pasar parámetros a través de estructura
    struct {
        EventGroupHandle_t wifi_event_group;
        SemaphoreHandle_t sd_mutex;
    } *params = malloc(sizeof(*params));
    
    if (params == NULL) {
        ESP_LOGE(TAG, "Error asignando memoria para parámetros");
        return;
    }
    
    params->wifi_event_group = wifi_event_group;
    params->sd_mutex = sd_mutex_param;
    
    xTaskCreate(&webserver_task, "webserver_task", 8192, params, 5, NULL);
    ESP_LOGI(TAG, "Tarea webserver creada");
}
