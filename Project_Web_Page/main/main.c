#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "wifi_config.h"
#include "gpio_config.h"
#include "web_server.h"

static const char *TAG = "main";

void app_main(void)
{
    // Inicializar NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Configurar GPIO
    configure_gpio();

    // Inicializar WiFi
    wifi_init_softap();

    // Iniciar servidor web
    start_webserver();

    // Crear tarea para leer botones
    xTaskCreate(button_task, "button_task", 2048, NULL, 10, NULL);

    ESP_LOGI(TAG, "Sistema iniciado correctamente");
}
