// ===== INCLUDES Y CONFIGURACIÓN =====
#include "uart_commands.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "string.h"
#include "stdio.h"
#include "stdlib.h"
#include "rgb_led.h"

static const char *TAG = "UART_COMMANDS";

// ===== CONFIGURACIÓN UART =====
#define UART_NUM UART_NUM_0
#define BUF_SIZE 1024
#define UART_TX_PIN 1   // Usar GPIO1 para TX
#define UART_RX_PIN 3   // Usar GPIO3 para RX

// ===== VARIABLES GLOBALES =====
static temp_thresholds_t current_thresholds = {
    .r_min = 0.0,   // Rojo: 0-15°C
    .r_max = 15.0,
    .g_min = 10.0,  // Verde: 10-30°C
    .g_max = 30.0,
    .b_min = 40.0,  // Azul: 40-50°C
    .b_max = 50.0
};

static bool uart_initialized = false;
static bool manual_control_active = false;  // Flag para control manual
static bool temperature_control_initialized = false;  // Flag para control de temperatura inicializado
static QueueHandle_t led_command_queue = NULL;  // Cola para comandos LED

// ===== FUNCIONES PRIVADAS =====
static void uart_init(void)
{
    if (uart_initialized) return;
    
    ESP_LOGI(TAG, "Inicializando UART...");
    
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM, BUF_SIZE * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    
    uart_initialized = true;
    ESP_LOGI(TAG, "UART inicializado correctamente en GPIO%d (TX) y GPIO%d (RX)", UART_TX_PIN, UART_RX_PIN);
    printf("=== UART INICIADO ===\n");
    printf("TX: GPIO%d, RX: GPIO%d\n", UART_TX_PIN, UART_RX_PIN);
    printf("Velocidad: 115200 baudios\n");
    printf("=====================\n");
}


