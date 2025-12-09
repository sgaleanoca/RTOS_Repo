/**
 * ============================================================================
 * ARCHIVO: terminal_commands.h
 * ============================================================================
 * 
 * RESUMEN:
 * Header file para el módulo de procesamiento de comandos de terminal.
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
 * Sección 1: INCLUDES Y DEFINICIONES se encuentra en las líneas 30 a 40
 * Sección 2: PROTOTIPOS DE FUNCIONES se encuentra en las líneas 42 a 50
 * ============================================================================
 * 
 * ============================================================================
 * RESUMEN DE TAREAS, COLAS Y SEMÁFOROS IMPLEMENTADOS:
 * ============================================================================
 * 
 * === TAREAS (TASKS) ===
 * 
 * 1. terminal_command_task()
 *    - Definida en: terminal_commands.c
 *    - Creada en: web_server.c (start_webserver())
 *    - Nombre: "gpio_cmd_task"
 *    - Stack: 4096 bytes
 *    - Prioridad: 5
 *    - Función: Procesa comandos de terminal de forma asíncrona
 * 
 * === COLAS (QUEUES) ===
 * 
 * 1. gpio_command_queue
 *    - Tipo: QueueHandle_t
 *    - Tamaño: 10 elementos (GPIO_QUEUE_SIZE)
 *    - Elemento: gpio_command_t
 *    - Uso: Recibe comandos desde handler HTTP
 * 
 * 2. gpio_response_queue
 *    - Tipo: QueueHandle_t
 *    - Tamaño: 10 elementos (GPIO_QUEUE_SIZE)
 *    - Elemento: gpio_command_t
 *    - Uso: Envía respuestas al handler HTTP
 * 
 * === SEMÁFOROS (MUTEXES) ===
 * 
 * Ninguno en este módulo. Los semáforos se gestionan en web_server.c
 * 
 * ============================================================================
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// ===== INCLUDES =====
#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// ===== DEFINICIONES =====
// Estructura para comandos GPIO
// Esta estructura se usa para comunicar comandos entre el handler HTTP y la tarea de procesamiento
// mediante colas (queues) de FreeRTOS
typedef struct {
    uint32_t command_id;  // ID único para emparejar comando-respuesta
                        // Permite que múltiples comandos estén en proceso simultáneamente
                        // y que cada handler HTTP reciba la respuesta correcta
    char command[100];   // Texto del comando recibido (ej: "led y on", "status", "help")
    char response[512];  // Respuesta generada por process_terminal_command()
                        // Se llena después de procesar el comando
} gpio_command_t;

// ===== PROTOTIPOS DE FUNCIONES =====
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
void process_terminal_command(gpio_command_t *cmd);

/**
 * Tarea de FreeRTOS que procesa comandos GPIO recibidos desde la web
 * Lee comandos de la cola, los ejecuta y envía respuestas
 * 
 * @param pvParameters: Puntero al contexto del servidor web (terminal_context_t)
 *                      Debe contener gpio_command_queue y gpio_response_queue
 * 
 * ===== ARQUITECTURA DE TAREAS Y COLAS =====
 * Esta función implementa una tarea de FreeRTOS que procesa comandos de forma asíncrona.
 * 
 * Flujo de comunicación mediante colas:
 * 1. Handler HTTP recibe comando del cliente web
 * 2. Handler envía comando a gpio_command_queue (xQueueSend)
 * 3. Esta tarea recibe comando de gpio_command_queue (xQueueReceive, bloqueante)
 * 4. Procesa comando con process_terminal_command()
 * 5. Envía respuesta a gpio_response_queue (xQueueSend)
 * 6. Handler HTTP recibe respuesta de gpio_response_queue (xQueueReceive)
 * 7. Handler envía respuesta al cliente web
 * 
 * Ventajas:
 * - El servidor HTTP no se bloquea esperando procesamiento
 * - Múltiples comandos pueden estar en cola simultáneamente
 * - Thread-safe gracias a las colas de FreeRTOS
 * - Permite manejar picos de tráfico sin perder comandos
 * 
 * Configuración (en web_server.c):
 * - Stack: 4096 bytes
 * - Prioridad: 5 (alta, para procesamiento rápido)
 * - Nombre: "gpio_cmd_task"
 */
void terminal_command_task(void *pvParameters);

#ifdef __cplusplus
}
#endif
