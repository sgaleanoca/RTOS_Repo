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
 * 
 * ============================================================================
 * ÍNDICE DE SECCIONES:
 * ============================================================================
 * Sección 1: INCLUDES se encuentra en las líneas 22 a 32
 * Sección 2: DEFINICIONES Y CONSTANTES se encuentra en las líneas 34 a 39
 * Sección 3: VARIABLES GLOBALES se encuentra en las líneas 41 a 43
 * Sección 4: FUNCIONES AUXILIARES THREAD-SAFE se encuentra en las líneas 45 a 81
 * Sección 5: INICIALIZACIÓN se encuentra en las líneas 82 a 108
 * Sección 6: CONTROL DE LED AMARILLO se encuentra en las líneas 109 a 128
 * Sección 7: CONTROL DE LED AZUL se encuentra en las líneas 130 a 149
 * ============================================================================
 */

// ===== INCLUDES =====
// Header local
#include "gpio_driver.h"

// ESP-IDF
#include "driver/gpio.h"
#include "esp_log.h"

// FreeRTOS
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// ===== DEFINICIONES Y CONSTANTES =====
static const char *TAG = "GPIO_DRIVER";

// Configuración de pines GPIO
#define LED_YELLOW_GPIO 2  // GPIO 2 - LED Amarillo
#define LED_BLUE_GPIO   5  // GPIO 5 - LED Azul

// ===== VARIABLES GLOBALES =====
// Semáforo mutex para proteger acceso concurrente a GPIO desde múltiples tareas
static SemaphoreHandle_t gpio_mutex = NULL;

// ===== SECCIÓN: FUNCIONES AUXILIARES THREAD-SAFE =====
/**
 * Establece el nivel de un pin GPIO de forma thread-safe
 * Protege el acceso con mutex para evitar condiciones de carrera
 * 
 * @param pin: Número del pin GPIO
 * @param level: Nivel a establecer (0 = bajo, 1 = alto)
 */
static void gpio_set_level_safe(gpio_num_t pin, int level) {
    if (gpio_mutex != NULL && xSemaphoreTake(gpio_mutex, portMAX_DELAY) == pdTRUE) {
        gpio_set_level(pin, level);
        xSemaphoreGive(gpio_mutex);
    } else {
        // Fallback si el mutex no está disponible (no debería pasar en operación normal)
        gpio_set_level(pin, level);
    }
}

/**
 * Lee el nivel de un pin GPIO de forma thread-safe
 * Protege el acceso con mutex para evitar condiciones de carrera
 * 
 * @param pin: Número del pin GPIO
 * @return Nivel del pin (0 = bajo, 1 = alto)
 */
static int gpio_get_level_safe(gpio_num_t pin) {
    int level = 0;
    if (gpio_mutex != NULL && xSemaphoreTake(gpio_mutex, portMAX_DELAY) == pdTRUE) {
        level = gpio_get_level(pin);
        xSemaphoreGive(gpio_mutex);
    } else {
        // Fallback si el mutex no está disponible (no debería pasar en operación normal)
        level = gpio_get_level(pin);
    }
    return level;
}

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
    gpio_reset_pin(LED_YELLOW_GPIO);
    gpio_reset_pin(LED_BLUE_GPIO);
    
    // Configurar pines como entrada/salida (para poder leer y escribir)
    gpio_set_direction(LED_YELLOW_GPIO, GPIO_MODE_INPUT_OUTPUT);
    gpio_set_direction(LED_BLUE_GPIO, GPIO_MODE_INPUT_OUTPUT);
    
    ESP_LOGI(TAG, "LEDs configurados: Amarillo (GPIO%d), Azul (GPIO%d)", 
             LED_YELLOW_GPIO, LED_BLUE_GPIO);
    
    ESP_LOGI(TAG, "GPIO inicializado con protección de mutex");
}

// ===== SECCIÓN: CONTROL DE LED AMARILLO =====
/**
 * Establece el estado del LED amarillo (encendido/apagado)
 * Función thread-safe que protege el acceso al GPIO con mutex
 * 
 * @param state: true para encender, false para apagar
 */
void gpio_set_yellow(bool state) {
    gpio_set_level_safe(LED_YELLOW_GPIO, state ? 1 : 0);
}

/**
 * Lee el estado actual del LED amarillo
 * Función thread-safe que protege el acceso al GPIO con mutex
 * 
 * @return true si está encendido, false si está apagado
 */
bool gpio_get_yellow(void) {
    return (gpio_get_level_safe(LED_YELLOW_GPIO) != 0);
}

// ===== SECCIÓN: CONTROL DE LED AZUL =====
/**
 * Establece el estado del LED azul (encendido/apagado)
 * Función thread-safe que protege el acceso al GPIO con mutex
 * 
 * @param state: true para encender, false para apagar
 */
void gpio_set_blue(bool state) {
    gpio_set_level_safe(LED_BLUE_GPIO, state ? 1 : 0);
}

/**
 * Lee el estado actual del LED azul
 * Función thread-safe que protege el acceso al GPIO con mutex
 * 
 * @return true si está encendido, false si está apagado
 */
bool gpio_get_blue(void) {
    return (gpio_get_level_safe(LED_BLUE_GPIO) != 0);
}