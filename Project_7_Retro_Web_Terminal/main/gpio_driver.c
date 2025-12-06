/**
 * ============================================================================
 * ARCHIVO: gpio_driver.c
 * ============================================================================
 * 
 * RESUMEN:
 * Este módulo gestiona el control de los LEDs conectados al ESP32 mediante GPIO.
 * Implementa funciones para encender/apagar LEDs amarillo y azul de forma segura
 * usando mutex para proteger el acceso concurrente desde múltiples tareas.
 * 
 * Hardware:
 * - LED Amarillo: GPIO 2
 * - LED Azul: GPIO 5
 * 
 * Características:
 * - Protección thread-safe con mutex
 * - Funciones para leer y escribir estado de LEDs
 * - Inicialización segura de pines GPIO
 * ============================================================================
 */

// ===== INCLUDES =====
#include "gpio_driver.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"

// ===== DEFINICIONES Y VARIABLES GLOBALES =====
static const char *TAG = "GPIO_DRIVER";

// Pines GPIO asignados a los LEDs
#define LED_YELLOW 2  // GPIO 2 - LED Amarillo
#define LED_BLUE   5  // GPIO 5 - LED Azul

// Semáforo mutex para proteger acceso concurrente a GPIO desde múltiples tareas
static SemaphoreHandle_t gpio_mutex = NULL;

// ===== SECCIÓN: INICIALIZACIÓN =====
/**
 * Inicializa los pines GPIO para los LEDs y crea el mutex de protección
 * Esta función debe llamarse una vez al inicio del programa
 */
void gpio_init_leds(void) {
    // Crear mutex para proteger acceso concurrente a GPIO
    gpio_mutex = xSemaphoreCreateMutex();
    if (gpio_mutex == NULL) {
        ESP_LOGE(TAG, "Error al crear mutex para GPIO");
        return;
    }
    
    // Resetear configuración previa de los pines
    gpio_reset_pin(LED_YELLOW);
    gpio_reset_pin(LED_BLUE);
    
    // Configurar pines como entrada/salida (para poder leer y escribir)
    gpio_set_direction(LED_YELLOW, GPIO_MODE_INPUT_OUTPUT);
    gpio_set_direction(LED_BLUE, GPIO_MODE_INPUT_OUTPUT);
    
    ESP_LOGI(TAG, "GPIO inicializado con protección de mutex");
}

// ===== SECCIÓN: CONTROL DE LED AMARILLO =====
/**
 * Establece el estado del LED amarillo (encendido/apagado)
 * @param state: true para encender, false para apagar
 */
void gpio_set_yellow(bool state) {
    // Proteger acceso con mutex para evitar condiciones de carrera
    if (gpio_mutex != NULL && xSemaphoreTake(gpio_mutex, portMAX_DELAY) == pdTRUE) {
        gpio_set_level(LED_YELLOW, state);
        xSemaphoreGive(gpio_mutex);
    } else {
        // Fallback si el mutex no está disponible (no debería pasar)
        gpio_set_level(LED_YELLOW, state);
    }
}

/**
 * Lee el estado actual del LED amarillo
 * @return true si está encendido, false si está apagado
 */
bool gpio_get_yellow(void) {
    bool state = false;
    if (gpio_mutex != NULL && xSemaphoreTake(gpio_mutex, portMAX_DELAY) == pdTRUE) {
        state = gpio_get_level(LED_YELLOW);
        xSemaphoreGive(gpio_mutex);
    } else {
        // Fallback si el mutex no está disponible
        state = gpio_get_level(LED_YELLOW);
    }
    return state;
}

// ===== SECCIÓN: CONTROL DE LED AZUL =====
/**
 * Establece el estado del LED azul (encendido/apagado)
 * @param state: true para encender, false para apagar
 */
void gpio_set_blue(bool state) {
    // Proteger acceso con mutex para evitar condiciones de carrera
    if (gpio_mutex != NULL && xSemaphoreTake(gpio_mutex, portMAX_DELAY) == pdTRUE) {
        gpio_set_level(LED_BLUE, state);
        xSemaphoreGive(gpio_mutex);
    } else {
        // Fallback si el mutex no está disponible (no debería pasar)
        gpio_set_level(LED_BLUE, state);
    }
}

/**
 * Lee el estado actual del LED azul
 * @return true si está encendido, false si está apagado
 */
bool gpio_get_blue(void) {
    bool state = false;
    if (gpio_mutex != NULL && xSemaphoreTake(gpio_mutex, portMAX_DELAY) == pdTRUE) {
        state = gpio_get_level(LED_BLUE);
        xSemaphoreGive(gpio_mutex);
    } else {
        // Fallback si el mutex no está disponible
        state = gpio_get_level(LED_BLUE);
    }
    return state;
}