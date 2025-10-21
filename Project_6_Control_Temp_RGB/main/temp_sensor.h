#ifndef TEMP_SENSOR_H
#define TEMP_SENSOR_H

#include "driver/adc.h"
#include "esp_adc_cal.h"

// ADC Configuration
#define ADC_CHANNEL ADC1_CHANNEL_0  // GPIO36
#define ADC_WIDTH ADC_WIDTH_BIT_12
#define ADC_ATTEN ADC_ATTEN_DB_11
#define NUM_SAMPLES 64

// Thermistor parameters (10K NTC thermistor)
#define THERMISTOR_NOMINAL 10000.0f
#define TEMPERATURE_NOMINAL 25.0f
#define B_COEFFICIENT 3950.0f
#define SERIES_RESISTOR 10000.0f

// Function prototypes
void temp_sensor_init(void);
float temp_sensor_read(void);
float temp_sensor_read_raw(void);
float temp_sensor_convert_to_celsius(float adc_value);
void temp_sensor_task(void *pvParameters);

#endif // TEMP_SENSOR_H
