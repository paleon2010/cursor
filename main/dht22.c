/**
 * @file dht22.c
 * @brief Implementación del módulo DHT22
 */

#include "dht22.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/task.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>
#include <math.h>

static const char *TAG = "DHT22";

// Mutex para proteger acceso a los datos del sensor
static SemaphoreHandle_t dht22_mutex = NULL;

// Estructura global con los datos del sensor
static dht22_data_t sensor_data = {
    .temperature = 0.0,
    .humidity = 0.0,
    .valid = false
};

// Timeouts para el protocolo DHT22 (en microsegundos)
#define DHT22_START_SIGNAL_LOW_US    18000  // Señal de inicio: 18ms LOW
#define DHT22_START_SIGNAL_HIGH_US   30     // Señal de inicio: 30us HIGH
#define DHT22_TIMEOUT_US              100   // Timeout para esperar respuesta
#define DHT22_BIT_0_THRESHOLD_US      50    // Umbral para bit 0 (< 50us = 0, > 50us = 1)

/**
 * @brief Lee un bit del sensor DHT22
 * @return 0, 1, o -1 en caso de error
 */
static int dht22_read_bit(void) {
    // Esperar que el pin baje (inicio de bit)
    int timeout = 0;
    while (gpio_get_level(DHT22_GPIO_PIN) == 1) {
        if (++timeout > DHT22_TIMEOUT_US) {
            return -1;
        }
        ets_delay_us(1);
    }
    
    // Medir tiempo en HIGH
    timeout = 0;
    while (gpio_get_level(DHT22_GPIO_PIN) == 0) {
        if (++timeout > DHT22_TIMEOUT_US) {
            return -1;
        }
        ets_delay_us(1);
    }
    
    int high_time = 0;
    while (gpio_get_level(DHT22_GPIO_PIN) == 1) {
        if (++high_time > DHT22_TIMEOUT_US) {
            return -1;
        }
        ets_delay_us(1);
    }
    
    // Si el tiempo en HIGH es mayor al umbral, es un bit 1
    return (high_time > DHT22_BIT_0_THRESHOLD_US) ? 1 : 0;
}

/**
 * @brief Lee un byte del sensor DHT22
 * @return Byte leído o -1 en caso de error
 */
static int dht22_read_byte(void) {
    int byte = 0;
    for (int i = 0; i < 8; i++) {
        int bit = dht22_read_bit();
        if (bit < 0) {
            return -1;
        }
        byte |= (bit << (7 - i));
    }
    return byte;
}

/**
 * @brief Realiza una lectura completa del sensor DHT22
 * @param data Puntero a estructura donde almacenar los datos
 * @return ESP_OK si la lectura fue exitosa, ESP_FAIL en caso contrario
 */
