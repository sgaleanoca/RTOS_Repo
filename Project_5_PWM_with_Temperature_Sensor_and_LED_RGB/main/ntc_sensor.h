#ifndef NTC_SENSOR_H
#define NTC_SENSOR_H

// Función para inicializar el ADC para el termistor
void ntc_sensor_init(void);

// Función para leer la temperatura en grados Celsius
float get_ntc_temperature(void);

#endif //NTC_SENSOR_H