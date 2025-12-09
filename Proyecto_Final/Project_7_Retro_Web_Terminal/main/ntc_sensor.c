/**
 * ============================================================================
 * ARCHIVO: ntc_sensor.c
 * ============================================================================
 * 
 * RESUMEN:
 * Implementación del módulo de sensor de temperatura NTC. Este módulo:
 * - Inicializa el ADC1 del ESP32 para leer el voltaje del divisor de voltaje
 * - Calcula la resistencia del NTC basándose en el valor ADC
 * - Convierte la resistencia a temperatura usando la ecuación de Steinhart-Hart
 * - Ejecuta una tarea periódica que lee la temperatura cada segundo
 * - Proporciona acceso thread-safe a los datos de temperatura
 * 
 * Circuito:
 * VCC ---[10k Resistor]---[NTC 10k]---GND
 *                |
 *              GPIO32 (ADC1_CH4)
 * 
 * ============================================================================
 * ÍNDICE DE SECCIONES:
 * ============================================================================
 * Sección 1: INCLUDES se encuentra en las líneas 21 a 38
 * Sección 2: DEFINICIONES Y CONSTANTES se encuentra en las líneas 40 a 55
 * Sección 3: VARIABLES GLOBALES se encuentra en las líneas 57 a 65
 * Sección 4: CALIBRACIÓN DEL ADC se encuentra en las líneas 67 a 108
 * Sección 5: INICIALIZACIÓN se encuentra en las líneas 109 a 137
 * Sección 6: LECTURA Y CÁLCULO DE TEMPERATURA se encuentra en las líneas 139 a 191
 * Sección 7: TAREA DE LECTURA PERIÓDICA se encuentra en las líneas 193 a 237
 * Sección 8: FUNCIONES GETTER se encuentra en las líneas 239 a 277
 * ============================================================================
 */

// ===== INCLUDES =====
// Header local
#include "ntc_sensor.h"
#include "iot_client.h"

// ESP-IDF
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "hal/adc_types.h"

// FreeRTOS
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

// Estándar C
#include <math.h>
#include <time.h>

// ===== DEFINICIONES Y CONSTANTES =====
static const char *TAG = "NTC_TEMP_CONTROL";

// Valores de error y validación
#define TEMP_ERROR_VALUE -999.0
#define ADC_MAX_VALUE 4096
#define ADC_MIN_VALUE 0
#define RESISTANCE_MAX_OHMS 1000000.0

// Configuración de temporización
#define SENSOR_READ_INTERVAL_MS 1000      // Intervalo de lectura del sensor (1 segundo)
#define ADC_STABILIZATION_DELAY_MS 100    // Tiempo de espera para estabilización del ADC

// Configuración de tarea
#define NTC_TASK_STACK_SIZE 4096          // Tamaño del stack de la tarea
#define NTC_TASK_PRIORITY 5               // Prioridad de la tarea

// ===== VARIABLES GLOBALES =====
// Handles del ADC y calibración
static adc_oneshot_unit_handle_t adc1_handle;
static adc_cali_handle_t adc1_cali_handle = NULL;

// Datos actuales del sensor (protegidos por mutex)
static ntc_data_t current_ntc_data = {.temperature_c = TEMP_ERROR_VALUE, .resistance = 0.0, .raw_adc_value = 0};
static bool data_ready = false;
static SemaphoreHandle_t data_mutex = NULL; // Mutex para proteger acceso concurrente

// ===== SECCIÓN: CALIBRACIÓN DEL ADC =====
/**
 * Inicializa la calibración del ADC para obtener lecturas más precisas
 * Intenta usar curve fitting primero, luego line fitting como fallback
 * @param unit: Unidad ADC (ADC_UNIT_1 o ADC_UNIT_2)
 * @param atten: Atenuación del ADC
 * @param out_handle: Puntero donde se guardará el handle de calibración
 * @return true si la calibración fue exitosa, false en caso contrario
 */
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
        ESP_LOGI(TAG, "Calibración exitosa");
    } else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated) {
        ESP_LOGW(TAG, "Calibración no soportada, use valores sin calibrar");
    } else {
        ESP_LOGE(TAG, "Fallo en la calibración");
    }
    return calibrated;
}

