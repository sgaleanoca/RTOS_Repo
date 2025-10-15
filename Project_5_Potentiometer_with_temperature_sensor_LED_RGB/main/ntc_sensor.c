#include "ntc_sensor.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>

static const char *TAG = "NTC_TEMP_CONTROL";

// Variables globales para ADC
static adc_oneshot_unit_handle_t adc2_handle;
static adc_cali_handle_t adc2_cali_handle = NULL;

// Función para inicializar la calibración del ADC
static bool adc_calibration_init(adc_unit_t unit, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "Calibración por curve fitting");
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "Calibración por line fitting");
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

    *out_handle = handle;
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Calibración exitosa");
    } else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated) {
        ESP_LOGW(TAG, "Calibración no soportada, use valores sin calibrar");
    } else {
        ESP_LOGE(TAG, "Fallo en la calibración");
    }

    return calibrated;
}

// Función para inicializar el ADC
void ntc_sensor_init(void) {
    // Configurar ADC2
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc2_handle));

    // Configurar canal
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc2_handle, NTC_PIN, &config));

    // Inicializar calibración
    adc_calibration_init(ADC_UNIT, ADC_ATTEN_DB_12, &adc2_cali_handle);
    
    ESP_LOGI(TAG, "Inicialización de ADC2 en GPIO26 completada.");
}

// Función para inicializar el control PWM del LED
void ntc_led_pwm_init(void) {
    // Configurar el timer
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_MODE,
        .timer_num        = LEDC_TIMER,
        .duty_resolution  = LEDC_DUTY_RES,
        .freq_hz          = LEDC_FREQUENCY,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // Configurar el canal
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL,
        .timer_sel      = LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = LED_PIN,
        .duty           = 0, // Iniciar con el LED apagado
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
    ESP_LOGI(TAG, "Inicialización de LED PWM completada en GPIO %d", LED_PIN);
}

// Función para leer la temperatura del sensor NTC
ntc_data_t ntc_read_temperature(void) {
    ntc_data_t ntc_data = {0};
    int raw_adc_value;
    esp_err_t result = adc_oneshot_read(adc2_handle, NTC_PIN, &raw_adc_value);

    if (result == ESP_OK) {
        ntc_data.raw_adc_value = raw_adc_value;
        
        // --- Cálculo de la Temperatura (Ecuación de Steinhart-Hart simplificada) ---
        float resistance = SERIES_RESISTOR * ((4095.0 / raw_adc_value) - 1.0);
        ntc_data.resistance = resistance;

        float steinhart;
        steinhart = resistance / NOMINAL_RESISTANCE;     // (R/R0)
        steinhart = log(steinhart);                      // ln(R/R0)
        steinhart /= B_COEFFICIENT;                      // 1/B * ln(R/R0)
        steinhart += 1.0 / (NOMINAL_TEMPERATURE + 273.15); // + 1/T0
        steinhart = 1.0 / steinhart;                     // Invertir
        float temperature_c = steinhart - 273.15;        // Convertir a Celsius
        ntc_data.temperature_c = temperature_c;

        // --- Control del LED basado en la temperatura ---
        int duty_cycle = 0;
        float brightness_percent = 0.0;
        
        if (temperature_c <= TEMP_MIN) {
            duty_cycle = 0; // Por debajo del mínimo, el LED está apagado
            brightness_percent = 0.0;
        } else if (temperature_c >= TEMP_MAX) {
            duty_cycle = (1 << LEDC_DUTY_RES) - 1; // Por encima del máximo, el LED está al 100% (1023)
            brightness_percent = 100.0;
        } else {
            // Mapear el rango de temperatura al rango de PWM (0 a 1023)
            // Fórmula: (temp - temp_min) / (temp_max - temp_min) * 100
            brightness_percent = ((temperature_c - TEMP_MIN) / (TEMP_MAX - TEMP_MIN)) * 100.0;
            duty_cycle = (int)(brightness_percent * ((1 << LEDC_DUTY_RES) - 1) / 100.0);
        }
        
        ntc_data.brightness_percent = brightness_percent;
        ntc_data.duty_cycle = duty_cycle;

    } else {
        ESP_LOGE(TAG, "Error al leer ADC2: %s", esp_err_to_name(result));
    }

    return ntc_data;
}

// Función para actualizar el brillo del LED basado en la temperatura
void ntc_update_led_brightness(float temperature_c) {
    int duty_cycle = 0;
    
    if (temperature_c <= TEMP_MIN) {
        duty_cycle = 0; // Por debajo del mínimo, el LED está apagado
    } else if (temperature_c >= TEMP_MAX) {
        duty_cycle = (1 << LEDC_DUTY_RES) - 1; // Por encima del máximo, el LED está al 100% (1023)
    } else {
        // Mapear el rango de temperatura al rango de PWM (0 a 1023)
        float brightness_percent = ((temperature_c - TEMP_MIN) / (TEMP_MAX - TEMP_MIN)) * 100.0;
        duty_cycle = (int)(brightness_percent * ((1 << LEDC_DUTY_RES) - 1) / 100.0);
    }

    // Aplicar el ciclo de trabajo al LED
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty_cycle));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));
}

// Función para probar el LED
void ntc_test_led(void) {
    ESP_LOGI(TAG, "Probando LED...");
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, 512)); // 50% de brillo
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, 0)); // Apagar
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));
    ESP_LOGI(TAG, "Prueba de LED completada");
}
