/**
 * ============================================================================
 * ARCHIVO: gpio_driver.h
 * ============================================================================
 * 
 * RESUMEN:
 * Header file para el controlador de GPIO. Define la interfaz pública para
 * controlar los LEDs del sistema (amarillo y azul) de forma thread-safe.
 * 
 * Hardware:
 * - LED Amarillo: GPIO 2
 * - LED Azul: GPIO 5
 * 
 * Características:
 * - Protección thread-safe con mutex para acceso concurrente
 * - Funciones para leer y escribir estado de LEDs
 * - Inicialización segura de pines GPIO
 * - Validación de parámetros y manejo de errores
 * 
 * ============================================================================
 */

#ifndef GPIO_DRIVER_H
#define GPIO_DRIVER_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ===== PROTOTIPOS DE FUNCIONES =====

/**
 * @brief Inicializa los pines GPIO para los LEDs y crea el mutex de protección
 * 
 * Configura los pines GPIO 2 (LED amarillo) y GPIO 5 (LED azul) como
 * entrada/salida y crea un mutex para proteger el acceso concurrente desde
 * múltiples tareas.
 * 
 * Esta función debe llamarse una vez al inicio del programa antes de usar
 * cualquier otra función de este módulo.
 * 
 * @return true si la inicialización fue exitosa, false en caso contrario
 */
bool gpio_init_leds(void);

/**
 * @brief Establece el estado del LED amarillo
 * 
 * Función thread-safe que protege el acceso al GPIO con mutex.
 * Puede ser llamada desde cualquier tarea de forma segura.
 * 
 * @param state true para encender, false para apagar
 */
void gpio_set_yellow(bool state);

/**
 * @brief Lee el estado actual del LED amarillo
 * 
 * Función thread-safe que protege el acceso al GPIO con mutex.
 * Puede ser llamada desde cualquier tarea de forma segura.
 * 
 * @return true si está encendido, false si está apagado o si el driver no está inicializado
 */
bool gpio_get_yellow(void);

/**
 * @brief Establece el estado del LED azul
 * 
 * Función thread-safe que protege el acceso al GPIO con mutex.
 * Puede ser llamada desde cualquier tarea de forma segura.
 * 
 * @param state true para encender, false para apagar
 */
void gpio_set_blue(bool state);

/**
 * @brief Lee el estado actual del LED azul
 * 
 * Función thread-safe que protege el acceso al GPIO con mutex.
 * Puede ser llamada desde cualquier tarea de forma segura.
 * 
 * @return true si está encendido, false si está apagado o si el driver no está inicializado
 */
bool gpio_get_blue(void);

#ifdef __cplusplus
}
#endif

#endif // GPIO_DRIVER_H
