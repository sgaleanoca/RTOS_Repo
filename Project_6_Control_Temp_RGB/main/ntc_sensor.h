#ifndef NTC_SENSOR_H
#define NTC_SENSOR_H

#include <stdint.h>

// --- Configuración de Pines ---
#define NTC_PIN         ADC_CHANNEL_9   // GPIO26 es ADC_CHANNEL_9
#define LED_PIN         25              // LED rojo de temperatura en GPIO25
#define ADC_UNIT        ADC_UNIT_2

// --- Constantes del Termistor NTC 10k ---
#define NOMINAL_RESISTANCE      10000.0 // Resistencia nominal a 25°C
#define NOMINAL_TEMPERATURE     25.0    // Temperatura nominal en Celsius
#define B_COEFFICIENT           3380.0  // Coeficiente Beta del termistor (ajustado para NTC 10k)
#define SERIES_RESISTOR         10000.0 // Valor de la resistencia en serie (10k)

// --- Configuración del LED PWM ---
#define LEDC_TIMER              LEDC_TIMER_0
#define LEDC_MODE               LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL            LEDC_CHANNEL_0
#define LEDC_DUTY_RES           LEDC_TIMER_10_BIT // Resolución de 10 bits (0-1023)
#define LEDC_FREQUENCY          (5000) // Frecuencia en Hz

// --- Rango de Temperatura y Mapeo ---
#define TEMP_MIN                10.0 // Temperatura a 0% de brillo
#define TEMP_MAX                50.0 // Temperatura a 100% de brillo

// Estructura para datos del sensor NTC
typedef struct {
    float temperature_c;
    float resistance;
    int raw_adc_value;
    float brightness_percent;
    int duty_cycle;
} ntc_data_t;

// Funciones públicas
void ntc_sensor_init(void);
void ntc_led_pwm_init(void);
ntc_data_t ntc_read_temperature(void);
void ntc_update_led_brightness(float temperature_c);
void ntc_test_led(void);

#endif // NTC_SENSOR_H
