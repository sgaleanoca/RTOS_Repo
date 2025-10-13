idf_component_register(SRCS "ledc_basic_example_main.c"
                          "ntc_sensor.c"
                          "potentiometer.c"
                          "rgb_led.c"
                    PRIV_REQUIRES esp_driver_ledc 
                                 esp_driver_gpio 
                                 esp_adc
                                 driver
                                 esp_timer
                                 freertos
                    INCLUDE_DIRS ".")
