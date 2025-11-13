#include "gpio_config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "gpio_config";

// Estados
static bool estadoAmarillo = false;
static bool estadoAzul = false;

// Estados para evitar múltiples detecciones por botón
static bool botonPresionadoAmarillo = false;
static bool botonPresionadoAzul = false;

void configure_gpio(void)
{
    // Configurar LEDs como salidas
    gpio_reset_pin(LED_AMARILLO);
    gpio_set_direction(LED_AMARILLO, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_AMARILLO, 0);

    gpio_reset_pin(LED_AZUL);
    gpio_set_direction(LED_AZUL, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_AZUL, 0);

    // Configurar botones como entradas con pull-up
    gpio_reset_pin(BOTON_AMARILLO);
    gpio_set_direction(BOTON_AMARILLO, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BOTON_AMARILLO, GPIO_PULLUP_ONLY);

    gpio_reset_pin(BOTON_AZUL);
    gpio_set_direction(BOTON_AZUL, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BOTON_AZUL, GPIO_PULLUP_ONLY);
}

bool get_estado_rojo(void)
{
    return estadoAmarillo;
}

bool get_estado_azul(void)
{
    return estadoAzul;
}

void set_estado_rojo(bool estado)
{
    estadoAmarillo = estado;
    gpio_set_level(LED_AMARILLO, estado ? 1 : 0);
    ESP_LOGI(TAG, "LED Rojo: %s", estado ? "ON" : "OFF");
}

void set_estado_azul(bool estado)
{
    estadoAzul = estado;
    gpio_set_level(LED_AZUL, estado ? 1 : 0);
    ESP_LOGI(TAG, "LED Azul: %s", estado ? "ON" : "OFF");
}

void toggle_led_rojo(void)
{
    estadoAmarillo = !estadoAmarillo;
    gpio_set_level(LED_AMARILLO, estadoAmarillo ? 1 : 0);
    ESP_LOGI(TAG, "LED Rojo: %s", estadoAmarillo ? "ON" : "OFF");
}

void toggle_led_azul(void)
{
    estadoAzul = !estadoAzul;
    gpio_set_level(LED_AZUL, estadoAzul ? 1 : 0);
    ESP_LOGI(TAG, "LED Azul: %s", estadoAzul ? "ON" : "OFF");
}

void set_estado_ambos(bool estado)
{
    estadoAmarillo = estado;
    estadoAzul = estado;
    gpio_set_level(LED_AMARILLO, estado ? 1 : 0);
    gpio_set_level(LED_AZUL, estado ? 1 : 0);
    ESP_LOGI(TAG, "Ambos LEDs: %s", estado ? "ON (ROSA)" : "OFF");
}

void button_task(void *pvParameters)
{
    while (1) {
        // Botón físico Rojo
        if (gpio_get_level(BOTON_AMARILLO) == 0) {
            if (!botonPresionadoAmarillo) {
                botonPresionadoAmarillo = true;
                toggle_led_rojo();
                ESP_LOGI(TAG, "Botón físico presionado: LED Rojo cambiado");
            }
        } else {
            botonPresionadoAmarillo = false;
        }

        // Botón físico Azul
        if (gpio_get_level(BOTON_AZUL) == 0) {
            if (!botonPresionadoAzul) {
                botonPresionadoAzul = true;
                toggle_led_azul();
                ESP_LOGI(TAG, "Botón físico presionado: LED Azul cambiado");
            }
        } else {
            botonPresionadoAzul = false;
        }

        vTaskDelay(pdMS_TO_TICKS(50)); // Delay de 50ms para debounce
    }
}

