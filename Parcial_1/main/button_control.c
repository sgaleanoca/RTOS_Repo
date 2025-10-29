// ===== INCLUDES Y CONFIGURACIÓN =====
#include "button_control.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_timer.h"
#include <stdlib.h>

static const char *TAG = "BUTTON_CONTROL";

typedef struct button_ctx {
    button_state_t state;
    QueueHandle_t queue;
} button_ctx_t;

// ===== FUNCIONES DE INICIALIZACIÓN =====
// Configura el GPIO del botón, estado inicial y crea la cola de eventos
button_ctx_t* button_control_create(void) {
    button_ctx_t *ctx = (button_ctx_t*)calloc(1, sizeof(button_ctx_t));
    if (!ctx) return NULL;
    ESP_LOGI(TAG, "Inicializando control de botón en GPIO%d...", BUTTON_PIN);
    
    // Configuración del GPIO del botón
    gpio_config_t button_config = {
        .pin_bit_mask = (1ULL << BUTTON_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&button_config));
    
    // Inicializar estado del botón
    ctx->state.is_pressed = false;
    ctx->state.print_enabled = true;  // La impresión deja de togglearse con el botón
    ctx->state.led_forced_off = false; // LED no forzado apagado por defecto
    ctx->state.press_start_time = 0;
    ctx->state.last_event_time = 0;
    
    
    // Crear cola de eventos del botón
    ctx->queue = xQueueCreate(5, sizeof(button_event_t));
    if (ctx->queue == NULL) {
        ESP_LOGE(TAG, "Error creando cola de eventos del botón");
        free(ctx);
        return NULL;
    }
    
    ESP_LOGI(TAG, "Control de botón inicializado correctamente");
    ESP_LOGI(TAG, "Estado inicial: Impresión %s", ctx->state.print_enabled ? "HABILITADA" : "DESHABILITADA");
    return ctx;
}

void button_control_destroy(button_ctx_t* ctx) {
    if (!ctx) return;
    if (ctx->queue) vQueueDelete(ctx->queue);
    free(ctx);
}

// ===== FUNCIONES DE LECTURA DEL BOTÓN =====
// Lee el estado físico del botón (activo en bajo)
static bool read_button_state(void) { return !gpio_get_level(BUTTON_PIN); }
// Devuelve tiempo actual en ms a partir del temporizador de ESP-IDF
static uint32_t get_current_time_ms(void) { return (uint32_t)(esp_timer_get_time() / 1000); }
// Verifica si pasó el tiempo de debounce para aceptar un nuevo evento
static bool is_debounce_ready(button_ctx_t* ctx) { return (get_current_time_ms() - ctx->state.last_event_time) >= BUTTON_DEBOUNCE_TIME; }
// Determina si la pulsación actual ya es considerada larga
static bool is_long_press(button_ctx_t* ctx) { return ctx->state.is_pressed && (get_current_time_ms() - ctx->state.press_start_time) >= BUTTON_LONG_PRESS_TIME; }

// ===== FUNCIONES DE CONTROL =====
// Tarea: detecta pulsaciones cortas/largas y alterna impresión en pulsación corta
void button_task(void *arg) {
    button_ctx_t *ctx = (button_ctx_t*)arg;
    ESP_LOGI(TAG, "Tarea de control de botón iniciada");
    
    bool last_button_state = false;
    bool long_press_detected = false;
    
    while (1) {
        bool current_button_state = read_button_state();
        uint32_t current_time = get_current_time_ms();
        
        // Detectar flanco de bajada (botón presionado)
        if (current_button_state && !last_button_state && is_debounce_ready(ctx)) {
            ctx->state.is_pressed = true;
            ctx->state.press_start_time = current_time;
            ctx->state.last_event_time = current_time;
            long_press_detected = false;
            
            ESP_LOGI(TAG, "Botón presionado");
        }
        
        // Detectar flanco de subida (botón liberado)
        else if (!current_button_state && last_button_state && is_debounce_ready(ctx)) {
            ctx->state.is_pressed = false;
            ctx->state.last_event_time = current_time;
            
            if (!long_press_detected) {
                // IMPLEMENTACIÓN PARCIAL_1 BOTON ALTERNADO DEL LED
                // Pulsación corta: alterna forzado de LED apagado (una sola vez por pulsación)
                ctx->state.led_forced_off = !ctx->state.led_forced_off;
                ESP_LOGI(TAG, "Pulsación corta - LED %s", 
                         ctx->state.led_forced_off ? "FORZADO APAGADO" : "CONTROL NORMAL");

                // Enviar evento a la cola (se mantiene por compatibilidad)
                button_event_t event = BUTTON_EVENT_PRESSED;
                xQueueSend(ctx->queue, &event, pdMS_TO_TICKS(10));
            }
            
            ESP_LOGI(TAG, "Botón liberado");
        }
        
        // Detectar pulsación larga
        else if (ctx->state.is_pressed && is_long_press(ctx) && !long_press_detected) {
            long_press_detected = true;
            ESP_LOGI(TAG, "Pulsación larga detectada");
            
            // Enviar evento de pulsación larga
            button_event_t event = BUTTON_EVENT_LONG_PRESS;
            xQueueSend(ctx->queue, &event, pdMS_TO_TICKS(10));
        }
        
        last_button_state = current_button_state;
        vTaskDelay(pdMS_TO_TICKS(10));  // Polling cada 10ms
    }
}

// ===== FUNCIONES PÚBLICAS =====
bool is_print_enabled(button_ctx_t* ctx) { return ctx ? ctx->state.print_enabled : false; }
void set_print_enabled(button_ctx_t* ctx, bool enabled) { 
    if (!ctx) return;
    ctx->state.print_enabled = enabled;
    ESP_LOGI(TAG, "Impresión %s manualmente", enabled ? "HABILITADA" : "DESHABILITADA");
}
button_state_t get_button_state(button_ctx_t* ctx) { return ctx ? ctx->state : (button_state_t){0}; }
bool is_led_forced_off(button_ctx_t* ctx) { return ctx ? ctx->state.led_forced_off : false; }
