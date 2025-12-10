idf_component_register(SRCS "main.c"
                       "ntc_sensor.c"
                       "registros.c"
                       "terminal_commands.c"
                       "web_server.c"
                       "wifi_app.c"
                       "fan_control.c"
                       "rgb_led.c"
                       "time_sync.c"
                       "pir_driver.c"
                       PRIV_REQUIRES spi_flash nvs_flash esp_driver_gpio esp_adc esp_http_server json esp_wifi spiffs esp_driver_ledc esp_netif lwip esp_timer freertos
                       INCLUDE_DIRS ".")
