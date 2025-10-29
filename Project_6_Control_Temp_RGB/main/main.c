// ===== INCLUDES Y CONFIGURACIÓN =====
#include <stdio.h>
#include <stdarg.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

#include "ntc_sensor.h"
#include "button_control.h"
#include "potentiometer.h"
#include "rgb_led.h"
#include "uart_commands.h"

static const char *TAG = "MAIN";

// ===== ESTRUCTURAS DE DATOS Y VARIABLES GLOBALES =====
typedef struct {
    uint8_t pot_percent;
    uint32_t pot_voltage_mv;
} pot_data_t;

// Colas del sistema para comunicación entre tareas
static QueueHandle_t pot_queue = NULL;
static QueueHandle_t ntc_queue = NULL;
static QueueHandle_t led_queue = NULL;
static QueueHandle_t led_command_queue = NULL;  // Cola para comandos LED desde UART

// Variables globales (mantenidas para compatibilidad)
static pot_data_t current_pot_data = {0};
static ntc_data_t current_ntc_data = {0};
static bool data_ready = false;

// ===== FUNCIÓN AUXILIAR PARA LOGS CONDICIONALES =====
static void conditional_log_info(const char *tag, const char *format, ...) {
    if (is_print_enabled()) {
        va_list args; va_start(args, format);
        esp_log_writev(ESP_LOG_INFO, tag, format, args); va_end(args);
    }
}

