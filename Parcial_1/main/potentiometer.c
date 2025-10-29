// ===== INCLUDES Y CONFIGURACIÓN =====
#include "potentiometer.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdlib.h>

static const char *TAG = "POT";

// ===== CONFIGURACION Y CONTEXTO =====
#define DEFAULT_VREF    1100
#define NO_OF_SAMPLES   8

typedef struct pot_ctx {
    adc_oneshot_unit_handle_t adc_handle;
    adc_cali_handle_t cali_handle;
    bool has_calibration;
    adc_channel_t channel;
} pot_ctx_t;

// ===== FUNCIONES DE CALIBRACIÓN DEL ADC =====
// Intenta crear el manejador de calibración del ADC y reporta estado
static bool adc_calibration_init(adc_unit_t unit, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated) {
        adc_cali_curve_fitting_config_t cali_config = {.unit_id = unit, .atten = atten, .bitwidth = ADC_BITWIDTH_DEFAULT};
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK) calibrated = true;
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!calibrated) {
        adc_cali_line_fitting_config_t cali_config = {.unit_id = unit, .atten = atten, .bitwidth = ADC_BITWIDTH_DEFAULT};
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
        if (ret == ESP_OK) calibrated = true;
    }
#endif
    *out_handle = handle;
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Calibración del ADC exitosa");
    } else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated) {
        ESP_LOGW(TAG, "Calibración no soportada, usando valores sin calibrar");
    } else {
        ESP_LOGE(TAG, "Error en la calibración del ADC");
    }
    return calibrated;
}

// ===== FUNCIONES DE INICIALIZACIÓN =====
pot_ctx_t* pot_create(void)
{
    pot_ctx_t *ctx = (pot_ctx_t*)calloc(1, sizeof(pot_ctx_t));
    if (!ctx) return NULL;
    ESP_LOGI(TAG, "Inicializando ADC1 para potenciómetro...");
    ctx->channel = ADC_CHANNEL_6;
    adc_oneshot_unit_init_cfg_t init_config1 = {.unit_id = ADC_UNIT_1};
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &ctx->adc_handle));
    adc_oneshot_chan_cfg_t config = {.bitwidth = ADC_BITWIDTH_DEFAULT, .atten = ADC_ATTEN_DB_12};
    ESP_ERROR_CHECK(adc_oneshot_config_channel(ctx->adc_handle, ctx->channel, &config));
    ctx->has_calibration = adc_calibration_init(ADC_UNIT_1, ADC_ATTEN_DB_12, &ctx->cali_handle);
    ESP_LOGI(TAG, "Potenciómetro inicializado en GPIO34 (ADC1_CH6)");
    return ctx;
}

void pot_destroy(pot_ctx_t* ctx)
{
    if (!ctx) return;
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (ctx->cali_handle) adc_cali_delete_scheme_curve_fitting(ctx->cali_handle);
#endif
#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (ctx->cali_handle) adc_cali_delete_scheme_line_fitting(ctx->cali_handle);
#endif
    if (ctx->adc_handle) adc_oneshot_del_unit(ctx->adc_handle);
    free(ctx);
}

// ===== FUNCIONES DE LECTURA =====
// Lee varias muestras del ADC y devuelve el promedio para suavizar ruido
static int read_raw_avg(pot_ctx_t* ctx)
{
    int adc_sum = 0;
    for (int i = 0; i < NO_OF_SAMPLES; i++) {
        int adc_raw;
        ESP_ERROR_CHECK(adc_oneshot_read(ctx->adc_handle, ctx->channel, &adc_raw));
        adc_sum += adc_raw;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return adc_sum / NO_OF_SAMPLES;
}

// ===== FUNCIONES PÚBLICAS =====
// Convierte la lectura ADC a milivoltios (usa calibración si está disponible)
uint32_t pot_get_voltage_mv(pot_ctx_t* ctx)
{
    int adc_raw = read_raw_avg(ctx);
    int voltage = 0;
    if (!ctx) return 0;
    
    if (ctx->has_calibration) {
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(ctx->cali_handle, adc_raw, &voltage));
    } else {
        voltage = (adc_raw * 3300) / 4095;
    }
    
    return (uint32_t)voltage;
}

// Calcula el porcentaje 0-100% en función del voltaje medido
uint8_t pot_get_percent(pot_ctx_t* ctx)
{
    uint32_t mv = pot_get_voltage_mv(ctx);
    return mv >= 3300 ? 100 : (uint8_t)((mv * 100) / 3300);
}
