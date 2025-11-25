idf_component_register(SRCS "main.c" "wifi_app.c" "web_server.c" "gpio_driver.c"
                       PRIV_REQUIRES spi_flash spiffs nvs_flash esp_http_server driver esp_wifi
                       INCLUDE_DIRS ".")