// ===== SECCIÓN: INICIALIZACIÓN =====
/**
 * Inicializa el ADC1 y configura el canal para leer el sensor NTC
 * - Crea el mutex para proteger acceso a datos
 * - Configura ADC1 con atenuación de 12dB (rango 0-3.3V)
 * - Inicializa la calibración del ADC si está disponible
 * 
 * Nota: Usamos ADC1 porque ADC2 no funciona cuando WiFi está activo
 */
void ntc_sensor_init(void) {
    ESP_LOGI(TAG, "Inicializando ADC1 para sensor NTC en GPIO32...");
    
    // Crear mutex para proteger acceso concurrente a los datos
    data_mutex = xSemaphoreCreateMutex();
    if (data_mutex == NULL) {
        ESP_LOGE(TAG, "Error al crear mutex para datos del sensor");
        return;
    }
    
    adc_oneshot_unit_init_cfg_t init_config1 = {.unit_id = ADC_UNIT};
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    adc_oneshot_chan_cfg_t config = {.bitwidth = ADC_BITWIDTH_DEFAULT, .atten = ADC_ATTEN_DB_12};
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, NTC_PIN, &config));

    adc_calibration_init(ADC_UNIT, ADC_ATTEN_DB_12, &adc1_cali_handle);
    ESP_LOGI(TAG, "ADC1 inicializado correctamente en GPIO32 (ADC_CHANNEL_4)");
}


// ===== SECCIÓN: LECTURA Y CÁLCULO DE TEMPERATURA =====
/**
 * Lee el valor del ADC, calcula la resistencia del NTC y convierte a temperatura
 * 
 * Proceso:
 * 1. Lee el valor crudo del ADC (0-4095)
 * 2. Calcula la resistencia del NTC usando la fórmula del divisor de voltaje:
 *    R_ntc = R_series * (4095 / ADC_value - 1)
 * 3. Convierte resistencia a temperatura usando ecuación de Steinhart-Hart:
 *    1/T = 1/T0 + (1/B) * ln(R/R0)
 * 
 * @return Estructura ntc_data_t con temperatura, resistencia y valor ADC
 */
ntc_data_t ntc_read_temperature(void) {
    ntc_data_t ntc_data = {0};
    int raw_adc_value;
    
    if (adc_oneshot_read(adc1_handle, NTC_PIN, &raw_adc_value) != ESP_OK) {
        ESP_LOGE(TAG, "Error al leer ADC1");
        ntc_data.temperature_c = TEMP_ERROR_VALUE;
        return ntc_data;
    }
    
    ntc_data.raw_adc_value = raw_adc_value;
    
    // Validar rango del ADC
    if (raw_adc_value <= ADC_MIN_VALUE || raw_adc_value >= ADC_MAX_VALUE) {
        ESP_LOGW(TAG, "Valor ADC fuera de rango: %d", raw_adc_value);
        ntc_data.temperature_c = TEMP_ERROR_VALUE;
        return ntc_data;
    }
    
    // Calcular resistencia del NTC usando fórmula del divisor de voltaje
    // R_ntc = R_series * (VCC / V_adc - 1) = R_series * (ADC_MAX / ADC_value - 1)
    float resistance = SERIES_RESISTOR * ((4095.0f / raw_adc_value) - 1.0f);
    ntc_data.resistance = resistance;

    // Validar rango de resistencia
    if (resistance <= 0 || resistance >= RESISTANCE_MAX_OHMS) {
        ESP_LOGW(TAG, "Resistencia fuera de rango: %.0fΩ", resistance);
        ntc_data.temperature_c = TEMP_ERROR_VALUE;
        return ntc_data;
    }
    
    // Cálculo de temperatura usando ecuación de Steinhart-Hart simplificada
    // 1/T = 1/T0 + (1/B) * ln(R/R0)
    // Donde T0 = 25°C = 298.15K, R0 = resistencia nominal, B = coeficiente Beta
    float steinhart = logf(resistance / NOMINAL_RESISTANCE) / B_COEFFICIENT + 
                      1.0f / (NOMINAL_TEMPERATURE + 273.15f);
    ntc_data.temperature_c = (1.0f / steinhart) - 273.15f;
    
    return ntc_data;
}

