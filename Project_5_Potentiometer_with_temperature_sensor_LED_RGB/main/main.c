#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

#include "potentiometer.h"
#include "rgb_led.h"
#include "ntc_sensor.h"

static const char *TAG = "MAIN";

// Estructura para mensajes de la cola
typedef struct {
    uint8_t pot_percent;
    uint32_t pot_voltage_mv;
} pot_data_t;

// Cola para comunicación entre tareas
static QueueHandle_t pot_queue = NULL;
static QueueHandle_t ntc_queue = NULL;

// Variables globales para mostrar información
static pot_data_t current_pot_data = {0};
static ntc_data_t current_ntc_data = {0};

// Tarea para lectura del potenciómetro
void pot_reading_task(void *arg)
{
    pot_data_t pot_data;
    
    while (1) {
        // Leer datos del potenciómetro
        pot_data.pot_percent = pot_get_percent();
        pot_data.pot_voltage_mv = pot_get_voltage_mv();
        
        // Actualizar datos globales
        current_pot_data = pot_data;
        
        // Enviar datos a la cola
        if (xQueueSend(pot_queue, &pot_data, pdMS_TO_TICKS(100)) != pdTRUE) {
            ESP_LOGW(TAG, "Error enviando datos a la cola");
        }
        
        vTaskDelay(pdMS_TO_TICKS(250)); // leer 4 veces por segundo
    }
}

// Tarea para lectura del sensor NTC
void ntc_reading_task(void *arg)
{
    ntc_data_t ntc_data;
    
    while (1) {
        // Leer datos del sensor NTC
        ntc_data = ntc_read_temperature();
        
        // Actualizar datos globales
        current_ntc_data = ntc_data;
        
        // Enviar datos a la cola
        if (xQueueSend(ntc_queue, &ntc_data, pdMS_TO_TICKS(100)) != pdTRUE) {
            ESP_LOGW(TAG, "Error enviando datos NTC a la cola");
        }
        
        vTaskDelay(pdMS_TO_TICKS(2000)); // leer cada 2 segundos
    }
}

// Tarea para control del LED RGB (potenciómetro)
void rgb_control_task(void *arg)
{
    pot_data_t received_data;
    
    while (1) {
        // Recibir datos de la cola del potenciómetro
        if (xQueueReceive(pot_queue, &received_data, portMAX_DELAY) == pdTRUE) {
            // Actualizar PWM del LED verde basado en el porcentaje del potenciómetro
            rgb_set_green_percent(received_data.pot_percent);
        }
    }
}

// Tarea para control del LED de temperatura (NTC)
void ntc_led_control_task(void *arg)
{
    ntc_data_t received_data;
    
    while (1) {
        // Recibir datos de la cola del sensor NTC
        if (xQueueReceive(ntc_queue, &received_data, portMAX_DELAY) == pdTRUE) {
            // Actualizar brillo del LED basado en la temperatura
            ntc_update_led_brightness(received_data.temperature_c);
        }
    }
}

// Tarea para mostrar información organizada
void display_info_task(void *arg)
{
    while (1) {
        // Mostrar información organizada cada 2 segundos
        printf("\n=== SISTEMA DE MONITOREO ===\n");
        printf("LED Verde: %d%% | Potenciómetro: %lu mV\n", 
               current_pot_data.pot_percent, current_pot_data.pot_voltage_mv);
        printf("Temperatura: %.1f°C | LED Rojo: %.1f%% brillo\n", 
               current_ntc_data.temperature_c, current_ntc_data.brightness_percent);
        printf("=============================\n\n");
        
        vTaskDelay(pdMS_TO_TICKS(2000)); // Mostrar cada 2 segundos
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Inicializando componentes...");
    
    // Inicializar hardware
    pot_init();                    // ADC1 - Potenciómetro (GPIO34)
    rgb_led_init();               // LED RGB
    ntc_sensor_init();            // ADC2 - Sensor NTC (GPIO26)
    ntc_led_pwm_init();           // LED de temperatura (GPIO27)
    
    // Crear colas para comunicación entre tareas
    pot_queue = xQueueCreate(5, sizeof(pot_data_t));
    ntc_queue = xQueueCreate(5, sizeof(ntc_data_t));
    
    if (pot_queue == NULL || ntc_queue == NULL) {
        ESP_LOGE(TAG, "Error creando las colas");
        return;
    }
    ESP_LOGI(TAG, "Colas creadas exitosamente");
    
    // Prueba del LED de temperatura al inicio
    ntc_test_led();
    
    // Crear tarea para lectura del potenciómetro
    if (xTaskCreate(pot_reading_task, "pot_reading_task", 4096, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Error creando tarea de lectura del potenciómetro");
        return;
    }
    
    // Crear tarea para lectura del sensor NTC
    if (xTaskCreate(ntc_reading_task, "ntc_reading_task", 4096, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Error creando tarea de lectura del sensor NTC");
        return;
    }
    
    // Crear tarea para control del LED RGB (potenciómetro)
    if (xTaskCreate(rgb_control_task, "rgb_control_task", 4096, NULL, 4, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Error creando tarea de control del LED RGB");
        return;
    }
    
    // Crear tarea para control del LED de temperatura (NTC)
    if (xTaskCreate(ntc_led_control_task, "ntc_led_control_task", 4096, NULL, 4, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Error creando tarea de control del LED de temperatura");
        return;
    }
    
    // Crear tarea para mostrar información organizada
    if (xTaskCreate(display_info_task, "display_info_task", 4096, NULL, 3, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Error creando tarea de visualización");
        return;
    }
    
    ESP_LOGI(TAG, "Todas las tareas creadas exitosamente. Sistema ejecutando...");
    ESP_LOGI(TAG, "Potenciómetro: ADC1 CH6 (GPIO34) -> LED Verde (GPIO27)");
    ESP_LOGI(TAG, "Sensor NTC: ADC2 CH9 (GPIO26) -> LED Rojo (GPIO25)");
}
