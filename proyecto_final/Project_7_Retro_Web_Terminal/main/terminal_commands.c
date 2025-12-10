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
 * 
 * ============================================================================
 * RESUMEN DE TAREAS, COLAS Y SEMÁFOROS IMPLEMENTADOS:
 * ============================================================================
 * 
 * === TAREAS (TASKS) ===
 * 
 * 1. terminal_command_task (Sección 4)
 *    - Nombre: "gpio_cmd_task" (configurado en web_server.c)
 *    - Stack: 4096 bytes
 *    - Prioridad: 5 (alta)
 *    - Función: Procesa comandos GPIO recibidos desde la terminal web
 *    - Propósito: Ejecutar comandos de forma asíncrona sin bloquear el servidor HTTP
 *    - Estado: Loop infinito, bloquea esperando comandos (portMAX_DELAY)
 *    - Flujo:
 *      1. Recibe comando de gpio_command_queue (xQueueReceive, bloqueante)
 *      2. Procesa comando con process_terminal_command()
 *      3. Envía respuesta a gpio_response_queue (xQueueSend)
 * 
 * === COLAS (QUEUES) ===
 * 
 * 1. gpio_command_queue (recibida en contexto)
 *    - Tipo: QueueHandle_t
 *    - Tamaño: GPIO_QUEUE_SIZE (10 elementos)
 *    - Elemento: gpio_command_t
 *    - Dirección: Handler HTTP → Esta tarea
 *    - Operación: xQueueReceive() con portMAX_DELAY (bloqueante)
 *    - Uso: Esta tarea recibe comandos de la cola para procesarlos
 * 
 * 2. gpio_response_queue (recibida en contexto)
 *    - Tipo: QueueHandle_t
 *    - Tamaño: GPIO_QUEUE_SIZE (10 elementos)
 *    - Elemento: gpio_command_t (con respuesta generada)
 *    - Dirección: Esta tarea → Handler HTTP
 *    - Operación: xQueueSend() con timeout de 100ms
 *    - Uso: Esta tarea envía respuestas procesadas a la cola
 * 
 * === SEMÁFOROS (MUTEXES) ===
 * 
 * Ninguno en este módulo. Los semáforos se gestionan en web_server.c
 * 
 * ============================================================================
 * ARQUITECTURA:
 * ============================================================================
 * 
 * Esta tarea implementa el patrón Producer-Consumer:
 * - Producer: Handler HTTP /cmd (envía comandos a gpio_command_queue)
 * - Consumer: terminal_command_task (recibe y procesa comandos)
 * - Producer: terminal_command_task (envía respuestas a gpio_response_queue)
 * - Consumer: Handler HTTP /cmd (recibe respuestas)
 * 
 * Ventajas:
 * - Servidor HTTP no se bloquea esperando procesamiento
 * - Múltiples comandos pueden estar en cola simultáneamente
 * - Thread-safe gracias a las colas de FreeRTOS
 * - Permite manejar picos de tráfico sin perder comandos
 * 
 * ============================================================================
 */

// ===== INCLUDES =====
// Headers locales
#include "terminal_commands.h"
#include "rgb_led.h"
#include "fan_control.h"

// ESP-IDF
#include <esp_log.h>

// FreeRTOS
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

// Estándar C
#include <string.h>
#include <stdlib.h>

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
 * - led on : Enciende el LED RGB verde (100% brillo)
 * - led off : Apaga el LED RGB verde (0% brillo)
 * - led <0-100> : Establece el brillo del LED RGB verde (0-100%)
 * - status : Estado del LED RGB verde
 * - help : Lista de comandos disponibles
 * - clear : Limpiar pantalla (manejado en frontend)
 */
