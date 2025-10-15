#ifndef POTENTIOMETER_H
#define POTENTIOMETER_H

#include <stdint.h>

void pot_init(void);
uint8_t pot_get_percent(void); // devuelve 0..100
uint32_t pot_get_voltage_mv(void); // devuelve mV medido

// Función para obtener el handle del ADC1 (para uso compartido)
void* pot_get_adc_handle(void);

#endif // POTENTIOMETER_H
