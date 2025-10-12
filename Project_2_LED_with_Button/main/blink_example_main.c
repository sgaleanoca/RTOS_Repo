#include <stdio.h>                   // Librería estándar para entrada/salida (printf, etc.)
#include "freertos/FreeRTOS.h"       // Núcleo de FreeRTOS (multitarea en ESP-IDF)
#include "freertos/task.h"           // Funciones para crear y manejar tareas
#include "freertos/semphr.h"         // Librería de semáforos de FreeRTOS
#include "driver/gpio.h"             // Control de pines GPIO
#include "esp_log.h"                 // Logs en consola
#include "sdkconfig.h"               // Configuración generada por menuconfig

static const char *TAG = "LED";      // Etiqueta para identificar logs

/* LED integrado (GPIO2 en el ESP32-WROOM por defecto) */
#define BLINK_GPIO CONFIG_BLINK_GPIO // Pin del LED, configurable en menuconfig

/* Botón físico conectado al GPIO19 */
#define BUTTON_GPIO 19               // Pin del botón

SemaphoreHandle_t xSemaphore;        // Semáforo binario para sincronizar botón y LED


/* ================= TAREA: BOTÓN ================= */
void button_task(void *pvParameter) {
    int last_button = 1;             // Último estado del botón (1 = no presionado, 0 = presionado)

    while (1) {
        int button_state = gpio_get_level(BUTTON_GPIO); // Leer estado actual del botón

        if (button_state == 0 && last_button == 1) {    // Detecta flanco descendente (botón presionado)
            xSemaphoreGive(xSemaphore);                 // Libera el semáforo → avisa a la tarea LED
            vTaskDelay(200 / portTICK_PERIOD_MS);       // Delay anti-rebote (200 ms)
        }

        last_button = button_state;                     // Actualiza el estado anterior
        vTaskDelay(10 / portTICK_PERIOD_MS);            // Revisa el botón cada 10 ms
    }
}

/* ================= TAREA: LED ================= */
void led_task(void *pvParameter) {
    int parpadeo_activo = 1;         // Controla si el LED debe parpadear (1 = sí, 0 = no)
    int led_state = 0;               // Estado del LED (0 = apagado, 1 = encendido)

    while (1) {
        if (xSemaphoreTake(xSemaphore, 0) == pdTRUE) {  // Revisa si el botón liberó el semáforo
            parpadeo_activo = !parpadeo_activo;         // Cambia modo (on/off parpadeo)

            ESP_LOGI(TAG, "Modo: %s", parpadeo_activo ? "Parpadeo" : "Apagado");

            if (!parpadeo_activo) {                     // Si se apagó el parpadeo
                gpio_set_level(BLINK_GPIO, 0);          // Asegura LED apagado
                led_state = 0;                          // Actualiza estado interno
            }
        }

        if (parpadeo_activo) {                          // Si el parpadeo está activo
            led_state = !led_state;                     // Cambia estado del LED
            gpio_set_level(BLINK_GPIO, led_state);      // Aplica el nuevo estado al pin
            vTaskDelay(500 / portTICK_PERIOD_MS);       // Espera 500 ms (medio segundo)
        } else {
            vTaskDelay(100 / portTICK_PERIOD_MS);       // Si no parpadea, espera más (ahorra CPU)
        }
    }
}