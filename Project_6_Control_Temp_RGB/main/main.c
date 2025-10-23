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
        va_list args;
        va_start(args, format);
        esp_log_writev(ESP_LOG_INFO, tag, format, args);
        va_end(args);
    }
}

// ===== TAREAS DEL SISTEMA =====
void pot_reading_task(void *arg)
{
    pot_data_t pot_data;
    static uint8_t last_intensity = 0;
    static uint32_t last_log_time = 0;
    uint32_t current_time;
    
    ESP_LOGI(TAG, "Tarea de lectura del potenciómetro iniciada");
    
    while (1) {
        pot_data.pot_percent = pot_get_percent();
        pot_data.pot_voltage_mv = pot_get_voltage_mv();
        
        // Enviar datos a la cola del potenciómetro
        if (pot_queue != NULL) {
            xQueueSend(pot_queue, &pot_data, pdMS_TO_TICKS(10));
        }
        
        // Mantener compatibilidad con variables globales
        current_pot_data = pot_data;
        
        // Controlar intensidad del LED RGB con el potenciómetro (solo si no está en modo manual)
        if (pot_data.pot_percent != last_intensity && !is_manual_control_active()) {
            rgb_led_set_intensity(pot_data.pot_percent);
            last_intensity = pot_data.pot_percent;
        }
        
        // Solo imprimir cada 2 segundos
        current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if (current_time - last_log_time >= 2000) {
            conditional_log_info(TAG, "Potenciómetro: %d%% (%lu mV) - LED RGB: %d%%", 
                               pot_data.pot_percent, pot_data.pot_voltage_mv, pot_data.pot_percent);
            last_log_time = current_time;
        }
        
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

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
            
            // Enviar datos a la cola del sensor NTC
            if (ntc_queue != NULL) {
                xQueueSend(ntc_queue, &ntc_data, pdMS_TO_TICKS(10));
            }
            
            // Mantener compatibilidad con variables globales
            current_ntc_data = ntc_data;
            data_ready = true;
            
            // Actualizar LED RGB según temperatura y thresholds
            // Función de control RGB por temperatura deshabilitada
            
            conditional_log_info(TAG, "Datos válidos: Temp=%.1f°C, ADC=%d, R=%.0fΩ", 
                     ntc_data.temperature_c, ntc_data.raw_adc_value, ntc_data.resistance);
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
            }
            else if (strcmp(led_cmd.command, "led_off") == 0) {
                rgb_led_off();
                ESP_LOGI(TAG, "LED apagado");
            }
            else if (strcmp(led_cmd.command, "set_color") == 0) {
                rgb_led_set_color(led_cmd.red, led_cmd.green, led_cmd.blue);
                ESP_LOGI(TAG, "Color establecido: R=%d, G=%d, B=%d", led_cmd.red, led_cmd.green, led_cmd.blue);
            }
            
            // Actualizar flag de control manual
            if (led_cmd.manual_control) {
                // El flag se actualiza en uart_commands.c
                ESP_LOGI(TAG, "Control manual activado por comando UART");
            }
        }
        
        // Procesar datos del potenciómetro desde la cola (siempre activo para intensidad)
        pot_status = xQueueReceive(pot_queue, &pot_data, pdMS_TO_TICKS(10));
        if (pot_status == pdPASS) {
            // Actualizar intensidad del LED RGB basado en el potenciómetro
            // Esto NO afecta el color, solo la intensidad
            rgb_led_set_intensity(pot_data.pot_percent);
            ESP_LOGD(TAG, "Intensidad LED actualizada: %d%%", pot_data.pot_percent);
        }
        
        // Procesar datos del sensor NTC desde la cola (solo si no está en modo manual)
        ntc_status = xQueueReceive(ntc_queue, &ntc_data, pdMS_TO_TICKS(10));
        if (ntc_status == pdPASS) {
            // Solo controlar LED si no está en modo manual
            if (!is_manual_control_active()) {
                // Control RGB basado en temperatura
                thresholds = get_temp_thresholds();
                float temp = ntc_data.temperature_c;
                
                // Lógica específica para rangos de temperatura
                // Mantener los 3 umbrales pero con comportamiento definido
                bool in_red = (temp >= thresholds->r_min && temp <= thresholds->r_max);
                bool in_green = (temp >= thresholds->g_min && temp <= thresholds->g_max);
                bool in_blue = (temp >= thresholds->b_min && temp <= thresholds->b_max);
                
                // Debug: Mostrar información de rangos (deshabilitado para limpiar salida)
                // ESP_LOGI(TAG, "Temperatura: %.1f°C | R:%.1f-%.1f G:%.1f-%.1f B:%.1f-%.1f | in_red:%d in_green:%d in_blue:%d", 
                //          temp, thresholds->r_min, thresholds->r_max, thresholds->g_min, thresholds->g_max, 
                //          thresholds->b_min, thresholds->b_max, in_red, in_green, in_blue);
                
                // Lógica específica para rangos superpuestos
                if (in_red && in_green) {
                    // Rango superpuesto rojo-verde (10-15°C): AMARILLO (rojo + verde)
                    rgb_led_set_color(255, 255, 0);
                    // ESP_LOGI(TAG, "Temperatura %.1f°C -> LED AMARILLO (R+G superpuesto)", temp);
                }
                else if (in_green && !in_red) {
                    // Solo verde (16-30°C): VERDE PURO
                    rgb_led_set_color(0, 255, 0);
                    // ESP_LOGI(TAG, "Temperatura %.1f°C -> LED VERDE PURO", temp);
                }
                else if (in_red && !in_green) {
                    // Solo rojo (0-10°C): ROJO PURO
                    rgb_led_set_color(255, 0, 0);
                    // ESP_LOGI(TAG, "Temperatura %.1f°C -> LED ROJO PURO", temp);
                }
                else if (in_blue) {
                    // Azul (40-50°C): AZUL PURO
                    rgb_led_set_color(0, 0, 255);
                    // ESP_LOGI(TAG, "Temperatura %.1f°C -> LED AZUL PURO", temp);
                }
                else {
                    // Temperatura fuera de todos los rangos - APAGAR LED
                    rgb_led_off();
                    // ESP_LOGI(TAG, "Temperatura %.1f°C -> LED APAGADO (fuera de rangos)", temp);
                }
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(50));  // Control cada 50ms
    }
}

