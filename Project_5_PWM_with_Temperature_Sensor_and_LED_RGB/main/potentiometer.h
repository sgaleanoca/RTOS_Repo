#ifndef POTENTIOMETER_H
#define POTENTIOMETER_H

// Función para inicializar el ADC para el potenciómetro
void potentiometer_init(void);

// Función para leer el voltaje en milivoltios
float get_potentiometer_voltage(void);

#endif //POTENTIOMETER_H