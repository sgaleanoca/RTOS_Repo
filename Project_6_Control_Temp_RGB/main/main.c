/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "esp_log.h"

// Include our custom modules
#include "uart_handler.h"
#include "temp_sensor.h"
#include "rgb_led.h"
#include "button_handler.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    ESP_LOGI(TAG, "Starting Temperature Control RGB System");

    /* Print chip information */
    esp_chip_info_t chip_info;
    uint32_t flash_size;
    esp_chip_info(&chip_info);
    printf("This is %s chip with %d CPU core(s), %s%s%s%s, ",
           CONFIG_IDF_TARGET,
           chip_info.cores,
           (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "WiFi/" : "",
           (chip_info.features & CHIP_FEATURE_BT) ? "BT" : "",
           (chip_info.features & CHIP_FEATURE_BLE) ? "BLE" : "",
           (chip_info.features & CHIP_FEATURE_IEEE802154) ? ", 802.15.4 (Zigbee/Thread)" : "");

    unsigned major_rev = chip_info.revision / 100;
    unsigned minor_rev = chip_info.revision % 100;
    printf("silicon revision v%d.%d, ", major_rev, minor_rev);
    if(esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
        printf("Get flash size failed");
        return;
    }

    printf("%" PRIu32 "MB %s flash\n", flash_size / (uint32_t)(1024 * 1024),
           (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    printf("Minimum free heap size: %" PRIu32 " bytes\n", esp_get_minimum_free_heap_size());

    // Initialize all modules
    ESP_LOGI(TAG, "Initializing modules...");
    
    // Initialize UART handler
    uart_init();
    
    // Initialize temperature sensor
    temp_sensor_init();
    
    // Initialize RGB LED
    rgb_led_init();
    
    // Initialize button handler
    button_handler_init();
    
    ESP_LOGI(TAG, "All modules initialized successfully");

    // Create tasks
    ESP_LOGI(TAG, "Creating tasks...");
    
    // UART task for command handling
    xTaskCreate(uart_task, "uart_task", 4096, NULL, 5, NULL);
    
    // Temperature sensor task
    xTaskCreate(temp_sensor_task, "temp_sensor_task", 4096, NULL, 3, NULL);
    
    // RGB LED control task
    xTaskCreate(rgb_led_task, "rgb_led_task", 4096, NULL, 4, NULL);
    
    // Button handler task
    xTaskCreate(button_handler_task, "button_handler_task", 4096, NULL, 2, NULL);
    
    ESP_LOGI(TAG, "All tasks created successfully");
    ESP_LOGI(TAG, "System ready! Use UART commands to interact with the system.");
    ESP_LOGI(TAG, "Type 'HELP' for available commands.");
    
    // Main loop - handle button events
    button_event_t button_event;
    while (1) {
        if (xQueueReceive(button_event_queue, &button_event, portMAX_DELAY)) {
            switch (button_event) {
                case BUTTON_EVENT_PRESS:
                    ESP_LOGI(TAG, "Button short press - toggling print status");
                    button_handler_toggle_print();
                    break;
                    
                case BUTTON_EVENT_LONG_PRESS:
                    ESP_LOGI(TAG, "Button long press - system status");
                    button_handler_get_status();
                    break;
                    
                default:
                    break;
            }
        }
    }
}
