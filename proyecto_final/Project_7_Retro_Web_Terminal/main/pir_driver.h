/**
 * ============================================================================
 * ARCHIVO: pir_driver.h
 * ============================================================================
 * 
 * RESUMEN:
 * Header file para el driver del sensor PIR (Passive Infrared Sensor).
 * Este módulo gestiona la detección de movimiento mediante interrupciones GPIO.
 * 
 * Hardware:
 * - Sensor PIR: GPIO 12 (configurable mediante PIR_GPIO_PIN)
 * - El sensor PIR detecta movimiento mediante cambios en radiación infrarroja
 * 
 * Características:
 * - Detección de movimiento mediante interrupciones GPIO (flanco de subida/bajada)
 * - Soporte para cola de eventos opcional para notificaciones asíncronas
 * - Lectura síncrona del estado actual del sensor
 * - ISR (Interrupt Service Routine) thread-safe usando colas desde ISR
 * - Configuración automática de pull-up/pull-down (deshabilitados, el módulo PIR ya los tiene)
 * 
 * Uso en el sistema:
 * - El ventilador verifica presencia mediante pir_is_motion_active()
 * - En modo MANUAL, el ventilador ignora el PIR
 * - En otros modos, el ventilador solo funciona si hay presencia detectada
 * 
 * ============================================================================
 * ÍNDICE DE SECCIONES:
 * ============================================================================
 * Sección 1: CONFIGURACIÓN DE PIN GPIO se encuentra en las líneas 35 a 40
 * Sección 2: ESTRUCTURAS DE DATOS se encuentra en las líneas 42 a 51
 * Sección 3: PROTOTIPOS DE FUNCIONES se encuentra en las líneas 53 a 94
 * ============================================================================
 */

#ifndef PIR_DRIVER_H
#define PIR_DRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

// ===== CONFIGURACIÓN DE PIN GPIO =====
/**
 * Pin GPIO donde está conectado el sensor PIR
 * Cambia este valor según tu conexión física
 */
#define PIR_GPIO_PIN 12  // GPIO 12 - Ajusta según tu hardware

// ===== ESTRUCTURAS DE DATOS =====
/**
 * @brief Estructura de evento del sensor PIR
 * 
 * Esta estructura se envía a través de la cola de eventos cuando se detecta
 * un cambio en el estado del sensor PIR (movimiento detectado o no detectado).
 */
typedef struct {
    bool motion;  ///< true si hay movimiento detectado, false si no hay movimiento
} pir_event_t;

// ===== PROTOTIPOS DE FUNCIONES =====

/**
 * @brief Inicializa el driver del sensor PIR
 * 
 * Configura el pin GPIO como entrada con interrupciones en ambos flancos
 * (subida y bajada) para detectar cambios en el estado del sensor.
 * 
 * Si se proporciona una cola de eventos, los eventos de movimiento se enviarán
 * automáticamente a través de la cola cuando se detecten cambios. Esto permite
 * procesamiento asíncrono de eventos de movimiento en una tarea separada.
 * 
 * Proceso de inicialización:
 * 1. Configura el pin GPIO como entrada
 * 2. Instala el servicio de ISR de GPIO (si no está instalado)
 * 3. Registra el handler de interrupción para el pin
 * 
 * @param pir_gpio Número del pin GPIO donde está conectado el sensor PIR
 * @param pir_queue Cola de eventos donde se enviarán las notificaciones de movimiento.
 *                  Puede ser NULL si no se necesita notificación asíncrona.
 *                  La cola debe ser creada previamente con xQueueCreate().
 * 
 * @return true si la inicialización fue exitosa, false en caso contrario
 * 
 * @note La cola de eventos debe ser creada antes de llamar a esta función.
 *       Ejemplo: QueueHandle_t pir_queue = xQueueCreate(10, sizeof(pir_event_t));
 */
bool pir_init(gpio_num_t pir_gpio, QueueHandle_t pir_queue);

/**
 * @brief Lee el estado actual del sensor PIR
 * 
 * Lee directamente el nivel del pin GPIO para determinar si hay movimiento
 * detectado. Esta función es síncrona y puede ser llamada desde cualquier tarea.
 * 
 * @return true si hay movimiento detectado (GPIO en nivel alto), 
 *         false si no hay movimiento (GPIO en nivel bajo)
 * 
 * @note Esta función lee el estado actual del GPIO, no el último evento.
 *       Para recibir notificaciones de cambios, usar la cola de eventos en pir_init().
 */
bool pir_is_motion_active(void);

#ifdef __cplusplus
}
#endif

#endif // PIR_DRIVER_H
