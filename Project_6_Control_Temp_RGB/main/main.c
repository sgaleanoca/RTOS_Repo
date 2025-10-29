// ===== INCLUDES Y CONFIGURACIÓN =====
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

#include "ntc_sensor.h"
#include "button_control.h"
#include "potentiometer.h"
#include "rgb_led.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_vfs_dev.h"

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
// Cola para comandos recibidos por UART
typedef enum { CMD_SET_RED, CMD_SET_GREEN, CMD_SET_BLUE, CMD_SET_POT } command_type_t; 
typedef struct {
    command_type_t type;
    float value1; // min_temp o canal del pot (1=r,2=g,3=b,0=none)
    float value2; // max_temp
} app_command_t;
static QueueHandle_t command_queue = NULL; 

// Prototipo de tarea UART
void uart_receiver_task(void *arg); //

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
// Tarea: Lee el potenciómetro periódicamente y envía porcentaje y mV a la cola
void pot_reading_task(void *arg)
{
    pot_data_t pot_data;
    static uint32_t last_log_time = 0;
    
    ESP_LOGI(TAG, "Tarea de lectura del potenciómetro iniciada");
    
    while (1) {
        pot_data.pot_percent = pot_get_percent();
        pot_data.pot_voltage_mv = pot_get_voltage_mv();
        
        if (pot_queue != NULL) xQueueSend(pot_queue, &pot_data, pdMS_TO_TICKS(10));
        current_pot_data = pot_data;
        
        // La intensidad del LED se controla desde la tarea de RGB para evitar conflictos
        
        uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if (current_time - last_log_time >= 2000) last_log_time = current_time;
        
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

// Tarea: Lee temperatura del NTC, valida rangos y publica datos a la cola
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
// Tarea: Controla el LED RGB según temperatura (automático) o potenciómetro (manual)
void rgb_control_task(void *arg)
{
    pot_data_t pot_data;
    ntc_data_t ntc_data;
    BaseType_t pot_status, ntc_status;
    app_command_t received_cmd;

    // Umbrales por defecto (°C)
    float red_min = 0.0f,  red_max = 15.0f;
    float green_min = 10.0f, green_max = 30.0f;
    float blue_min = 25.0f, blue_max = 40.0f;
    int pot_control_target = 0; // 0: none, 1: red, 2: green, 3: blue
    bool any_color_active = false; // indica si hay color activo en modo automático
    
    ESP_LOGI(TAG, "Tarea de control del LED RGB iniciada");
    
    while (1) {
        // Procesar comandos UART si existen (no bloqueante)
        // Gestión de comandos recibidos por UART para ajustar umbrales o control del pot
        if (command_queue != NULL && xQueueReceive(command_queue, &received_cmd, 0) == pdPASS) {
            switch (received_cmd.type) {
                case CMD_SET_RED:
                    red_min = received_cmd.value1;
                    red_max = (received_cmd.value2 <= received_cmd.value1) ? (received_cmd.value1 + 0.1f) : received_cmd.value2; 
                    break;
                case CMD_SET_GREEN:
                    green_min = received_cmd.value1;
                    green_max = (received_cmd.value2 <= received_cmd.value1) ? (received_cmd.value1 + 0.1f) : received_cmd.value2;
                    break;
                case CMD_SET_BLUE:
                    blue_min = received_cmd.value1;
                    blue_max = (received_cmd.value2 <= received_cmd.value1) ? (received_cmd.value1 + 0.1f) : received_cmd.value2;
                    break;
                case CMD_SET_POT:
                    pot_control_target = (int)received_cmd.value1; // 0,1,2,3
                    break;
            }
        }
        
        // Leer pot sin bloquear; se apoya en current_pot_data actualizado por su tarea
        pot_status = xQueueReceive(pot_queue, &pot_data, pdMS_TO_TICKS(1));
        (void)pot_status; (void)pot_data;

        // Modo manual: si el pot controla un color, se aplica intensidad y se omite control por NTC
        if (pot_control_target != 0) {
            uint8_t intensity = current_pot_data.pot_percent;
            if (pot_control_target == 1) {
                rgb_led_set_color(255, 0, 0);
            } else if (pot_control_target == 2) {
                rgb_led_set_color(0, 255, 0);
            } else if (pot_control_target == 3) {
                rgb_led_set_color(0, 0, 255);
            }
            rgb_led_set_intensity(intensity);
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        
        // Modo automático: decidir color según rangos configurables de temperatura
        ntc_status = xQueueReceive(ntc_queue, &ntc_data, pdMS_TO_TICKS(10));
        if (ntc_status == pdPASS) {
            float temp = ntc_data.temperature_c;
            bool in_red   = (temp >= red_min && temp <= red_max);
            bool in_green = (temp >= green_min && temp <= green_max);
            bool in_blue  = (temp >= blue_min && temp <= blue_max);

            // Resolver solapamientos de rangos mezclando colores (blanco/amarillo/magenta/cian)
            if (!in_red && !in_green && !in_blue) {
                rgb_led_off();
                any_color_active = false;
            } else if (in_red && in_green && in_blue) {
                rgb_led_set_color(255, 255, 255);   // Blanco: solapado de los tres
                any_color_active = true;
            } else if (in_red && in_green) {
                rgb_led_set_color(255, 255, 0);     // Amarillo: R+G
                any_color_active = true;
            } else if (in_red && in_blue) {
                rgb_led_set_color(255, 0, 255);     // Magenta: R+B
                any_color_active = true;
            } else if (in_green && in_blue) {
                rgb_led_set_color(0, 255, 255);     // Cian: G+B
                any_color_active = true;
            } else if (in_red) {
                rgb_led_set_color(255, 0, 0);
                any_color_active = true;
            } else if (in_green) {
                rgb_led_set_color(0, 255, 0);
                any_color_active = true;
            } else { // in_blue
                rgb_led_set_color(0, 0, 255);
                any_color_active = true;
            }
        }
        // Actualizar intensidad desde el pot incluso en modo automático si hay color activo
        if (any_color_active) {
            rgb_led_set_intensity(current_pot_data.pot_percent);
        }

        vTaskDelay(pdMS_TO_TICKS(50));  // Control cada 50ms
    }
}

// Tarea: Muestra por consola el estado del sistema cada 2s si la impresión está habilitada
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
            
            printf("Modo: AUTOMÁTICO | Control Temp: ACTIVADO | Impresión: HABILITADA\n");
            printf("==========================================\n\n");
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}


// ===== FUNCIÓN PRINCIPAL DEL SISTEMA =====
// Punto de entrada: inicializa hardware, crea colas y tareas del sistema
void app_main(void)
{
    ESP_LOGI(TAG, "=== INICIANDO SISTEMA RTOS - SENSOR NTC ===");
    ESP_LOGI(TAG, "Inicializando componentes de hardware...");
    
    // ===== INICIALIZACIÓN DE HARDWARE =====
    ntc_sensor_init();
    button_control_init();
    pot_init();
    rgb_led_init();
    // Configurar UART0 para recepción de comandos a 115200-8N1
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_driver_install(UART_NUM_0, 1024, 0, 0, NULL, 0);
    uart_param_config(UART_NUM_0, &uart_config);
    uart_set_pin(UART_NUM_0, GPIO_NUM_1, GPIO_NUM_3, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    esp_vfs_dev_uart_use_driver(UART_NUM_0);
    uart_flush_input(UART_NUM_0);
    
    ESP_LOGI(TAG, "Hardware inicializado correctamente");
    
    // ===== CREACIÓN DE COLAS DEL SISTEMA =====
    ESP_LOGI(TAG, "Creando colas del sistema...");
    
    pot_queue = xQueueCreate(5, sizeof(pot_data_t));
    ntc_queue = xQueueCreate(5, sizeof(ntc_data_t));
    led_queue = xQueueCreate(5, sizeof(uint8_t));
    // Cola de comandos UART
    command_queue = xQueueCreate(5, sizeof(app_command_t));
    
    if (pot_queue == NULL || ntc_queue == NULL || led_queue == NULL || command_queue == NULL) {
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
        xTaskCreate(uart_receiver_task, "uart_receiver_task", 4096, NULL, 7, NULL) != pdPASS) {
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

// ====== TAREA: Recepción de comandos por UART ======
// Muestra el menú de ayuda para comandos UART
static void print_help_menu(void)
{
    printf("\n\n=== MODO CONFIGURACIÓN ===\n");
    printf("Comandos disponibles:\n");
    printf("----------------------------------------------------------\n");
    printf("  R <min> <max>   -> Rango temp para ROJO (ej: R 0 15.5)\n");
    printf("  G <min> <max>   -> Rango temp para VERDE (ej: G 10 30)\n");
    printf("  B <min> <max>   -> Rango temp para AZUL (ej: B 25 40)\n");
    printf("  pot <r|g|b|none>-> Asigna el potenciómetro a un color\n");
    printf("  help            -> Muestra este menú\n");
    printf("----------------------------------------------------------\n\n");
    printf("> ");
    fflush(stdout);
}

// Tarea: Lee comandos por UART, los parsea y los envía a la cola de comandos
void uart_receiver_task(void *arg)
{
    uint8_t *data = (uint8_t *)malloc(256);
    if (!data) {
        ESP_LOGE(TAG, "Sin memoria para buffer UART");
        vTaskDelete(NULL);
        return;
    }
    print_help_menu();
    while (1) {
        int len = uart_read_bytes(UART_NUM_0, data, 255, pdMS_TO_TICKS(50));
        if (len > 0) {
            data[len] = '\0';
            // Normalizar fin de línea
            for (int i = 0; i < len; ++i) { if (data[i] == '\r' || data[i] == '\n') data[i] = ' '; }
            app_command_t cmd; char type_char; char pot_target[8] = {0};

            if (strncmp((char*)data, "help", 4) == 0) {
                print_help_menu();
                continue;
            }

            if (sscanf((char*)data, " %c %f %f", &type_char, &cmd.value1, &cmd.value2) == 3) {
                bool send = true;
                if (type_char == 'R' || type_char == 'r') cmd.type = CMD_SET_RED;
                else if (type_char == 'G' || type_char == 'g') cmd.type = CMD_SET_GREEN;
                else if (type_char == 'B' || type_char == 'b') cmd.type = CMD_SET_BLUE;
                else send = false;
                if (send) {
                    (void)xQueueSend(command_queue, &cmd, pdMS_TO_TICKS(10));
                    printf("OK: %c %.2f %.2f\n> ", type_char, cmd.value1, cmd.value2);
                    fflush(stdout);
                }
                continue;
            }

            if (sscanf((char*)data, "pot %7s", pot_target) == 1) {
                cmd.type = CMD_SET_POT;
                if (strcmp(pot_target, "r") == 0 || strcmp(pot_target, "R") == 0) cmd.value1 = 1;
                else if (strcmp(pot_target, "g") == 0 || strcmp(pot_target, "G") == 0) cmd.value1 = 2;
                else if (strcmp(pot_target, "b") == 0 || strcmp(pot_target, "B") == 0) cmd.value1 = 3;
                else cmd.value1 = 0; // none
                (void)xQueueSend(command_queue, &cmd, pdMS_TO_TICKS(10));
                printf("OK: pot %s\n> ", pot_target);
                fflush(stdout);
                continue;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}