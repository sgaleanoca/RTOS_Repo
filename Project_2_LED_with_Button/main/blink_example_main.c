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