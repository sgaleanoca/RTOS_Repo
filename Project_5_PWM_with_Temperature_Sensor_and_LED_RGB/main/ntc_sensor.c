#include "ntc_sensor.h"
#include "driver/adc.h"
#include "esp_adc/adc_oneshot.h"
#include <math.h>

#define NTC_ADC_CHANNEL     ADC1_CHANNEL_7 // GPIO35
#define SERIES_RESISTOR     10000.0 // Resistencia de 10kΩ en serie
#define NOMINAL_RESISTANCE  10000.0 // Resistencia nominal del NTC a 25°C
#define B_COEFFICIENT       3950.0 // Coeficiente Beta del NTC
#define NOMINAL_TEMPERATURE 25.0 // Temperatura nominal en Celsius

void ntc_sensor_init(void) {
    // El ADC1 ya fue configurado por el potenciómetro, solo configuramos el nuevo canal
    adc1_config_channel_atten(NTC_ADC_CHANNEL, ADC_ATTEN_DB_11);
}

float get_ntc_temperature(void) {
    int adc_raw = adc1_get_raw(NTC_ADC_CHANNEL);
    float voltage = (adc_raw * 3300.0) / 4095.0;

    // Calcular la resistencia del NTC a partir del voltaje
    float ntc_resistance = (voltage * SERIES_RESISTOR) / (3300.0 - voltage);
    
    // Aplicar la ecuación de Steinhart-Hart simplificada
    float steinhart;
    steinhart = ntc_resistance / NOMINAL_RESISTANCE;     // R/R0
    steinhart = log(steinhart);                          // ln(R/R0)
    steinhart /= B_COEFFICIENT;                          // (1/B) * ln(R/R0)
    steinhart += 1.0 / (NOMINAL_TEMPERATURE + 273.15);   // + 1/T0
    steinhart = 1.0 / steinhart;                         // Invertir para T en Kelvin
    steinhart -= 273.15;                                 // Convertir a Celsius

    return steinhart;
}