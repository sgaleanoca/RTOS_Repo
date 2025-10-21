#include "button_handler.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include <string.h>

static const char *TAG = "BUTTON_HANDLER";

// Global variables
bool print_enabled = true;
QueueHandle_t button_event_queue;

// Button state tracking
static button_state_t last_button_state = BUTTON_RELEASED;
static uint32_t button_press_start_time = 0;
static bool button_pressed = false;

void button_handler_init(void) {
    // Configure button GPIO
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&io_conf);
    
    // Create button event queue
    button_event_queue = xQueueCreate(10, sizeof(button_event_t));
    
    ESP_LOGI(TAG, "Button handler initialized");
}

void button_handler_task(void *pvParameters) {
    button_event_t event;
    uint32_t current_time;
    
    while (1) {
        current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        
        // Read button state
        int button_level = gpio_get_level(BUTTON_GPIO);
        button_state_t current_state = (button_level == 0) ? BUTTON_PRESSED : BUTTON_RELEASED;
        
        // Handle button press
        if (current_state == BUTTON_PRESSED && last_button_state == BUTTON_RELEASED) {
            button_press_start_time = current_time;
            button_pressed = true;
            ESP_LOGI(TAG, "Button pressed");
        }
        
        // Handle button release
        if (current_state == BUTTON_RELEASED && last_button_state == BUTTON_PRESSED) {
            if (button_pressed) {
                uint32_t press_duration = current_time - button_press_start_time;
                
                if (press_duration >= BUTTON_LONG_PRESS_TIME_MS) {
                    // Long press
                    event = BUTTON_EVENT_LONG_PRESS;
                    ESP_LOGI(TAG, "Button long press detected");
                } else {
                    // Short press
                    event = BUTTON_EVENT_PRESS;
                    ESP_LOGI(TAG, "Button short press detected");
                }
                
                // Send event to queue
                xQueueSend(button_event_queue, &event, 0);
                button_pressed = false;
            }
        }
        
        // Handle long press while button is still pressed
        if (current_state == BUTTON_PRESSED && button_pressed) {
            uint32_t press_duration = current_time - button_press_start_time;
            if (press_duration >= BUTTON_LONG_PRESS_TIME_MS) {
                event = BUTTON_EVENT_LONG_PRESS;
                xQueueSend(button_event_queue, &event, 0);
                button_pressed = false;
            }
        }
        
        last_button_state = current_state;
        vTaskDelay(pdMS_TO_TICKS(10)); // Check every 10ms
    }
}

void button_handler_get_status(void) {
    ESP_LOGI(TAG, "Button Status - Print enabled: %s", print_enabled ? "YES" : "NO");
}

bool button_handler_is_print_enabled(void) {
    return print_enabled;
}

void button_handler_toggle_print(void) {
    print_enabled = !print_enabled;
    ESP_LOGI(TAG, "Print %s", print_enabled ? "enabled" : "disabled");
}
