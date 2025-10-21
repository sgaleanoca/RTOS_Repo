#include "uart_handler.h"
#include "temp_sensor.h"
#include "rgb_led.h"
#include "button_handler.h"
#include <string.h>
#include <stdio.h>
#include "esp_log.h"

static const char *TAG = "UART_HANDLER";

// Queue for UART commands
QueueHandle_t uart_cmd_queue;

void uart_init(void) {
    // Configure UART parameters
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    
    // Install UART driver
    uart_driver_install(UART_NUM, BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_NUM, &uart_config);
    uart_set_pin(UART_NUM, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    
    // Create command queue
    uart_cmd_queue = xQueueCreate(10, sizeof(uart_cmd_t));
    
    ESP_LOGI(TAG, "UART initialized");
}

void uart_task(void *pvParameters) {
    uint8_t data[RD_BUF_SIZE];
    char command[64];
    int command_len = 0;
    
    while (1) {
        int len = uart_read_bytes(UART_NUM, data, RD_BUF_SIZE, portMAX_DELAY);
        
        for (int i = 0; i < len; i++) {
            if (data[i] == '\n' || data[i] == '\r') {
                if (command_len > 0) {
                    command[command_len] = '\0';
                    uart_process_command(command);
                    command_len = 0;
                }
            } else if (command_len < sizeof(command) - 1) {
                command[command_len++] = data[i];
            }
        }
    }
}

void uart_process_command(const char* cmd) {
    uart_cmd_t command = {CMD_UNKNOWN, 0, 0, 0};
    
    if (strcmp(cmd, "GET_TEMP") == 0) {
        command.type = CMD_GET_TEMP;
        uart_send_temperature(temp_sensor_read());
    }
    else if (strncmp(cmd, "SET_RGB", 7) == 0) {
        int r, g, b;
        if (sscanf(cmd, "SET_RGB %d %d %d", &r, &g, &b) == 3) {
            command.type = CMD_SET_RGB;
            command.param1 = r;
            command.param2 = g;
            command.param3 = b;
            rgb_led_set_color(r, g, b);
            uart_send_response("RGB set successfully");
        } else {
            uart_send_response("Invalid RGB format. Use: SET_RGB r g b");
        }
    }
    else if (strcmp(cmd, "GET_STATUS") == 0) {
        command.type = CMD_GET_STATUS;
        uart_send_system_status();
    }
    else if (strcmp(cmd, "HELP") == 0) {
        uart_send_response("Available commands:");
        uart_send_response("GET_TEMP - Get current temperature");
        uart_send_response("SET_RGB r g b - Set RGB color (0-255)");
        uart_send_response("GET_STATUS - Get system status");
        uart_send_response("HELP - Show this help");
    }
    else {
        uart_send_response("Unknown command. Type HELP for available commands.");
    }
    
    // Send command to queue if it's a valid command
    if (command.type != CMD_UNKNOWN) {
        xQueueSend(uart_cmd_queue, &command, 0);
    }
}

void uart_send_response(const char* response) {
    uart_write_bytes(UART_NUM, response, strlen(response));
    uart_write_bytes(UART_NUM, "\n", 1);
}

void uart_send_temperature(float temp) {
    char response[64];
    snprintf(response, sizeof(response), "Temperature: %.2f°C", temp);
    uart_send_response(response);
}

void uart_send_rgb_status(int r, int g, int b) {
    char response[64];
    snprintf(response, sizeof(response), "RGB Status: R=%d G=%d B=%d", r, g, b);
    uart_send_response(response);
}

void uart_send_system_status(void) {
    uart_send_response("=== System Status ===");
    uart_send_temperature(temp_sensor_read());
    rgb_led_get_status();
    button_handler_get_status();
    uart_send_response("===================");
}