// ===== TAREAS DEL SISTEMA =====
void pot_reading_task(void *arg)
{
    pot_data_t pot_data;
    static uint8_t last_intensity = 0;
    static uint32_t last_log_time = 0;
    
    ESP_LOGI(TAG, "Tarea de lectura del potenciómetro iniciada");
    
    while (1) {
        pot_data.pot_percent = pot_get_percent();
        pot_data.pot_voltage_mv = pot_get_voltage_mv();
        
        if (pot_queue != NULL) xQueueSend(pot_queue, &pot_data, pdMS_TO_TICKS(10));
        current_pot_data = pot_data;
        
        if (pot_data.pot_percent != last_intensity && !is_manual_control_active()) {
            rgb_led_set_intensity(pot_data.pot_percent);
            last_intensity = pot_data.pot_percent;
        }
        
        uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if (current_time - last_log_time >= 2000) last_log_time = current_time;
        
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

void ntc_reading_task(void *arg)
{
    ntc_data_t ntc_data;
    ESP_LOGI(TAG, "Tarea de lectura del sensor NTC iniciada");
    
    while (1) {
        ntc_data = ntc_read_temperature();
        
        if (ntc_data.raw_adc_value > 0 && ntc_data.raw_adc_value < 4096 && 
            ntc_data.temperature_c > -50.0 && ntc_data.temperature_c < 150.0 && ntc_data.temperature_c != -999.0) {
            if (ntc_queue != NULL) xQueueSend(ntc_queue, &ntc_data, pdMS_TO_TICKS(10));
            current_ntc_data = ntc_data; data_ready = true;
        } else {
            conditional_log_info(TAG, "Datos inválidos: Temp=%.1f°C, ADC=%d, R=%.0fΩ", 
                     ntc_data.temperature_c, ntc_data.raw_adc_value, ntc_data.resistance);
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

// ===== TAREA DE CONTROL DEL LED RGB =====
void rgb_control_task(void *arg)
{
    pot_data_t pot_data;
    ntc_data_t ntc_data;
    led_command_t led_cmd;
    BaseType_t pot_status, ntc_status, led_cmd_status;
    temp_thresholds_t* thresholds;
    
    ESP_LOGI(TAG, "Tarea de control del LED RGB iniciada");
    
    while (1) {
        // Procesar comandos LED desde UART (prioridad alta)
        led_cmd_status = xQueueReceive(led_command_queue, &led_cmd, pdMS_TO_TICKS(10));
        if (led_cmd_status == pdPASS) {
            ESP_LOGI(TAG, "Procesando comando LED: %s", led_cmd.command);
            
            if (strcmp(led_cmd.command, "led_on") == 0) {
                rgb_led_set_color(led_cmd.red, led_cmd.green, led_cmd.blue);
                ESP_LOGI(TAG, "LED encendido: R=%d, G=%d, B=%d", led_cmd.red, led_cmd.green, led_cmd.blue);
            } else if (strcmp(led_cmd.command, "led_off") == 0) {
                rgb_led_off();
                ESP_LOGI(TAG, "LED apagado");
            } else if (strcmp(led_cmd.command, "set_color") == 0) {
                rgb_led_set_color(led_cmd.red, led_cmd.green, led_cmd.blue);
                ESP_LOGI(TAG, "Color establecido: R=%d, G=%d, B=%d", led_cmd.red, led_cmd.green, led_cmd.blue);
            }
            
            if (led_cmd.manual_control) ESP_LOGI(TAG, "Control manual activado por comando UART");
        }
        
        // Procesar datos del potenciómetro desde la cola (siempre activo para intensidad)
        pot_status = xQueueReceive(pot_queue, &pot_data, pdMS_TO_TICKS(10));
        if (pot_status == pdPASS) {
            rgb_led_set_intensity(pot_data.pot_percent);
            ESP_LOGD(TAG, "Intensidad LED actualizada: %d%%", pot_data.pot_percent);
        }
        
        // Procesar datos del sensor NTC desde la cola (solo si no está en modo manual y está inicializado)
        ntc_status = xQueueReceive(ntc_queue, &ntc_data, pdMS_TO_TICKS(10));
        if (ntc_status == pdPASS && !is_manual_control_active() && is_temperature_control_initialized()) {
            thresholds = get_temp_thresholds();
            float temp = ntc_data.temperature_c;
            char last_color = get_last_configured_color();
            bool in_range = false;
            
            if (last_color == 'R') {
                in_range = (temp >= thresholds->r_min && temp <= thresholds->r_max);
                if (in_range) rgb_led_set_color(255, 0, 0); else rgb_led_off();
            } else if (last_color == 'G') {
                in_range = (temp >= thresholds->g_min && temp <= thresholds->g_max);
                if (in_range) rgb_led_set_color(0, 255, 0); else rgb_led_off();
            } else if (last_color == 'B') {
                in_range = (temp >= thresholds->b_min && temp <= thresholds->b_max);
                if (in_range) rgb_led_set_color(0, 0, 255); else rgb_led_off();
            } else {
                rgb_led_off();
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(50));  // Control cada 50ms
    }
}

void display_info_task(void *arg)
{
    ESP_LOGI(TAG, "Tarea de visualización iniciada");
    
    while (1) {
        if (is_print_enabled()) {
            printf("\n=== SISTEMA DE MONITOREO ===\n");
            printf("Potenciómetro: %d%% (%lu mV)\n", current_pot_data.pot_percent, current_pot_data.pot_voltage_mv);
            
            if (rgb_led_is_on()) {
                printf("LED RGB: %s | Intensidad: %d%% | RGB(%d,%d,%d)\n", 
                       rgb_led_get_color_name(), rgb_led_get_intensity(),
                       rgb_led_get_red(), rgb_led_get_green(), rgb_led_get_blue());
            } else {
                printf("LED RGB: APAGADO\n");
            }
            
            if (data_ready) {
                printf("Temperatura: %.1f°C\n", current_ntc_data.temperature_c);
                printf("Resistencia NTC: %.0f Ohms | ADC Raw: %d\n", current_ntc_data.resistance, current_ntc_data.raw_adc_value);
            } else {
                printf("Esperando datos del sensor NTC...\n");
            }
            
            if (is_temperature_control_initialized()) {
                printf("Modo: %s | Control Temp: ACTIVADO | Impresión: HABILITADA\n", 
                       is_manual_control_active() ? "MANUAL" : "AUTOMÁTICO");
            } else {
                printf("Modo: INICIALIZANDO | Control Temp: DESACTIVADO | Impresión: HABILITADA\n");
            }
            printf("==========================================\n\n");
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}


// ===== FUNCIÓN PRINCIPAL DEL SISTEMA =====
void app_main(void)
{
    ESP_LOGI(TAG, "=== INICIANDO SISTEMA RTOS - SENSOR NTC ===");
    ESP_LOGI(TAG, "Inicializando componentes de hardware...");
    
    // ===== INICIALIZACIÓN DE HARDWARE =====
    ntc_sensor_init();
    button_control_init();
    pot_init();
    rgb_led_init();
    uart_commands_init();
    
    ESP_LOGI(TAG, "Hardware inicializado correctamente");
    
    // ===== CREACIÓN DE COLAS DEL SISTEMA =====
    ESP_LOGI(TAG, "Creando colas del sistema...");
    
    pot_queue = xQueueCreate(5, sizeof(pot_data_t));
    ntc_queue = xQueueCreate(5, sizeof(ntc_data_t));
    led_queue = xQueueCreate(5, sizeof(uint8_t));
    led_command_queue = get_led_command_queue();
    
    if (pot_queue == NULL || ntc_queue == NULL || led_queue == NULL || led_command_queue == NULL) {
        ESP_LOGE(TAG, "Error creando colas del sistema");
        return;
    }
    ESP_LOGI(TAG, "Colas del sistema creadas correctamente");
    
    // ===== CREACIÓN DE TAREAS DEL SISTEMA =====
    ESP_LOGI(TAG, "Creando tareas del sistema...");
    
    if (xTaskCreate(pot_reading_task, "pot_reading_task", 4096, NULL, 5, NULL) != pdPASS ||
        xTaskCreate(ntc_reading_task, "ntc_reading_task", 4096, NULL, 5, NULL) != pdPASS ||
        xTaskCreate(display_info_task, "display_info_task", 4096, NULL, 3, NULL) != pdPASS ||
        xTaskCreate(button_task, "button_task", 4096, NULL, 4, NULL) != pdPASS ||
        xTaskCreate(rgb_control_task, "rgb_control_task", 4096, NULL, 6, NULL) != pdPASS ||
        xTaskCreate(uart_commands_task, "uart_commands_task", 4096, NULL, 7, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Error creando tareas del sistema");
        return;
    }
    
    // ===== SISTEMA INICIADO EXITOSAMENTE =====
    ESP_LOGI(TAG, "=== SISTEMA RTOS INICIADO EXITOSAMENTE ===");
    ESP_LOGI(TAG, "Hardware: Pot=GPIO34, NTC=GPIO26, Botón=GPIO14, RGB=R13/G12/B25, UART=GPIO1/3");
    ESP_LOGI(TAG, "Frecuencias: Pot=4Hz, NTC=0.5Hz, LED=20Hz, Monitor=0.5Hz, Botón=100Hz, UART=tiempo real");
    ESP_LOGI(TAG, "Controles: Botón=impresión ON/OFF, Pot=intensidad LED, UART=umbrales temperatura");
    ESP_LOGI(TAG, "=== SISTEMA EN FUNCIONAMIENTO ===");
}