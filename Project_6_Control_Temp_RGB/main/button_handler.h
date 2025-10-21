#ifndef BUTTON_HANDLER_H
#define BUTTON_HANDLER_H

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// Button GPIO pin
#define BUTTON_GPIO GPIO_NUM_0  // Boot button

// Button states
typedef enum {
    BUTTON_RELEASED,
    BUTTON_PRESSED,
    BUTTON_LONG_PRESS
} button_state_t;

// Button events
typedef enum {
    BUTTON_EVENT_PRESS,
    BUTTON_EVENT_RELEASE,
    BUTTON_EVENT_LONG_PRESS
} button_event_t;

// Button configuration
#define BUTTON_DEBOUNCE_TIME_MS    50
#define BUTTON_LONG_PRESS_TIME_MS  2000

// Function prototypes
void button_handler_init(void);
void button_handler_task(void *pvParameters);
void button_handler_get_status(void);
bool button_handler_is_print_enabled(void);
void button_handler_toggle_print(void);

// External variables
extern bool print_enabled;
extern QueueHandle_t button_event_queue;

#endif // BUTTON_HANDLER_H
