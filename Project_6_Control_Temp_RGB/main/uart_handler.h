#ifndef UART_HANDLER_H
#define UART_HANDLER_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "driver/uart.h"

// UART Configuration
#define UART_NUM UART_NUM_0
#define BUF_SIZE 1024
#define RD_BUF_SIZE BUF_SIZE

// Command types
typedef enum {
    CMD_GET_TEMP,
    CMD_SET_RGB,
    CMD_GET_STATUS,
    CMD_UNKNOWN
} uart_command_t;

// Command structure
typedef struct {
    uart_command_t type;
    int param1;
    int param2;
    int param3;
} uart_cmd_t;

// Function prototypes
void uart_init(void);
void uart_task(void *pvParameters);
void uart_send_response(const char* response);
void uart_send_temperature(float temp);
void uart_send_rgb_status(int r, int g, int b);
void uart_send_system_status(void);

// External queue for commands
extern QueueHandle_t uart_cmd_queue;

#endif // UART_HANDLER_H
