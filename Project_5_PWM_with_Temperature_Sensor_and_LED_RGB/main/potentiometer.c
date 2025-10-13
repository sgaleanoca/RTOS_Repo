#include "potentiometer.h"
#include "driver/adc.h"

#define POT_ADC_CHANNEL ADC1_CHANNEL_6 // GPIO34

void potentiometer_init(void) {
    // Configurar el ADC1 con una resolución de 12 bits (0-4095)
    adc1_config_width(ADC_WIDTH_BIT_12);
    // Configurar la atenuación del canal para poder leer el rango completo de 0 a 3.3V
    adc1_config_channel_atten(POT_ADC_CHANNEL, ADC_ATTEN_DB_11);
}

float get_potentiometer_voltage(void) {
    // Leer el valor crudo del ADC
    int adc_raw = adc1_get_raw(POT_ADC_CHANNEL);
    // Convertir el valor crudo a milivoltios (mV)
    float voltage = (adc_raw * 3300.0) / 4095.0;
    return voltage;
}