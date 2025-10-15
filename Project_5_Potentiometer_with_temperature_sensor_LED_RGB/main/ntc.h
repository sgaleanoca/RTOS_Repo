#ifndef NTC_H
#define NTC_H

#include <stdint.h>

// Configuración del termistor NTC 10k
#define NTC_ADC_CHANNEL     ADC_CHANNEL_7  // GPIO35
#define NTC_REF_RESISTOR    10000           // Resistencia de referencia 10kΩ
#define NTC_NOMINAL_TEMP    25              // Temperatura nominal 25°C
#define NTC_NOMINAL_RES     10000           // Resistencia nominal 10kΩ
#define NTC_BETA_COEFF      3950            // Coeficiente Beta típico para NTC 10k

// Rango de temperatura para el porcentaje (10°C = 0%, 50°C = 100%)
#define TEMP_MIN_CELSIUS    10.0
#define TEMP_MAX_CELSIUS    50.0

/**
 * @brief Inicializa el ADC para el termistor NTC
 */
void ntc_init(void);

/**
 * @brief Lee la temperatura actual del termistor
 * @return Temperatura en grados Celsius
 */
float ntc_get_temperature_celsius(void);

/**
 * @brief Convierte la temperatura a porcentaje (10°C = 0%, 50°C = 100%)
 * @param temp_celsius Temperatura en grados Celsius
 * @return Porcentaje de 0 a 100
 */
uint8_t ntc_temp_to_percent(float temp_celsius);

/**
 * @brief Obtiene la resistencia del termistor en ohmios
 * @return Resistencia en ohmios
 */
float ntc_get_resistance(void);

#endif // NTC_H