static uart_command_t parse_command(const char* input)
{
    uart_command_t cmd = {0};
    cmd.valid = false;
    
    char buffer[64];
    strncpy(buffer, input, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    
    // Eliminar caracteres de nueva línea
    char* newline = strchr(buffer, '\n');
    if (newline) *newline = '\0';
    newline = strchr(buffer, '\r');
    if (newline) *newline = '\0';
    
    // Convertir a minúsculas
    for (int i = 0; buffer[i]; i++) {
        if (buffer[i] >= 'A' && buffer[i] <= 'Z') {
            buffer[i] = buffer[i] - 'A' + 'a';
        }
    }
    
    if (strcmp(buffer, "help") == 0) {
        strcpy(cmd.command, "help");
        cmd.valid = true;
    }
    else if (strcmp(buffer, "status") == 0) {
        strcpy(cmd.command, "status");
        cmd.valid = true;
    }
    else if (strcmp(buffer, "manual_on") == 0) {
        strcpy(cmd.command, "manual_on");
        cmd.valid = true;
    }
    else if (strcmp(buffer, "manual_off") == 0) {
        strcpy(cmd.command, "manual_off");
        cmd.valid = true;
    }
    else if (strcmp(buffer, "temp_control_off") == 0) {
        strcpy(cmd.command, "temp_control_off");
        cmd.valid = true;
    }
    else if (strcmp(buffer, "led_on") == 0) {
        strcpy(cmd.command, "led_on");
        cmd.valid = true;
    }
    else if (strcmp(buffer, "led_off") == 0) {
        strcpy(cmd.command, "led_off");
        cmd.valid = true;
    }
    else if (strncmp(buffer, "set_color", 9) == 0) {
        strcpy(cmd.command, "set_color");
        int temp_red, temp_green, temp_blue;
        if (sscanf(buffer, "set_color %d %d %d", &temp_red, &temp_green, &temp_blue) == 3) {
            cmd.red = (uint8_t)temp_red;
            cmd.green = (uint8_t)temp_green;
            cmd.blue = (uint8_t)temp_blue;
            cmd.valid = true;
        }
    }
    else if (strncmp(buffer, "set_red", 7) == 0) {
        strcpy(cmd.command, "set_red");
        if (sscanf(buffer, "set_red %f %f", &cmd.value1, &cmd.value2) == 2) {
            cmd.valid = true;
        }
    }
    else if (strncmp(buffer, "set_green", 9) == 0) {
        strcpy(cmd.command, "set_green");
        if (sscanf(buffer, "set_green %f %f", &cmd.value1, &cmd.value2) == 2) {
            cmd.valid = true;
        }
    }
    else if (strncmp(buffer, "set_blue", 8) == 0) {
        strcpy(cmd.command, "set_blue");
        if (sscanf(buffer, "set_blue %f %f", &cmd.value1, &cmd.value2) == 2) {
            cmd.valid = true;
        }
    }
    else if (strncmp(buffer, "set_all", 7) == 0) {
        strcpy(cmd.command, "set_all");
        if (sscanf(buffer, "set_all %f %f %f %f %f %f", 
                   &cmd.value1, &cmd.value2, &cmd.value1, &cmd.value2, &cmd.value1, &cmd.value2) == 6) {
            cmd.valid = true;
        }
    }
    
    return cmd;
}

static void execute_command(const uart_command_t* cmd)
{
    if (!cmd->valid) {
        printf("Comando inválido. Escriba 'help' para ver comandos disponibles.\n");
        return;
    }
    
    if (strcmp(cmd->command, "help") == 0) {
        print_help();
    }
    else if (strcmp(cmd->command, "status") == 0) {
        print_current_thresholds();
    }
    else if (strcmp(cmd->command, "manual_on") == 0) {
        manual_control_active = true;
        printf("Control manual activado - El LED no cambiará automáticamente\n");
    }
    else if (strcmp(cmd->command, "manual_off") == 0) {
        manual_control_active = false;
        printf("Control automático activado - El LED cambiará según temperatura\n");
    }
    else if (strcmp(cmd->command, "temp_control_off") == 0) {
        temperature_control_initialized = false;
        rgb_led_off();
        printf("Control de temperatura DESACTIVADO - LED apagado\n");
    }
    else if (strcmp(cmd->command, "led_on") == 0) {
        send_led_command("led_on", 255, 255, 255, true);
        printf("Comando LED enviado: encender en blanco\n");
    }
    else if (strcmp(cmd->command, "led_off") == 0) {
        send_led_command("led_off", 0, 0, 0, true);  // Cambiar a true para forzar apagado
        printf("Comando LED enviado: apagar\n");
    }
    else if (strcmp(cmd->command, "set_color") == 0) {
        send_led_command("set_color", cmd->red, cmd->green, cmd->blue, true);
        printf("Comando LED enviado: color R=%d, G=%d, B=%d\n", 
               cmd->red, cmd->green, cmd->blue);
    }
    else if (strcmp(cmd->command, "set_red") == 0) {
        if (cmd->value1 < cmd->value2) {
            current_thresholds.r_min = cmd->value1;
            current_thresholds.r_max = cmd->value2;
            temperature_control_initialized = true;  // Activar control de temperatura
            printf("Umbrales rojos actualizados: %.1f°C - %.1f°C\n", cmd->value1, cmd->value2);
            // Encender LED en rojo para mostrar el color configurado
            send_led_command("set_color", 255, 0, 0, true);
            printf("LED encendido en ROJO para mostrar el color configurado\n");
            printf("El LED se mantendrá encendido solo cuando la temperatura esté entre %.1f°C y %.1f°C\n", cmd->value1, cmd->value2);
            printf("Control automático de temperatura ACTIVADO\n");
        } else {
            printf("Error: El valor mínimo debe ser menor que el máximo\n");
        }
    }
    else if (strcmp(cmd->command, "set_green") == 0) {
        if (cmd->value1 < cmd->value2) {
            current_thresholds.g_min = cmd->value1;
            current_thresholds.g_max = cmd->value2;
            temperature_control_initialized = true;  // Activar control de temperatura
            printf("Umbrales verdes actualizados: %.1f°C - %.1f°C\n", cmd->value1, cmd->value2);
            // Encender LED en verde para mostrar el color configurado
            send_led_command("set_color", 0, 255, 0, true);
            printf("LED encendido en VERDE para mostrar el color configurado\n");
            printf("El LED se mantendrá encendido solo cuando la temperatura esté entre %.1f°C y %.1f°C\n", cmd->value1, cmd->value2);
            printf("Control automático de temperatura ACTIVADO\n");
        } else {
            printf("Error: El valor mínimo debe ser menor que el máximo\n");
        }
    }
    else if (strcmp(cmd->command, "set_blue") == 0) {
        if (cmd->value1 < cmd->value2) {
            current_thresholds.b_min = cmd->value1;
            current_thresholds.b_max = cmd->value2;
            temperature_control_initialized = true;  // Activar control de temperatura
            printf("Umbrales azules actualizados: %.1f°C - %.1f°C\n", cmd->value1, cmd->value2);
            // Encender LED en azul para mostrar el color configurado
            send_led_command("set_color", 0, 0, 255, true);
            printf("LED encendido en AZUL para mostrar el color configurado\n");
            printf("El LED se mantendrá encendido solo cuando la temperatura esté entre %.1f°C y %.1f°C\n", cmd->value1, cmd->value2);
            printf("Control automático de temperatura ACTIVADO\n");
        } else {
            printf("Error: El valor mínimo debe ser menor que el máximo\n");
        }
    }
}

// ===== FUNCIONES PÚBLICAS =====
void uart_commands_init(void)
{
    uart_init();
    
    // Crear cola para comandos LED
    led_command_queue = xQueueCreate(10, sizeof(led_command_t));
    if (led_command_queue == NULL) {
        ESP_LOGE(TAG, "Error creando cola de comandos LED");
        return;
    }
    
    ESP_LOGI(TAG, "Sistema de comandos UART inicializado");
    printf("\n=== SISTEMA DE CONTROL RGB POR TEMPERATURA ===\n");
    printf("Comandos disponibles:\n");
    print_help();
    printf("===============================================\n\n");
}

void uart_commands_task(void *arg)
{
    uint8_t data[BUF_SIZE];
    int len;
    
    ESP_LOGI(TAG, "Tarea de comandos UART iniciada");
    printf("=== TAREA UART INICIADA ===\n");
    printf("Esperando comandos...\n");
    printf("Comandos disponibles: help, status, set_red, set_green, set_blue\n");
    printf("===========================\n");
    
    while (1) {
        len = uart_read_bytes(UART_NUM, data, BUF_SIZE - 1, pdMS_TO_TICKS(100));
        if (len > 0) {
            data[len] = '\0';
            printf("Comando recibido: %s\n", (char*)data);
            
            // Procesar comando
            uart_command_t cmd = parse_command((char*)data);
            execute_command(&cmd);
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

temp_thresholds_t* get_temp_thresholds(void)
{
    return &current_thresholds;
}

void set_temp_thresholds(float r_min, float r_max, float g_min, float g_max, float b_min, float b_max)
{
    current_thresholds.r_min = r_min;
    current_thresholds.r_max = r_max;
    current_thresholds.g_min = g_min;
    current_thresholds.g_max = g_max;
    current_thresholds.b_min = b_min;
    current_thresholds.b_max = b_max;
    
    ESP_LOGI(TAG, "Umbrales actualizados: R(%.1f-%.1f), G(%.1f-%.1f), B(%.1f-%.1f)", 
             r_min, r_max, g_min, g_max, b_min, b_max);
}

void process_uart_command(const char* command)
{
    uart_command_t cmd = parse_command(command);
    execute_command(&cmd);
}

void print_help(void)
{
    printf("Comandos disponibles:\n");
    printf("  help                    - Mostrar esta ayuda\n");
    printf("  status                  - Mostrar umbrales actuales y estado\n");
    printf("  manual_on               - Activar control manual del LED\n");
    printf("  manual_off              - Activar control automático por temperatura\n");
    printf("  temp_control_off         - Desactivar control de temperatura (LED apagado)\n");
    printf("  led_on                  - Encender LED en blanco (control manual)\n");
    printf("  led_off                 - Apagar LED\n");
    printf("  set_color <r> <g> <b>   - Establecer color RGB (0-255) (ej: set_color 255 0 0)\n");
    printf("  set_red <min> <max>     - Configurar umbrales rojos y mostrar color (ej: set_red 0 15)\n");
    printf("  set_green <min> <max>   - Configurar umbrales verdes y mostrar color (ej: set_green 10 30)\n");
    printf("  set_blue <min> <max>    - Configurar umbrales azules y mostrar color (ej: set_blue 40 50)\n");
    printf("\nEjemplos:\n");
    printf("  set_red 0 15            - Configurar rojo (0-15°C) y mostrar LED rojo\n");
    printf("  set_green 10 30         - Configurar verde (10-30°C) y mostrar LED verde\n");
    printf("  set_blue 40 50          - Configurar azul (40-50°C) y mostrar LED azul\n");
    printf("  set_color 255 0 0       - LED rojo manual\n");
    printf("  set_color 0 255 0       - LED verde manual\n");
    printf("  set_color 0 0 255       - LED azul manual\n");
    printf("  set_color 255 255 0     - LED amarillo\n");
    printf("  manual_on               - Activar control manual\n");
    printf("  manual_off              - Activar control automático\n");
}

void print_current_thresholds(void)
{
    printf("\n=== ESTADO DEL SISTEMA ===\n");
    printf("Umbrales de temperatura:\n");
    printf("  Rojo:   %.1f°C - %.1f°C\n", current_thresholds.r_min, current_thresholds.r_max);
    printf("  Verde:  %.1f°C - %.1f°C\n", current_thresholds.g_min, current_thresholds.g_max);
    printf("  Azul:   %.1f°C - %.1f°C\n", current_thresholds.b_min, current_thresholds.b_max);
    printf("\nModo de control: %s\n", manual_control_active ? "MANUAL" : "AUTOMÁTICO");
    
    // Mostrar comportamiento específico de la lógica
    printf("\nComportamiento específico del LED:\n");
    
    // Calcular rangos específicos
    float red_only_start = current_thresholds.r_min;
    float red_only_end = (current_thresholds.r_max < current_thresholds.g_min) ? current_thresholds.r_max : current_thresholds.g_min;
    float overlap_start = (current_thresholds.r_min > current_thresholds.g_min) ? current_thresholds.r_min : current_thresholds.g_min;
    float overlap_end = (current_thresholds.r_max < current_thresholds.g_max) ? current_thresholds.r_max : current_thresholds.g_max;
    float green_only_start = (current_thresholds.g_min > current_thresholds.r_max) ? current_thresholds.g_min : current_thresholds.r_max + 1;
    float green_only_end = current_thresholds.g_max;
    
    if (red_only_start < red_only_end) {
        printf("  🔴 Solo Rojo: %.1f°C - %.1f°C -> LED ROJO PURO\n", red_only_start, red_only_end);
    }
    if (overlap_start <= overlap_end) {
        printf("  🟡 Rojo + Verde: %.1f°C - %.1f°C -> LED AMARILLO\n", overlap_start, overlap_end);
    }
    if (green_only_start <= green_only_end) {
        printf("  🟢 Solo Verde: %.1f°C - %.1f°C -> LED VERDE PURO\n", green_only_start, green_only_end);
    }
    printf("  🔵 Solo Azul: %.1f°C - %.1f°C -> LED AZUL PURO\n", current_thresholds.b_min, current_thresholds.b_max);
    
    printf("\nCaracterísticas del sistema:\n");
    printf("  - El LED se mantiene encendido SOLO en el rango de temperatura configurado\n");
    printf("  - Se APAGA cuando la temperatura está fuera de los rangos configurados\n");
    printf("  - Los rangos superpuestos generan colores mezclados automáticamente\n");
    printf("  - En modo manual, el potenciómetro NO afecta el LED\n");
    
    printf("========================\n\n");
}

bool is_manual_control_active(void)
{
    return manual_control_active;
}

bool is_temperature_control_initialized(void)
{
    return temperature_control_initialized;
}

// ===== FUNCIONES DE COLA DE COMANDOS LED =====
QueueHandle_t get_led_command_queue(void)
{
    return led_command_queue;
}

void send_led_command(const char* command, uint8_t red, uint8_t green, uint8_t blue, bool manual_control)
{
    if (led_command_queue == NULL) {
        ESP_LOGE(TAG, "Cola de comandos LED no inicializada");
        return;
    }
    
    led_command_t led_cmd;
    strncpy(led_cmd.command, command, sizeof(led_cmd.command) - 1);
    led_cmd.command[sizeof(led_cmd.command) - 1] = '\0';
    led_cmd.red = red;
    led_cmd.green = green;
    led_cmd.blue = blue;
    led_cmd.manual_control = manual_control;
    
    BaseType_t result = xQueueSend(led_command_queue, &led_cmd, pdMS_TO_TICKS(100));
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Error enviando comando LED a la cola");
    } else {
        ESP_LOGD(TAG, "Comando LED enviado: %s (R=%d, G=%d, B=%d)", 
                 command, red, green, blue);
    }
}
