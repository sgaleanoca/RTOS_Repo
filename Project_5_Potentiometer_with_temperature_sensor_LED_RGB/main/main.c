#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "potentiometer.h"
#include "rgb_led.h"

static const char *TAG = "MAIN";

void pot_task(void *arg)
{
    while (1) {
        uint8_t pct = pot_get_percent();
        uint32_t mv = pot_get_voltage_mv();
        ESP_LOGI(TAG, "POT: %d %%   %d mV", pct, mv);

        // Actualizar PWM del LED verde
        rgb_set_green_percent(pct);

        vTaskDelay(pdMS_TO_TICKS(250)); // leer 4 veces por segundo
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Inicializando componentes...");
    pot_init();
    rgb_led_init();

    xTaskCreate(pot_task, "pot_task", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "Tareas creadas. Ejecutando...");
}