void process_terminal_command(gpio_command_t *cmd) {
    if (!cmd) {
        ESP_LOGE(TAG, "Comando NULL recibido");
        return;
    }
    
    // Comando "led <número>" para establecer brillo (0-100)
    if (strncmp(cmd->command, "led ", 4) == 0) {
        int brightness = atoi(cmd->command + 4);
        if (brightness >= 0 && brightness <= 100) {
            rgb_set_green_percent((uint8_t)brightness);
            snprintf(cmd->response, sizeof(cmd->response), 
                     "[OK] LED RGB verde establecido a %d%% de brillo.", brightness);
        } else {
            snprintf(cmd->response, sizeof(cmd->response), "[ERROR] El brillo debe estar entre 0 y 100.");
        }
        return;
    }
    
    // Comando "fan <número>" para establecer velocidad (0-100)
    if (strncmp(cmd->command, "fan ", 4) == 0) {
        int percent = atoi(cmd->command + 4);
        if (percent >= 0 && percent <= 100) {
            fan_set_mode(FAN_MODE_MANUAL);
            fan_set_manual_percent((uint8_t)percent);
            snprintf(cmd->response, sizeof(cmd->response), 
                     "[OK] Ventilador establecido a %d%% de velocidad.", percent);
        } else {
            snprintf(cmd->response, sizeof(cmd->response), "[ERROR] El porcentaje debe estar entre 0 y 100.");
        }
        return;
    }
    
    // Comandos simples
    if (strcmp(cmd->command, "led on") == 0) {
        rgb_set_green_percent(100);
        snprintf(cmd->response, sizeof(cmd->response), "[OK] LED RGB verde encendido (100%%).");
    } else if (strcmp(cmd->command, "led off") == 0) {
        rgb_set_green_percent(0);
        snprintf(cmd->response, sizeof(cmd->response), "[OK] LED RGB verde apagado.");
    } else if (strcmp(cmd->command, "fan on") == 0) {
        fan_set_mode(FAN_MODE_MANUAL);
        fan_set_manual_percent(50);
        snprintf(cmd->response, sizeof(cmd->response), "[OK] Ventilador encendido al 50%% de velocidad.");
    } else if (strcmp(cmd->command, "fan off") == 0) {
        fan_set_mode(FAN_MODE_OFF);
        snprintf(cmd->response, sizeof(cmd->response), "[OK] Ventilador apagado.");
    } else if (strcmp(cmd->command, "status") == 0) {
        uint8_t fan_percent = fan_get_current_percent();
        fan_mode_t fan_mode = fan_get_mode();
        const char *fan_mode_str;
        switch (fan_mode) {
            case FAN_MODE_OFF: fan_mode_str = "OFF"; break;
            case FAN_MODE_MANUAL: fan_mode_str = "MANUAL"; break;
            case FAN_MODE_AUTO_TEMP: fan_mode_str = "AUTO_TEMP"; break;
            case FAN_MODE_SCHEDULE: fan_mode_str = "SCHEDULE"; break;
            default: fan_mode_str = "UNKNOWN"; break;
        }
        snprintf(cmd->response, sizeof(cmd->response), 
                 "Estado del sistema:\n\n"
                 "  --- LED RGB ---\n"
                 "  - LED Verde: Configurado (GPIO 27, PWM)\n"
                 "  - Control: 'led on/off/<0-100>'\n\n"
                 "  --- Ventilador ---\n"
                 "  - Modo: %s\n"
                 "  - Velocidad: %d%%\n"
                 "  - Control: 'fan on/off/<0-100>'",
                 fan_mode_str, fan_percent);
    } else if (strcmp(cmd->command, "help") == 0) {
        snprintf(cmd->response, sizeof(cmd->response),
                 "Comandos disponibles:\n\n"
                 "  --- LED RGB ---\n"
                 "  led on        - Enciende LED (100%%).\n"
                 "  led off       - Apaga LED.\n"
                 "  led <0-100>   - Brillo LED (0-100%%).\n\n"
                 "  --- Ventilador ---\n"
                 "  fan on        - Enciende al 0%%.\n"
                 "  fan off       - Apaga.\n"
                 "  fan <0-100>   - Velocidad (0-100%%).\n\n"
                 "  --- Sistema ---\n"
                 "  status        - Estado.\n"
                 "  help          - Esta lista.\n"
                 "  clear         - Limpia pantalla.");
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
 * 
 * ===== EXPLICACIÓN DE LA TAREA =====
 * Esta es una tarea de FreeRTOS que se ejecuta de forma independiente
 * para procesar comandos de la terminal web de manera asíncrona.
 * 
 * Arquitectura:
 * 1. El handler HTTP (/cmd) recibe un comando del cliente web
 * 2. El handler envía el comando a gpio_command_queue (no bloquea)
 * 3. Esta tarea recibe el comando de la cola (bloqueante)
 * 4. Procesa el comando usando process_terminal_command()
 * 5. Envía la respuesta a gpio_response_queue
 * 6. El handler HTTP recibe la respuesta y la envía al cliente
 * 
 * Ventajas de esta arquitectura:
 * - El servidor HTTP no se bloquea esperando procesamiento
 * - Múltiples comandos pueden estar en cola simultáneamente
 * - El procesamiento es thread-safe gracias a las colas
 * - Permite manejar picos de tráfico sin perder comandos
 * 
 * ===== USO DE COLAS =====
 * gpio_command_queue (entrada):
 *   - xQueueReceive() espera indefinidamente (portMAX_DELAY) hasta recibir un comando
 *   - Cuando recibe un comando, lo copia a la variable local 'cmd'
 *   - La cola es thread-safe: múltiples handlers HTTP pueden enviar simultáneamente
 * 
 * gpio_response_queue (salida):
 *   - xQueueSend() envía la respuesta con el mismo command_id
 *   - Timeout de 100ms: si la cola está llena, espera máximo 100ms
 *   - Si falla, registra un warning pero continúa (el handler HTTP manejará el timeout)
 * 
 * ===== FLUJO DE DATOS =====
 * Cliente Web -> Handler HTTP -> gpio_command_queue -> Esta Tarea -> 
 * process_terminal_command() -> gpio_response_queue -> Handler HTTP -> Cliente Web
 * 
 * Prioridad: 5 (configurada en web_server.c)
 * Stack: 4096 bytes (suficiente para procesamiento de comandos)
 */
void terminal_command_task(void *pvParameters) {
    terminal_context_t *ctx = (terminal_context_t *)pvParameters;
    gpio_command_t cmd;
    ESP_LOGI(TAG, "Tarea de procesamiento de comandos GPIO iniciada");
    
    // Loop infinito: la tarea se ejecuta continuamente
    while (1) {
        // ===== RECEPCIÓN DE COMANDO DE LA COLA =====
        // xQueueReceive() es bloqueante: espera hasta que haya un comando disponible
        // portMAX_DELAY: espera indefinidamente (no hay timeout)
        // pdTRUE: indica que se recibió un elemento correctamente
        // La cola gpio_command_queue es llenada por el handler HTTP /cmd
        if (ctx->gpio_command_queue != NULL && 
            xQueueReceive(ctx->gpio_command_queue, &cmd, portMAX_DELAY) == pdTRUE) {
            
            // ===== PROCESAMIENTO DEL COMANDO =====
            // process_terminal_command() modifica cmd->response in-place
            // Ejecuta la acción correspondiente (control de LEDs, status, help, etc.)
            process_terminal_command(&cmd);
            
            // ===== ENVÍO DE RESPUESTA A LA COLA =====
            // Enviar la respuesta a gpio_response_queue para que el handler HTTP la reciba
            // El command_id se mantiene para que el handler pueda emparejar comando-respuesta
            // xQueueSend() es thread-safe y bloquea si la cola está llena
            // Timeout de 100ms: si la cola está llena, espera máximo 100ms
            if (ctx->gpio_response_queue != NULL && 
                xQueueSend(ctx->gpio_response_queue, &cmd, pdMS_TO_TICKS(100)) != pdTRUE) {
                // Si falla el envío, registrar warning pero continuar
                // El handler HTTP manejará el timeout y enviará un error al cliente
                ESP_LOGW(TAG, "Error al enviar respuesta a la cola");
            }
        }
    }
}
