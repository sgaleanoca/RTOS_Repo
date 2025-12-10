/**
 * @file rgb_led.h
 * @brief Controlador de LED RGB mediante PWM
 * @author Proyecto Final RTOS
 * @date 2024
 * 
 * @details Header file para el controlador de LED RGB (actualmente solo canal verde).
 * Este módulo gestiona el control PWM del LED RGB conectado al ESP32 mediante
 * el periférico LEDC (LED Controller).
 * 
 * @section hardware Hardware
 * - LED Verde: GPIO 27 (PWM mediante LEDC Channel 1, Timer 0)
 * 
 * @section features Características
 * - Control PWM de 8 bits (0-255 niveles)
 * - Frecuencia: 5kHz (adecuada para LEDs sin parpadeo visible)
 * - Interfaz simple con porcentaje (0-100%)
 * - Validación automática de parámetros
 * 
 * ============================================================================
 * ARCHIVO: rgb_led.h
 * ============================================================================
 * 
 * RESUMEN:
 * Header file para el controlador de LED RGB (actualmente solo canal verde).
 * Este módulo gestiona el control PWM del LED RGB conectado al ESP32 mediante
 * el periférico LEDC (LED Controller).
 * 
 * Hardware:
 * - LED Verde: GPIO 27 (PWM mediante LEDC Channel 1, Timer 0)
 * 
 * Características:
 * - Control PWM de 8 bits (0-255 niveles)
 * - Frecuencia: 5kHz (adecuada para LEDs sin parpadeo visible)
 * - Interfaz simple con porcentaje (0-100%)
 * - Validación automática de parámetros
 * 
 * ============================================================================
 * ÍNDICE DE SECCIONES:
 * ============================================================================
 * Sección 1: PROTOTIPOS DE FUNCIONES se encuentra en las líneas 40 a 51
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

/**
 * @brief Inicia la tarea de control automático del LED basado en el sensor PIR
 * 
 * Crea una tarea de FreeRTOS que monitorea continuamente el sensor PIR y controla
 * el LED automáticamente:
 * - LED al 100% cuando se detecta movimiento
 * - LED apagado cuando no hay movimiento
 * 
 * Requisitos:
 * - rgb_led_init() debe haber sido llamado
 * - pir_init() debe haber sido llamado
 * 
 * La tarea se ejecuta de forma independiente y monitorea el PIR continuamente.
 */
void rgb_led_start_pir_control(void);

/**
 * @brief Verifica si el control automático por PIR está activo
 * 
 * @return true si el control PIR está activo, false en caso contrario
 */
bool rgb_led_is_pir_control_active(void);

#ifdef __cplusplus
}
#endif

#endif // RGB_LED_H
