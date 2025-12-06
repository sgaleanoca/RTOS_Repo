/**
 * ============================================================================
 * ARCHIVO: gpio_driver.h
 * ============================================================================
 * 
 * RESUMEN:
 * Header file para el controlador de GPIO. Define la interfaz pública para
 * controlar los LEDs del sistema (amarillo y azul) de forma thread-safe.
 * 
 * Funciones disponibles:
 * - Inicialización de pines GPIO
 * - Control de LEDs (encender/apagar)
 * - Lectura de estado de LEDs
 * ============================================================================
 */

#pragma once
#include <stdbool.h>

// ===== PROTOTIPOS DE FUNCIONES =====

// --- Inicialización ---
/**
 * Inicializa los pines GPIO para los LEDs y crea el mutex de protección
 * Debe llamarse una vez al inicio del programa
 */
void gpio_init_leds(void);

// --- Control de LED Amarillo (GPIO 2) ---
/**
 * Establece el estado del LED amarillo
 * @param state: true para encender, false para apagar
 */
void gpio_set_yellow(bool state);

/**
 * Lee el estado actual del LED amarillo
 * @return true si está encendido, false si está apagado
 */
bool gpio_get_yellow(void);

// --- Control de LED Azul (GPIO 5) ---
/**
 * Establece el estado del LED azul
 * @param state: true para encender, false para apagar
 */
void gpio_set_blue(bool state);

/**
 * Lee el estado actual del LED azul
 * @return true si está encendido, false si está apagado
 */
bool gpio_get_blue(void);
