idf_component_register(SRCS "potentiometer.c" "button_control.c" "ntc_sensor.c" "main.c"
                       PRIV_REQUIRES spi_flash esp_adc esp_driver_ledc esp_timer
                       INCLUDE_DIRS "")
