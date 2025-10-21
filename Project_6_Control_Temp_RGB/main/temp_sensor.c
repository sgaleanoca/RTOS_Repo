#include "temp_sensor.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_adc_cal.h"
#include <math.h>

static const char *TAG = "TEMP_SENSOR";

static esp_adc_cal_characteristics_t *adc_chars;
static float last_temperature = 25.0f;

void temp_sensor_init(void) {
    // Configure ADC
    adc1_config_width(ADC_WIDTH);
    adc1_config_channel_atten(ADC_CHANNEL, ADC_ATTEN);
    
    // Characterize ADC
    adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN, ADC_WIDTH, 1100, adc_chars);
    
    ESP_LOGI(TAG, "Temperature sensor initialized");
}

float temp_sensor_read(void) {
    uint32_t adc_reading = 0;
    
    // Multisampling
    for (int i = 0; i < NUM_SAMPLES; i++) {
        adc_reading += adc1_get_raw(ADC_CHANNEL);
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    adc_reading /= NUM_SAMPLES;
    
    // Convert ADC reading to voltage
    uint32_t voltage = esp_adc_cal_raw_to_voltage(adc_reading, adc_chars);
    
    // Convert voltage to temperature
    float temperature = temp_sensor_convert_to_celsius(voltage);
    last_temperature = temperature;
    
    return temperature;
}

float temp_sensor_read_raw(void) {
    return adc1_get_raw(ADC_CHANNEL);
}

float temp_sensor_convert_to_celsius(float adc_value) {
    // Convert ADC value to resistance
    float resistance = SERIES_RESISTOR * (4095.0f / adc_value - 1.0f);
    
    // Calculate temperature using Steinhart-Hart equation
    float steinhart = log(resistance / THERMISTOR_NOMINAL) / B_COEFFICIENT;
    steinhart += 1.0f / (TEMPERATURE_NOMINAL + 273.15f);
    steinhart = 1.0f / steinhart;
    steinhart -= 273.15f;
    
    return steinhart;
}

void temp_sensor_task(void *pvParameters) {
    while (1) {
        float temperature = temp_sensor_read();
        ESP_LOGI(TAG, "Temperature: %.2f°C", temperature);
        vTaskDelay(pdMS_TO_TICKS(1000)); // Read every second
    }
}