// ===== SECCIÓN: TAREA DE LECTURA PERIÓDICA =====
/**
 * Tarea de FreeRTOS que lee la temperatura periódicamente cada segundo
 * - Lee la temperatura del sensor
 * - Valida que los datos estén en rangos razonables
 * - Almacena los datos de forma thread-safe usando mutex
 * - Marca los datos como "ready" cuando son válidos
 * 
 * @param arg: Argumentos de la tarea (no usado)
 */
static void ntc_reading_task(void *arg)
{
    ntc_data_t ntc_data;
    ESP_LOGI(TAG, "Tarea de lectura del sensor NTC iniciada");
    
    // Esperar un poco para que el ADC se estabilice
    vTaskDelay(pdMS_TO_TICKS(ADC_STABILIZATION_DELAY_MS));
    
    while (1) {
        ntc_data = ntc_read_temperature();
        
        // Proteger acceso concurrente con mutex
        if (xSemaphoreTake(data_mutex, portMAX_DELAY) == pdTRUE) {
            // Siempre almacenar los últimos datos leídos, incluso si no son perfectos
            // Esto permite que el web server vea qué está leyendo el sensor
            current_ntc_data = ntc_data;
            
            // Validar datos para marcar como "ready"
            // Aceptar cualquier temperatura calculada que sea válida
            if (ntc_data.raw_adc_value > ADC_MIN_VALUE && 
                ntc_data.raw_adc_value < ADC_MAX_VALUE && 
                ntc_data.temperature_c != TEMP_ERROR_VALUE && 
                isfinite(ntc_data.temperature_c) && 
                !isnan(ntc_data.temperature_c)) {
                data_ready = true;
                
                // Enviar temperatura al servidor IoT si el cliente está listo
                if (iot_client_is_ready()) {
                    iot_temperature_data_t iot_data = {
                        .temperature = ntc_data.temperature_c,
                        .timestamp = (uint32_t)time(NULL)
                    };
                    iot_send_temperature(&iot_data);
                }
            } else {
                data_ready = false;
            }
            
            xSemaphoreGive(data_mutex);
        }
        
        vTaskDelay(pdMS_TO_TICKS(SENSOR_READ_INTERVAL_MS));
    }
}

// ===== SECCIÓN: FUNCIONES GETTER =====
/**
 * Obtiene la temperatura actual almacenada por la tarea de lectura
 * Esta función es thread-safe y puede llamarse desde cualquier tarea
 * 
 * @return Estructura ntc_data_t con los últimos datos leídos
 */
ntc_data_t ntc_get_current_temperature(void) {
    ntc_data_t temp_data = {.temperature_c = TEMP_ERROR_VALUE, .resistance = 0.0, .raw_adc_value = 0};
    
    // Proteger acceso concurrente con mutex
    const TickType_t mutex_timeout = pdMS_TO_TICKS(100);
    if (data_mutex != NULL && xSemaphoreTake(data_mutex, mutex_timeout) == pdTRUE) {
        temp_data = current_ntc_data;
        xSemaphoreGive(data_mutex);
    }
    
    return temp_data;
}

/**
 * Crea e inicia la tarea de FreeRTOS para lectura periódica de temperatura
 * La tarea lee la temperatura cada segundo y actualiza los datos globales
 */
void ntc_start_reading_task(void) {
    BaseType_t ret = xTaskCreate(ntc_reading_task, 
                                  "ntc_reader", 
                                  NTC_TASK_STACK_SIZE, 
                                  NULL, 
                                  NTC_TASK_PRIORITY, 
                                  NULL);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Error al crear tarea de lectura del NTC");
    } else {
        ESP_LOGI(TAG, "Tarea de lectura periódica del NTC iniciada");
    }
}

