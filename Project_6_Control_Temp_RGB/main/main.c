// ===== INCLUDES Y CONFIGURACIÓN =====
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

#include "ntc_sensor.h"

static const char *TAG = "MAIN";

// ===== ESTRUCTURAS DE DATOS Y VARIABLES GLOBALES =====
static QueueHandle_t ntc_queue = NULL;
static ntc_data_t current_ntc_data = {0};

// ===== TAREAS DEL SISTEMA =====
void ntc_reading_task(void *arg)
{
    ntc_data_t ntc_data;
    
    ESP_LOGI(TAG, "Tarea de lectura del sensor NTC iniciada");
    
    while (1) {
        ntc_data = ntc_read_temperature();
        
        current_ntc_data = ntc_data;
        
        if (xQueueSend(ntc_queue, &ntc_data, pdMS_TO_TICKS(100)) != pdTRUE) {
            ESP_LOGW(TAG, "Error enviando datos del sensor NTC a la cola");
        }
        
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void ntc_led_control_task(void *arg)
{
    ntc_data_t received_data;
    
    ESP_LOGI(TAG, "Tarea de control del LED rojo iniciada");
    
    while (1) {
        if (xQueueReceive(ntc_queue, &received_data, portMAX_DELAY) == pdTRUE) {
            ntc_update_led_brightness(received_data.temperature_c);
        }
    }
}

void display_info_task(void *arg)
{
    ESP_LOGI(TAG, "Tarea de visualización iniciada");
    
    while (1) {
        printf("\n=== SISTEMA DE MONITOREO DE TEMPERATURA ===\n");
        printf("Temperatura: %.1f°C | LED Rojo: %.1f%% brillo\n", 
               current_ntc_data.temperature_c, current_ntc_data.brightness_percent);
        printf("Resistencia NTC: %.0f Ohms | ADC Raw: %d\n",
               current_ntc_data.resistance, current_ntc_data.raw_adc_value);
        printf("==========================================\n\n");
        
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
    ntc_led_pwm_init();
    
    ESP_LOGI(TAG, "Hardware inicializado correctamente");
    
    // ===== CREACIÓN DE COLA PARA COMUNICACIÓN =====
    ntc_queue = xQueueCreate(5, sizeof(ntc_data_t));
    
    if (ntc_queue == NULL) {
        ESP_LOGE(TAG, "Error creando la cola de comunicación");
        return;
    }
    ESP_LOGI(TAG, "Cola de comunicación creada exitosamente");
    
    // ===== PRUEBA INICIAL DEL LED =====
    ESP_LOGI(TAG, "Realizando prueba inicial del LED...");
    ntc_test_led();
    
    // ===== CREACIÓN DE TAREAS DEL SISTEMA =====
    ESP_LOGI(TAG, "Creando tareas del sistema...");
    
    if (xTaskCreate(ntc_reading_task, "ntc_reading_task", 4096, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Error creando tarea de lectura del sensor NTC");
        return;
    }
    
    if (xTaskCreate(ntc_led_control_task, "ntc_led_control_task", 4096, NULL, 4, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Error creando tarea de control del LED rojo");
        return;
    }
    
    if (xTaskCreate(display_info_task, "display_info_task", 4096, NULL, 3, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Error creando tarea de visualización");
        return;
    }
    
    // ===== SISTEMA INICIADO EXITOSAMENTE =====
    ESP_LOGI(TAG, "=== SISTEMA RTOS INICIADO EXITOSAMENTE ===");
    ESP_LOGI(TAG, "Configuración del hardware:");
    ESP_LOGI(TAG, "  - Sensor NTC: ADC2 CH9 (GPIO26) -> LED Rojo (GPIO25)");
    ESP_LOGI(TAG, "Frecuencias de operación:");
    ESP_LOGI(TAG, "  - Lectura sensor NTC: cada 2 segundos");
    ESP_LOGI(TAG, "  - Monitor serie: cada 1 segundo");
    ESP_LOGI(TAG, "=== SISTEMA EN FUNCIONAMIENTO ===");
}