#include "ntc.h"
#include "potentiometer.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"
#include <math.h>

static const char *TAG = "NTC";

// Variables globales para ADC
static adc_oneshot_unit_handle_t adc1_handle;
static adc_cali_handle_t adc1_cali_handle = NULL;
static bool do_calibration = false;

/**
 * @brief Calibra el ADC para obtener lecturas precisas
 */
static bool adc_calibration_init(void)
{
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "Calibración usando Curve Fitting");
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = ADC_UNIT_1,
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_12,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &adc1_cali_handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "Calibración usando Line Fitting");
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = ADC_UNIT_1,
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_12,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &adc1_cali_handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

    return calibrated;
}

/**
 * @brief Inicializa el ADC para el termistor NTC
 */
void ntc_init(void)
{
    // Obtener el handle del ADC1 ya inicializado por el potenciómetro
    adc1_handle = (adc_oneshot_unit_handle_t)pot_get_adc_handle();
    
    if (adc1_handle == NULL) {
        ESP_LOGE(TAG, "Error: ADC1 no está inicializado. Asegúrate de llamar pot_init() primero.");
        return;
    }

    // Configuración del canal ADC para el termistor
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, NTC_ADC_CHANNEL, &config));

    // Inicializar calibración
    do_calibration = adc_calibration_init();

    ESP_LOGI(TAG, "Termistor NTC inicializado en GPIO35 (ADC1_CH7)");
}

/**
 * @brief Obtiene la resistencia del termistor en ohmios
 * @return Resistencia en ohmios
 */
float ntc_get_resistance(void)
{
    int adc_raw = 0;
    int voltage = 0;
    
    // Leer valor ADC
    ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, NTC_ADC_CHANNEL, &adc_raw));
    
    // Convertir a voltaje si hay calibración
    if (do_calibration) {
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle, adc_raw, &voltage));
    } else {
        // Conversión simple sin calibración (aproximada)
        voltage = (adc_raw * 3300) / 4095; // 3.3V, 12-bit ADC
    }
    
    // Calcular resistencia usando divisor de voltaje
    // Vout = Vin * Rntc / (Rref + Rntc)
    // Rntc = Rref * Vout / (Vin - Vout)
    float vout = voltage / 1000.0; // Convertir mV a V
    float vin = 3.3; // Voltaje de alimentación
    
    if (vout >= vin) {
        return 0; // Evitar división por cero
    }
    
    float resistance = (NTC_REF_RESISTOR * vout) / (vin - vout);
    return resistance;
}

/**
 * @brief Lee la temperatura actual del termistor usando ecuación de Steinhart-Hart
 * @return Temperatura en grados Celsius
 */
float ntc_get_temperature_celsius(void)
{
    float resistance = ntc_get_resistance();
    
    if (resistance <= 0) {
        return -999; // Valor de error
    }
    
    // Ecuación de Steinhart-Hart simplificada usando coeficiente Beta
    // 1/T = 1/T0 + (1/B) * ln(R/R0)
    // Donde:
    // T0 = temperatura de referencia (25°C = 298.15K)
    // R0 = resistencia de referencia (10kΩ)
    // B = coeficiente Beta (3950)
    // R = resistencia actual
    
    float t0_kelvin = NTC_NOMINAL_TEMP + 273.15; // 298.15K
    float beta = NTC_BETA_COEFF;
    
    float temp_kelvin = 1.0 / ((1.0 / t0_kelvin) + (1.0 / beta) * log(resistance / NTC_NOMINAL_RES));
    float temp_celsius = temp_kelvin - 273.15;
    
    return temp_celsius;
}

/**
 * @brief Convierte la temperatura a porcentaje (10°C = 0%, 50°C = 100%)
 * @param temp_celsius Temperatura en grados Celsius
 * @return Porcentaje de 0 a 100
 */
uint8_t ntc_temp_to_percent(float temp_celsius)
{
    if (temp_celsius <= TEMP_MIN_CELSIUS) {
        return 0;
    }
    if (temp_celsius >= TEMP_MAX_CELSIUS) {
        return 100;
    }
    
    // Interpolación lineal entre 10°C y 50°C
    float percent = ((temp_celsius - TEMP_MIN_CELSIUS) / (TEMP_MAX_CELSIUS - TEMP_MIN_CELSIUS)) * 100.0;
    
    return (uint8_t)percent;
}
