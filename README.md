# Proyecto ESP32-WROOM-32 con FreeRTOS

Este proyecto implementa un sistema completo para ESP32-WROOM-32 usando FreeRTOS con las siguientes funcionalidades:

## Características

1. **Configuración WiFi**: Conecta automáticamente a la red WiFi especificada
2. **Lectura de MicroSD**: Lee archivos de una tarjeta MicroSD mediante SPI
3. **Visualización OLED**: Muestra información en una pantalla OLED 128x64 vía I2C
4. **Servidor Web**: Lista y permite descargar archivos de la MicroSD mediante HTTP

## Requisitos de Hardware

- ESP32-WROOM-32
- Módulo MicroSD con interfaz SPI
- Pantalla OLED SSD1306 128x64 con interfaz I2C
- Cables de conexión

## Conexiones

### MicroSD (SPI)
- MISO: GPIO 19
- MOSI: GPIO 23
- CLK: GPIO 18
- CS: GPIO 5

### OLED 128x64 (I2C)
- SCL: GPIO 22
- SDA: GPIO 21
- VCC: 3.3V
- GND: GND

**Nota**: No se utilizan pines de strapping del módulo ESP32-WROOM-32.

## Configuración WiFi

Edita las constantes en `main/main.c` para cambiar la configuración WiFi:

```c
#define WIFI_SSID      "repetidor"
#define WIFI_PASS      "JHUYUIJ"
```

## Compilación y Flasheo

### Requisitos previos

1. Instalar ESP-IDF (versión 4.4 o superior)
2. Configurar el entorno de ESP-IDF

### Pasos de compilación

```bash
# Configurar el proyecto
idf.py set-target esp32

# Configurar parámetros (opcional)
idf.py menuconfig

# Compilar el proyecto
idf.py build

# Flashear y abrir monitor serie
idf.py -p PORT flash monitor
```

Reemplaza `PORT` con el puerto serie de tu ESP32 (ej: COM3 en Windows, /dev/ttyUSB0 en Linux).

## Estructura del Proyecto

```
├── main/
│   ├── main.c          # Código principal con todas las tareas
│   └── CMakeLists.txt  # Configuración de compilación del componente
├── CMakeLists.txt      # Configuración principal del proyecto
├── sdkconfig.defaults  # Configuración por defecto del SDK
└── README.md           # Este archivo
```

## Funcionalidades de las Tareas

### Tarea WiFi (`wifi_task`)
- Se conecta automáticamente a la red WiFi especificada
- Maneja reconexión automática en caso de desconexión
- Emite eventos para sincronización con otras tareas

### Tarea MicroSD (`sd_card_task`)
- Inicializa la tarjeta MicroSD mediante SPI
- Lee y lista archivos periódicamente
- Usa mutex para controlar acceso exclusivo

### Tarea OLED (`oled_display_task`)
- Inicializa la pantalla OLED SSD1306 128x64
- Muestra información sobre los archivos en la MicroSD
- Actualiza la pantalla periódicamente usando el mutex compartido

### Tarea Servidor Web (`webserver_task`)
- Inicia un servidor HTTP después de conectar WiFi
- Lista archivos disponibles en la MicroSD
- Permite descargar archivos individuales
- Usa mutex para acceder de forma segura a la tarjeta

## Uso del Servidor Web

Una vez que el ESP32 se conecte a WiFi y obtenga una IP, podrás acceder al servidor web desde cualquier navegador:

- URL principal: `http://<IP_ESP32>/`
- Descarga de archivos: `http://<IP_ESP32>/download?file=<nombre_archivo>`

La IP asignada se mostrará en el monitor serie.

## Sincronización con Mutex

El proyecto utiliza un mutex (`sd_mutex`) para coordinar el acceso a la tarjeta MicroSD entre las diferentes tareas:

- Tarea de lectura MicroSD
- Tarea de visualización OLED
- Servidor web (al listar y descargar archivos)

Esto previene condiciones de carrera y garantiza la integridad de los datos.

## Solución de Problemas

### La tarjeta MicroSD no se detecta
- Verifica las conexiones SPI (MISO, MOSI, CLK, CS)
- Asegúrate de que la tarjeta esté formateada correctamente (FAT32)
- Revisa los logs en el monitor serie

### El OLED no muestra nada
- Verifica las conexiones I2C (SDA, SCL)
- Asegúrate de que la dirección I2C sea 0x3C (o ajusta en el código)
- Revisa que el display esté recibiendo 3.3V

### WiFi no se conecta
- Verifica que el SSID y contraseña sean correctos
- Asegúrate de que la red WiFi esté en rango
- Revisa los logs del monitor serie para más detalles

## Licencia

Este proyecto es de código abierto y está disponible para uso educativo y de desarrollo.
