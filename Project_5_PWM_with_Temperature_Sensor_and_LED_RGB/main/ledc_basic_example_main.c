#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

// Incluimos nuestras librerías
#include "potentiometer.h"
#include "ntc_sensor.h"
#include "rgb_led.h"

static const char *TAG = "Project_5";

// Función para mapear un valor de un rango a otro
float map_value(float value, float in_min, float in_max, float out_min, float out_max) {
    // Acotar el valor al rango de entrada
    if (value < in_min) value = in_min;
    if (value > in_max) value = in_max;
    return (value - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// Tarea principal que se ejecutará en bucle
void sensor_read_task(void *pvParameters) {
    while (1) {
        // 1. Leer sensores
        float voltage = get_potentiometer_voltage();
        float temperature = get_ntc_temperature();

        // 2. Calcular la intensidad de los LEDs
        // LED Rojo (Temperatura): 10°C es 0%, 50°C es 100%
        float red_intensity = map_value(temperature, 10.0, 50.0, 0.0, 100.0);
        
        // LED Verde (Potenciómetro): 0mV es 0%, 3300mV es 100%
        float green_intensity = map_value(voltage, 0.0, 3300.0, 0.0, 100.0);
        
        // 3. Actualizar los LEDs
        set_red_intensity(red_intensity);
        set_green_intensity(green_intensity);

        // 4. Imprimir los valores en el monitor serie
        printf("Temperatura: %.2f C  |  Voltaje: %.0f mV\n", temperature, voltage);

        // Esperar 250 milisegundos antes de la siguiente iteración
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "Inicializando componentes...");

    // Inicializar cada uno de nuestros módulos
    potentiometer_init();
    ntc_sensor_init();
    rgb_led_init();

    ESP_LOGI(TAG, "Componentes inicializados. Creando tarea.");

    // Crear la tarea que leerá los sensores y controlará los LEDs
    xTaskCreate(sensor_read_task, "sensor_read_task", 2048, NULL, 5, NULL);
}