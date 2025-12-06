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

// Inicialización
void gpio_init_leds(void);

// Control de LED Amarillo (GPIO 2)
void gpio_set_yellow(bool state);
bool gpio_get_yellow(void);

// Control de LED Azul (GPIO 5)
void gpio_set_blue(bool state);
bool gpio_get_blue(void);
