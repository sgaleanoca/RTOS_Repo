idf_component_register(SRCS "main.c" "wifi_config.c" "gpio_config.c" "web_server.c"
                       PRIV_REQUIRES esp_wifi esp_http_server esp_netif esp_event nvs_flash driver freertos
                       INCLUDE_DIRS "")
