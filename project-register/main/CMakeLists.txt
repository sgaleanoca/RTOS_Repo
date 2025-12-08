idf_component_register(SRCS "hello_world_main.c" "nvs_manager.c"
                       PRIV_REQUIRES nvs_flash spi_flash
                       INCLUDE_DIRS "")