static esp_err_t dht22_read_sensor(dht22_data_t *data) {
    uint8_t raw_data[5] = {0};
    
    // Configurar pin como salida
    gpio_set_direction(DHT22_GPIO_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(DHT22_GPIO_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(250)); // Esperar estabilización
    
    // Enviar señal de inicio: LOW por 18ms
    gpio_set_level(DHT22_GPIO_PIN, 0);
    ets_delay_us(DHT22_START_SIGNAL_LOW_US);
    
    // HIGH por 30us
    gpio_set_level(DHT22_GPIO_PIN, 1);
    ets_delay_us(DHT22_START_SIGNAL_HIGH_US);
    
    // Cambiar a entrada con pull-up
    gpio_set_direction(DHT22_GPIO_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(DHT22_GPIO_PIN, GPIO_PULLUP_ONLY);
    
    // Esperar respuesta del sensor (debe bajar)
    int timeout = 0;
    while (gpio_get_level(DHT22_GPIO_PIN) == 1) {
        if (++timeout > 100) {
            ESP_LOGE(TAG, "Timeout esperando respuesta del sensor");
            return ESP_FAIL;
        }
        ets_delay_us(1);
    }
    
    // Esperar que suba (inicio de datos)
    timeout = 0;
    while (gpio_get_level(DHT22_GPIO_PIN) == 0) {
        if (++timeout > 100) {
            ESP_LOGE(TAG, "Timeout en inicio de datos");
            return ESP_FAIL;
        }
        ets_delay_us(1);
    }
    
    // Esperar que baje (inicio real de datos)
    timeout = 0;
    while (gpio_get_level(DHT22_GPIO_PIN) == 1) {
        if (++timeout > 100) {
            ESP_LOGE(TAG, "Timeout en inicio real de datos");
            return ESP_FAIL;
        }
        ets_delay_us(1);
    }
    
    // Leer 5 bytes de datos
    for (int i = 0; i < 5; i++) {
        int byte = dht22_read_byte();
        if (byte < 0) {
            ESP_LOGE(TAG, "Error leyendo byte %d", i);
            return ESP_FAIL;
        }
        raw_data[i] = (uint8_t)byte;
    }
    
    // Verificar checksum
    uint8_t checksum = raw_data[0] + raw_data[1] + raw_data[2] + raw_data[3];
    if (checksum != raw_data[4]) {
        ESP_LOGE(TAG, "Error de checksum: esperado %d, recibido %d", checksum, raw_data[4]);
        return ESP_FAIL;
    }
    
    // Convertir datos
    uint16_t humidity_raw = (raw_data[0] << 8) | raw_data[1];
    uint16_t temperature_raw = (raw_data[2] << 8) | raw_data[3];
    
    data->humidity = humidity_raw / 10.0;
    
    // El bit más significativo del byte de temperatura indica signo
    if (temperature_raw & 0x8000) {
        // Temperatura negativa
        temperature_raw &= 0x7FFF;
        data->temperature = -(temperature_raw / 10.0);
    } else {
        data->temperature = temperature_raw / 10.0;
    }
    
    data->valid = true;
    
    return ESP_OK;
}

/**
 * @brief Tarea para leer periódicamente el sensor DHT22
 */
static void dht22_task(void *pvParameters) {
    ESP_LOGI(TAG, "Inicializando sensor DHT22 en GPIO %d", DHT22_GPIO_PIN);
    
    // Configurar GPIO
    gpio_reset_pin(DHT22_GPIO_PIN);
    gpio_set_direction(DHT22_GPIO_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(DHT22_GPIO_PIN, 1);
    
    vTaskDelay(pdMS_TO_TICKS(2000)); // Esperar estabilización del sensor
    
    dht22_data_t local_data;
    
    while (1) {
        // Leer sensor
        esp_err_t ret = dht22_read_sensor(&local_data);
        
        if (ret == ESP_OK && local_data.valid) {
            // Proteger escritura con mutex
            if (xSemaphoreTake(dht22_mutex, portMAX_DELAY) == pdTRUE) {
                sensor_data.temperature = local_data.temperature;
                sensor_data.humidity = local_data.humidity;
                sensor_data.valid = true;
                xSemaphoreGive(dht22_mutex);
                
                ESP_LOGI(TAG, "Temperatura: %.1f°C, Humedad: %.1f%%", 
                        sensor_data.temperature, sensor_data.humidity);
            }
        } else {
            ESP_LOGW(TAG, "Error leyendo sensor DHT22");
            if (xSemaphoreTake(dht22_mutex, portMAX_DELAY) == pdTRUE) {
                sensor_data.valid = false;
                xSemaphoreGive(dht22_mutex);
            }
        }
        
        // El DHT22 requiere al menos 2 segundos entre lecturas
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

/**
 * @brief Obtiene los últimos datos leídos del sensor
 */
esp_err_t dht22_get_data(dht22_data_t *data) {
    if (data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (dht22_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(dht22_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        data->temperature = sensor_data.temperature;
        data->humidity = sensor_data.humidity;
        data->valid = sensor_data.valid;
        xSemaphoreGive(dht22_mutex);
        
        return (data->valid) ? ESP_OK : ESP_FAIL;
    }
    
    return ESP_ERR_TIMEOUT;
}

/**
 * @brief Crea y configura la tarea de lectura del sensor DHT22
 */
void dht22_init_task(void) {
    // Crear mutex si no existe
    if (dht22_mutex == NULL) {
        dht22_mutex = xSemaphoreCreateMutex();
        if (dht22_mutex == NULL) {
            ESP_LOGE(TAG, "Error creando mutex para DHT22");
            return;
        }
    }
    
    xTaskCreate(&dht22_task, "dht22_task", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "Tarea DHT22 creada");
}
