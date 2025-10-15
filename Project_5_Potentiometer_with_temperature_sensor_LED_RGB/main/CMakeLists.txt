idf_component_register(SRCS "ntc_sensor.c" "main.c" "potentiometer.c" "rgb_led.c"
                       PRIV_REQUIRES spi_flash esp_adc esp_driver_ledc
                       INCLUDE_DIRS "")
