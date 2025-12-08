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
typedef struct {
    uint32_t command_id;  // ID único para emparejar comando-respuesta
    char command[100];
    char response[512];
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
 * @param pvParameters: Puntero al contexto del servidor web
 */
void terminal_command_task(void *pvParameters);

#ifdef __cplusplus
}
#endif
