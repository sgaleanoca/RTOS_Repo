/**
 * ============================================================================
 * ARCHIVO: rgb_led.h
 * ============================================================================
 * 
 * RESUMEN:
 * Header file para el controlador de LED RGB (actualmente solo canal verde).
 * Este módulo gestiona el control PWM del LED RGB conectado al ESP32.
 * 
 * Hardware:
 * - LED Verde: GPIO 27 (PWM mediante LEDC)
 * 
 * Características:
 * - Control PWM de 8 bits (0-255)
 * - Frecuencia: 5kHz
 * - Interfaz simple con porcentaje (0-100%)
 * 
 * ============================================================================
 */

#ifndef RGB_LED_H
#define RGB_LED_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ===== PROTOTIPOS DE FUNCIONES =====

/**
 * @brief Inicializa el controlador PWM para el LED RGB verde
 * 
 * Configura el timer LEDC y el canal PWM para controlar el LED verde.
 * Debe llamarse una vez durante la inicialización del sistema.
 * 
 * @return true si la inicialización fue exitosa, false en caso contrario
 */
bool rgb_led_init(void);

/**
 * @brief Establece el brillo del LED verde como porcentaje
 * 
 * Convierte el porcentaje (0-100) a valor PWM (0-255) y actualiza el LED.
 * Valores fuera de rango se limitan automáticamente.
 * 
 * @param percent Porcentaje de brillo (0-100)
 */
void rgb_set_green_percent(uint8_t percent);

#ifdef __cplusplus
}
#endif

#endif // RGB_LED_H
