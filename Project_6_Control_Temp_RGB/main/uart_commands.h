#ifndef UART_COMMANDS_H
#define UART_COMMANDS_H

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// Estructura para almacenar umbrales de temperatura
typedef struct {
    float r_min;    // Temperatura mínima para rojo
    float r_max;    // Temperatura máxima para rojo
    float g_min;    // Temperatura mínima para verde
    float g_max;    // Temperatura máxima para verde
    float b_min;    // Temperatura mínima para azul
    float b_max;    // Temperatura máxima para azul
} temp_thresholds_t;

// Estructura para comandos UART
typedef struct {
    char command[32];
    float value1;
    float value2;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    bool valid;
} uart_command_t;

// Estructura para comandos LED
typedef struct {
    char command[16];
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    bool manual_control;
} led_command_t;

// Funciones públicas
void uart_commands_init(void);
void uart_commands_task(void *arg);
temp_thresholds_t* get_temp_thresholds(void);
void set_temp_thresholds(float r_min, float r_max, float g_min, float g_max, float b_min, float b_max);
void process_uart_command(const char* command);
void print_help(void);
void print_current_thresholds(void);
bool is_manual_control_active(void);
bool is_temperature_control_initialized(void);

// Funciones para cola de comandos LED
QueueHandle_t get_led_command_queue(void);
void send_led_command(const char* command, uint8_t red, uint8_t green, uint8_t blue, bool manual_control);

#endif // UART_COMMANDS_H