void display_info_task(void *arg)
{
    ESP_LOGI(TAG, "Tarea de visualización iniciada");
    
    while (1) {
        // Solo mostrar datos si la impresión está habilitada
        if (is_print_enabled()) {
            printf("\n=== SISTEMA DE MONITOREO ===\n");
            printf("Potenciómetro: %d%% (%lu mV)\n", 
                   current_pot_data.pot_percent, current_pot_data.pot_voltage_mv);
            
            // Información del LED RGB
            if (rgb_led_is_on()) {
                printf("LED RGB: %s | Intensidad: %d%% | RGB(%d,%d,%d)\n", 
                       rgb_led_get_color_name(), 
                       rgb_led_get_intensity(),
                       rgb_led_get_red(), rgb_led_get_green(), rgb_led_get_blue());
            } else {
                printf("LED RGB: APAGADO\n");
            }
            
            // Información de temperatura
            if (data_ready) {
                printf("Temperatura: %.1f°C\n", current_ntc_data.temperature_c);
                printf("Resistencia NTC: %.0f Ohms | ADC Raw: %d\n",
                       current_ntc_data.resistance, current_ntc_data.raw_adc_value);
            } else {
                printf("Esperando datos del sensor NTC...\n");
            }
            
            // Estado del sistema
            printf("Modo: %s | Impresión: HABILITADA\n", 
                   is_manual_control_active() ? "MANUAL" : "AUTOMÁTICO");
            printf("==========================================\n\n");
        }
        // Cuando la impresión está deshabilitada, NO imprimir NADA
        // El monitor serial permanecerá completamente silencioso
        
        vTaskDelay(pdMS_TO_TICKS(2000));  // Cambiado a 2 segundos
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
    if (pot_queue == NULL) {
        ESP_LOGE(TAG, "Error creando cola del potenciómetro");
        return;
    }
    
    ntc_queue = xQueueCreate(5, sizeof(ntc_data_t));
    if (ntc_queue == NULL) {
        ESP_LOGE(TAG, "Error creando cola del sensor NTC");
        return;
    }
    
    led_queue = xQueueCreate(5, sizeof(uint8_t));  // Para comandos del LED
    if (led_queue == NULL) {
        ESP_LOGE(TAG, "Error creando cola del LED RGB");
        return;
    }
    
    // Obtener cola de comandos LED desde uart_commands
    led_command_queue = get_led_command_queue();
    if (led_command_queue == NULL) {
        ESP_LOGE(TAG, "Error obteniendo cola de comandos LED");
        return;
    }
    
    ESP_LOGI(TAG, "Colas del sistema creadas correctamente");
    
    // ===== CREACIÓN DE TAREAS DEL SISTEMA =====
    ESP_LOGI(TAG, "Creando tareas del sistema...");
    
    if (xTaskCreate(pot_reading_task, "pot_reading_task", 4096, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Error creando tarea de lectura del potenciómetro");
        return;
    }
    
    if (xTaskCreate(ntc_reading_task, "ntc_reading_task", 4096, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Error creando tarea de lectura del sensor NTC");
        return;
    }
    
    
    if (xTaskCreate(display_info_task, "display_info_task", 4096, NULL, 3, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Error creando tarea de visualización");
        return;
    }
    
    if (xTaskCreate(button_task, "button_task", 4096, NULL, 4, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Error creando tarea de control de botón");
        return;
    }
    
    if (xTaskCreate(rgb_control_task, "rgb_control_task", 4096, NULL, 6, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Error creando tarea de control del LED RGB");
        return;
    }
    
    if (xTaskCreate(uart_commands_task, "uart_commands_task", 4096, NULL, 7, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Error creando tarea de comandos UART");
        return;
    }
    
    // ===== SISTEMA INICIADO EXITOSAMENTE =====
    ESP_LOGI(TAG, "=== SISTEMA RTOS INICIADO EXITOSAMENTE ===");
    ESP_LOGI(TAG, "Configuración del hardware:");
    ESP_LOGI(TAG, "  - Potenciómetro: ADC1 CH6 (GPIO34)");
    ESP_LOGI(TAG, "  - Sensor NTC: ADC2 CH9 (GPIO26)");
    ESP_LOGI(TAG, "  - Botón de control: GPIO14");
    ESP_LOGI(TAG, "  - LED RGB: R=GPIO13, G=GPIO12, B=GPIO25");
    ESP_LOGI(TAG, "  - UART: TX=GPIO1, RX=GPIO3 (115200 baud)");
    ESP_LOGI(TAG, "Frecuencias de operación:");
    ESP_LOGI(TAG, "  - Lectura potenciómetro: 4 veces/segundo");
    ESP_LOGI(TAG, "  - Lectura sensor NTC: cada 2 segundos");
    ESP_LOGI(TAG, "  - Control LED RGB: cada 50ms");
    ESP_LOGI(TAG, "  - Monitor serie: cada 1 segundo");
    ESP_LOGI(TAG, "  - Control de botón: cada 10ms");
    ESP_LOGI(TAG, "  - Comandos UART: tiempo real");
    ESP_LOGI(TAG, "Comunicación entre tareas:");
    ESP_LOGI(TAG, "  - Cola potenciómetro: 5 elementos");
    ESP_LOGI(TAG, "  - Cola sensor NTC: 5 elementos");
    ESP_LOGI(TAG, "  - Cola LED RGB: 5 elementos");
    ESP_LOGI(TAG, "Controles:");
    ESP_LOGI(TAG, "  - Pulsación corta: Alternar impresión ON/OFF");
    ESP_LOGI(TAG, "  - Pulsación larga: Evento especial");
    ESP_LOGI(TAG, "  - Potenciómetro: Controla intensidad LED RGB (0-100%)");
    ESP_LOGI(TAG, "  - UART: Comandos para configurar umbrales de temperatura");
    ESP_LOGI(TAG, "=== SISTEMA EN FUNCIONAMIENTO ===");
}