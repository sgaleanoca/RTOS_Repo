/**
 * ============================================================================
 * ARCHIVO: terminal_commands.c
 * ============================================================================
 * 
 * RESUMEN:
 * Implementación del módulo de procesamiento de comandos de terminal.
 * Este módulo gestiona el procesamiento de comandos recibidos desde la
 * terminal web, incluyendo comandos para control de LEDs y comandos del sistema.
 * 
 * Funcionalidad:
 * - Procesa comandos de la terminal (led, status, help, clear, etc.)
 * - Ejecuta acciones correspondientes (control de GPIO)
 * - Genera respuestas apropiadas para cada comando
 * 
 * ============================================================================
 * ÍNDICE DE SECCIONES:
 * ============================================================================
 * Sección 1: INCLUDES se encuentra en las líneas 30 a 45
 * Sección 2: DEFINICIONES Y CONSTANTES se encuentra en las líneas 47 a 50
 * Sección 3: FUNCIÓN DE PROCESAMIENTO DE COMANDOS se encuentra en las líneas 52 a 100
 * Sección 4: TAREA DE PROCESAMIENTO DE COMANDOS se encuentra en las líneas 102 a 130
 * ============================================================================
 */

// ===== INCLUDES =====
// Headers locales
#include "terminal_commands.h"
#include "gpio_driver.h"

// ESP-IDF
#include <esp_log.h>

// FreeRTOS
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

// Estándar C
#include <string.h>

// ===== DEFINICIONES Y CONSTANTES =====
static const char *TAG = "TERMINAL_CMD";

// Estructura de contexto del servidor web (forward declaration)
// Esta estructura se define en web_server.c y se pasa como parámetro
typedef struct {
    QueueHandle_t gpio_command_queue;
    QueueHandle_t gpio_response_queue;
} terminal_context_t;

// ===== SECCIÓN: FUNCIÓN DE PROCESAMIENTO DE COMANDOS =====
/**
 * Procesa un comando de terminal y genera la respuesta correspondiente
 * 
 * @param cmd: Estructura con el comando a procesar (se modifica in-place con la respuesta)
 * 
 * Comandos soportados:
 * - led y on/off : Control LED amarillo
 * - led b on/off : Control LED azul
 * - led all on/off : Control ambos LEDs
 * - status : Estado de los LEDs
 * - help : Lista de comandos disponibles
 * - clear : Limpiar pantalla (manejado en frontend)
 */
void process_terminal_command(gpio_command_t *cmd) {
    if (cmd == NULL) {
        ESP_LOGE(TAG, "Comando NULL recibido");
        return;
    }
    
    // Procesar comando (mantener el command_id para la respuesta)
    if (strcmp(cmd->command, "led y on") == 0) {
        gpio_set_yellow(true);
        strcpy(cmd->response, "[OK] LED amarillo encendido.");
    } else if (strcmp(cmd->command, "led y off") == 0) {
        gpio_set_yellow(false);
        strcpy(cmd->response, "[OK] LED amarillo apagado.");
    } else if (strcmp(cmd->command, "led b on") == 0) {
        gpio_set_blue(true);
        strcpy(cmd->response, "[OK] LED azul encendido.");
    } else if (strcmp(cmd->command, "led b off") == 0) {
        gpio_set_blue(false);
        strcpy(cmd->response, "[OK] LED azul apagado.");
    } else if (strcmp(cmd->command, "led all on") == 0) {
        gpio_set_yellow(true);
        gpio_set_blue(true);
        strcpy(cmd->response, "[OK] Ambos LEDs encendidos.");
    } else if (strcmp(cmd->command, "led all off") == 0) {
        gpio_set_yellow(false);
        gpio_set_blue(false);
        strcpy(cmd->response, "[OK] Ambos LEDs apagados.");
    } else if (strcmp(cmd->command, "status") == 0) {
        const char *estadoAmarillo = gpio_get_yellow() ? "ON" : "OFF";
        const char *estadoAzul = gpio_get_blue() ? "ON" : "OFF";
        snprintf(cmd->response, sizeof(cmd->response), 
                 "Estado de los LEDs:\n  - Amarillo: %s\n  - Azul:     %s",
                 estadoAmarillo, estadoAzul);
    } else if (strcmp(cmd->command, "help") == 0) {
        strcpy(cmd->response, 
               "Comandos disponibles:\n\n"
               "  --- Control Individual ---\n"
               "  led y on          - Enciende el LED amarillo.\n"
               "  led y off         - Apaga el LED amarillo.\n"
               "  led b on          - Enciende el LED azul.\n"
               "  led b off         - Apaga el LED azul.\n\n"
               "  --- Control General ---\n"
               "  led all on        - Enciende ambos LEDs.\n"
               "  led all off       - Apaga ambos LEDs.\n\n"
               "  --- Sistema ---\n"
               "  status            - Muestra el estado de los LEDs.\n"
               "  help              - Muestra esta lista.\n"
               "  clear             - Limpia la pantalla.");
    } else {
        snprintf(cmd->response, sizeof(cmd->response), 
                 "[?] Comando no reconocido: '%s'. Escribe 'help' para ver la lista.", cmd->command);
    }
}

// ===== SECCIÓN: TAREA DE PROCESAMIENTO DE COMANDOS =====
/**
 * Tarea de FreeRTOS que procesa comandos GPIO recibidos desde la web
 * Lee comandos de la cola, los ejecuta y envía respuestas
 * 
 * @param pvParameters: Puntero al contexto del servidor web (terminal_context_t)
 */
void terminal_command_task(void *pvParameters) {
    terminal_context_t *ctx = (terminal_context_t *)pvParameters;
    gpio_command_t cmd;
    ESP_LOGI(TAG, "Tarea de procesamiento de comandos GPIO iniciada");
    
    while (1) {
        // Esperar comando de la cola (bloqueante)
        if (ctx->gpio_command_queue != NULL && 
            xQueueReceive(ctx->gpio_command_queue, &cmd, portMAX_DELAY) == pdTRUE) {
            
            // Procesar comando usando la función del módulo
            process_terminal_command(&cmd);
            
            // Enviar respuesta a la cola de respuestas (mantener el command_id)
            if (ctx->gpio_response_queue != NULL && 
                xQueueSend(ctx->gpio_response_queue, &cmd, pdMS_TO_TICKS(100)) != pdTRUE) {
                ESP_LOGW(TAG, "Error al enviar respuesta a la cola");
            }
        }
    }
}
