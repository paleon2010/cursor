idf_component_register(
    SRCS "main.c" 
         "wifi.c" 
         "sd_card.c" 
         "oled.c" 
         "webserver.c"
         "dht22.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES driver nvs_flash esp_wifi esp_http_server fatfs spiffs
)
