#ifndef BUTTON_CONTROL_H
#define BUTTON_CONTROL_H

#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

// --- Configuración del Botón ---
#define BUTTON_PIN             14              // Botón en GPIO14
#define BUTTON_DEBOUNCE_TIME   50              // Tiempo de debounce en ms
#define BUTTON_LONG_PRESS_TIME 1000            // Tiempo para pulsación larga en ms

// --- Estados del Botón ---
typedef enum {
    BUTTON_EVENT_NONE = 0,
    BUTTON_EVENT_PRESSED,
    BUTTON_EVENT_RELEASED,
    BUTTON_EVENT_LONG_PRESS
} button_event_t;

// --- Estructura de datos del botón ---
typedef struct {
    bool is_pressed;
    bool print_enabled;
    // IMPLEMENTACIÓN PARCIAL_1 BOTON ALTERNADO DEL LED
    bool led_forced_off;
    uint32_t press_start_time;
    uint32_t last_event_time;
} button_state_t;

// --- Cola de eventos del botón ---
// La cola queda encapsulada dentro del módulo

// Contexto opaco del botón
typedef struct button_ctx button_ctx_t;

button_ctx_t* button_control_create(void);
void button_control_destroy(button_ctx_t* ctx);
void button_task(void *arg); // arg = button_ctx_t*
bool is_print_enabled(button_ctx_t* ctx);
void set_print_enabled(button_ctx_t* ctx, bool enabled);
button_state_t get_button_state(button_ctx_t* ctx);
bool is_led_forced_off(button_ctx_t* ctx);

#endif // BUTTON_CONTROL_H
