idf_component_register(SRCS "terminal_commands.c" "registros.c" "ntc_sensor.c" "main.c" "wifi_app.c" "api_server.c" "gpio_driver.c"
                       PRIV_REQUIRES spi_flash spiffs nvs_flash esp_http_server driver esp_wifi esp_adc json
                       INCLUDE_DIRS ".")
