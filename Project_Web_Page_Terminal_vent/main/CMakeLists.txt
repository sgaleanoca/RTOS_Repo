idf_component_register(SRCS "main.c"
                             "session.c"
                             "gpio_control.c"
                             "wifi.c"
                             "web_content.c"
                             "http_handlers.c"
                             "web_server.c"
                       PRIV_REQUIRES esp_wifi esp_http_server esp_netif esp_event nvs_flash driver freertos esp_timer
                       INCLUDE_DIRS "")
