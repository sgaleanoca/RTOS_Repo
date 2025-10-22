// ===== INCLUDES Y CONFIGURACIÓN =====
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

#include "ntc_sensor.h"

static const char *TAG = "MAIN";

// ===== ESTRUCTURAS DE DATOS Y VARIABLES GLOBALES =====
static ntc_data_t current_ntc_data = {0};
static bool data_ready = false;

// ===== TAREAS DEL SISTEMA =====
void ntc_reading_task(void *arg)
{
    ntc_data_t ntc_data;
    
    ESP_LOGI(TAG, "Tarea de lectura del sensor NTC iniciada");
    
    while (1) {
        ntc_data = ntc_read_temperature();
        
        // Validar que los datos sean razonables
        if (ntc_data.raw_adc_value > 0 && ntc_data.raw_adc_value < 4096 && 
            ntc_data.temperature_c > -50.0 && ntc_data.temperature_c < 150.0 &&
            ntc_data.temperature_c != -999.0) {
            
            current_ntc_data = ntc_data;
            data_ready = true;
            
            ESP_LOGI(TAG, "Datos válidos: Temp=%.1f°C, ADC=%d, R=%.0fΩ", 
                     ntc_data.temperature_c, ntc_data.raw_adc_value, ntc_data.resistance);
        } else {
            ESP_LOGW(TAG, "Datos inválidos: Temp=%.1f°C, ADC=%d, R=%.0fΩ", 
                     ntc_data.temperature_c, ntc_data.raw_adc_value, ntc_data.resistance);
        }
        
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}


void display_info_task(void *arg)
{
    ESP_LOGI(TAG, "Tarea de visualización iniciada");
    
    while (1) {
        if (data_ready) {
            printf("\n=== SISTEMA DE MONITOREO DE TEMPERATURA ===\n");
            printf("Temperatura: %.1f°C\n", current_ntc_data.temperature_c);
            printf("Resistencia NTC: %.0f Ohms | ADC Raw: %d\n",
                   current_ntc_data.resistance, current_ntc_data.raw_adc_value);
            printf("==========================================\n\n");
        } else {
            printf("\n=== SISTEMA DE MONITOREO DE TEMPERATURA ===\n");
            printf("Esperando datos del sensor...\n");
            printf("==========================================\n\n");
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// ===== FUNCIÓN PRINCIPAL DEL SISTEMA =====
void app_main(void)
{
    ESP_LOGI(TAG, "=== INICIANDO SISTEMA RTOS - SENSOR NTC ===");
    ESP_LOGI(TAG, "Inicializando componentes de hardware...");
    
    // ===== INICIALIZACIÓN DE HARDWARE =====
    ntc_sensor_init();
    
    ESP_LOGI(TAG, "Hardware inicializado correctamente");
    
    
    
    // ===== CREACIÓN DE TAREAS DEL SISTEMA =====
    ESP_LOGI(TAG, "Creando tareas del sistema...");
    
    if (xTaskCreate(ntc_reading_task, "ntc_reading_task", 4096, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Error creando tarea de lectura del sensor NTC");
        return;
    }
    
    
    if (xTaskCreate(display_info_task, "display_info_task", 4096, NULL, 3, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Error creando tarea de visualización");
        return;
    }
    
    // ===== SISTEMA INICIADO EXITOSAMENTE =====
    ESP_LOGI(TAG, "=== SISTEMA RTOS INICIADO EXITOSAMENTE ===");
    ESP_LOGI(TAG, "Configuración del hardware:");
    ESP_LOGI(TAG, "  - Sensor NTC: ADC2 CH9 (GPIO26)");
    ESP_LOGI(TAG, "Frecuencias de operación:");
    ESP_LOGI(TAG, "  - Lectura sensor NTC: cada 2 segundos");
    ESP_LOGI(TAG, "  - Monitor serie: cada 1 segundo");
    ESP_LOGI(TAG, "=== SISTEMA EN FUNCIONAMIENTO ===");
}