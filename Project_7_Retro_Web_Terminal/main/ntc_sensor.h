/**
 * ============================================================================
 * ARCHIVO: ntc_sensor.h
 * ============================================================================
 * 
 * RESUMEN:
 * Header file para el módulo de sensor de temperatura NTC (Negative Temperature
 * Coefficient). Este módulo gestiona la lectura de temperatura usando un
 * termistor NTC 10k conectado a través de un divisor de voltaje al ADC del ESP32.
 * 
 * Hardware:
 * - Sensor: NTC 10k termistor
 * - Pin: GPIO32 (ADC1 Channel 4)
 * - Nota: ADC2 no funciona cuando WiFi está activo, por eso usamos ADC1
 * 
 * Características:
 * - Lectura periódica en tarea separada de FreeRTOS
 * - Cálculo de temperatura usando ecuación de Steinhart-Hart
 * - Protección thread-safe con mutex para acceso a datos
 * ============================================================================
 */

#ifndef NTC_SENSOR_H
#define NTC_SENSOR_H

#include <stdint.h>

// ===== CONFIGURACIÓN DE PINES Y ADC =====
// Nota: ADC2 no funciona con WiFi activo, por lo que usamos ADC1
// GPIO32 es ADC_CHANNEL_4 (ADC1) y funciona correctamente con WiFi
#define NTC_PIN         ADC_CHANNEL_4   // GPIO32 es ADC_CHANNEL_4 (ADC1)
#define ADC_UNIT        ADC_UNIT_1

// ===== CONSTANTES DEL TERMISTOR NTC 10k =====
#define NOMINAL_RESISTANCE      10000.0 // Resistencia nominal a 25°C (en ohmios)
#define NOMINAL_TEMPERATURE     25.0    // Temperatura nominal en Celsius
#define B_COEFFICIENT           3380.0  // Coeficiente Beta del termistor (ajustado para NTC 10k)
#define SERIES_RESISTOR         10000.0 // Valor de la resistencia en serie (10k ohmios)

// ===== ESTRUCTURAS DE DATOS =====
/**
 * Estructura que contiene los datos del sensor NTC
 */
typedef struct {
    float temperature_c;    // Temperatura calculada en grados Celsius
    float resistance;       // Resistencia calculada del NTC en ohmios
    int raw_adc_value;      // Valor crudo leído del ADC (0-4095)
} ntc_data_t;

// ===== PROTOTIPOS DE FUNCIONES =====

// Inicialización
void ntc_sensor_init(void);

// Lectura de temperatura
ntc_data_t ntc_read_temperature(void);
ntc_data_t ntc_get_current_temperature(void);

// Gestión de tarea de lectura periódica
void ntc_start_reading_task(void);

#endif // NTC_SENSOR_H
