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
// Flag de inicialización
static bool gpio_leds_initialized = false;

// ===== SECCIÓN: FUNCIONES AUXILIARES THREAD-SAFE =====
/**
 * @brief Establece el nivel de un pin GPIO de forma thread-safe
 * 
 * Protege el acceso con mutex para evitar condiciones de carrera cuando
 * múltiples tareas intentan acceder al GPIO simultáneamente.
 * 
 * @param pin Número del pin GPIO
 * @param level Nivel a establecer (0 = bajo, 1 = alto)
 */
static void gpio_set_level_safe(gpio_num_t pin, int level) {
    if (!gpio_leds_initialized || gpio_mutex == NULL) {
        ESP_LOGW(TAG, "GPIO no inicializado, llamar gpio_init_leds() primero");
        return;
    }

    if (xSemaphoreTake(gpio_mutex, portMAX_DELAY) == pdTRUE) {
        gpio_set_level(pin, level);
        xSemaphoreGive(gpio_mutex);
    } else {
        ESP_LOGE(TAG, "Error al adquirir mutex para GPIO");
    }
}

/**
 * @brief Lee el nivel de un pin GPIO de forma thread-safe
 * 
 * Protege el acceso con mutex para evitar condiciones de carrera cuando
 * múltiples tareas intentan leer el GPIO simultáneamente.
 * 
 * @param pin Número del pin GPIO
 * @return Nivel del pin (0 = bajo, 1 = alto), o 0 si hay error
 */
static int gpio_get_level_safe(gpio_num_t pin) {
    int level = 0;
    
    if (!gpio_leds_initialized || gpio_mutex == NULL) {
        ESP_LOGW(TAG, "GPIO no inicializado, llamar gpio_init_leds() primero");
        return 0;
    }

    if (xSemaphoreTake(gpio_mutex, portMAX_DELAY) == pdTRUE) {
        level = gpio_get_level(pin);
        xSemaphoreGive(gpio_mutex);
    } else {
        ESP_LOGE(TAG, "Error al adquirir mutex para GPIO");
    }
    
    return level;
}

// ===== SECCIÓN: INICIALIZACIÓN =====
/**
 * @brief Inicializa los pines GPIO para los LEDs y crea el mutex de protección
 * 
 * Configura los pines GPIO 2 (LED amarillo) y GPIO 5 (LED azul) como
 * entrada/salida y crea un mutex para proteger el acceso concurrente desde
 * múltiples tareas.
 * 
 * Esta función debe llamarse una vez al inicio del programa antes de usar
 * cualquier otra función de este módulo.
 * 
 * Proceso de inicialización:
 * 1. Crea un mutex para proteger acceso concurrente a GPIO
 * 2. Resetea la configuración previa de los pines
 * 3. Configura los pines como entrada/salida (GPIO_MODE_INPUT_OUTPUT)
 * 4. Inicia los LEDs apagados (nivel bajo)
 * 
 * @return true si la inicialización fue exitosa, false en caso contrario
 */
bool gpio_init_leds(void) {
    if (gpio_leds_initialized) {
        ESP_LOGW(TAG, "GPIO LEDs ya están inicializados");
        return true;
    }

    // Crear mutex para proteger acceso concurrente a GPIO
    gpio_mutex = xSemaphoreCreateMutex();
    if (gpio_mutex == NULL) {
        ESP_LOGE(TAG, "Error al crear mutex para GPIO");
        return false;
    }
    
    // Resetear configuración previa de los pines
    gpio_reset_pin(LED_YELLOW_GPIO);
    gpio_reset_pin(LED_BLUE_GPIO);
    
    // Configurar pines como entrada/salida (para poder leer y escribir)
    esp_err_t err = gpio_set_direction(LED_YELLOW_GPIO, GPIO_MODE_INPUT_OUTPUT);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error configurando dirección GPIO %d: %s (0x%x)", 
                 LED_YELLOW_GPIO, esp_err_to_name(err), err);
        vSemaphoreDelete(gpio_mutex);
        gpio_mutex = NULL;
        return false;
    }

    err = gpio_set_direction(LED_BLUE_GPIO, GPIO_MODE_INPUT_OUTPUT);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error configurando dirección GPIO %d: %s (0x%x)", 
                 LED_BLUE_GPIO, esp_err_to_name(err), err);
        vSemaphoreDelete(gpio_mutex);
        gpio_mutex = NULL;
        return false;
    }
    
    // Iniciar LEDs apagados (nivel bajo)
    gpio_set_level(LED_YELLOW_GPIO, 0);
    gpio_set_level(LED_BLUE_GPIO, 0);
    
    gpio_leds_initialized = true;
    ESP_LOGI(TAG, "LEDs configurados correctamente: Amarillo (GPIO%d), Azul (GPIO%d)", 
             LED_YELLOW_GPIO, LED_BLUE_GPIO);
    ESP_LOGI(TAG, "GPIO inicializado con protección de mutex");
    
    return true;
}

// ===== SECCIÓN: CONTROL DE LED AMARILLO =====
/**
 * @brief Establece el estado del LED amarillo (encendido/apagado)
 * 
 * Función thread-safe que protege el acceso al GPIO con mutex.
 * Puede ser llamada desde cualquier tarea de forma segura.
 * 
 * @param state true para encender, false para apagar
 */
void gpio_set_yellow(bool state) {
    if (!gpio_leds_initialized) {
        ESP_LOGW(TAG, "GPIO no inicializado, llamar gpio_init_leds() primero");
        return;
    }
    gpio_set_level_safe(LED_YELLOW_GPIO, state ? 1 : 0);
}

/**
 * @brief Lee el estado actual del LED amarillo
 * 
 * Función thread-safe que protege el acceso al GPIO con mutex.
 * Puede ser llamada desde cualquier tarea de forma segura.
 * 
 * @return true si está encendido, false si está apagado o si el driver no está inicializado
 */
bool gpio_get_yellow(void) {
    if (!gpio_leds_initialized) {
        ESP_LOGW(TAG, "GPIO no inicializado, llamar gpio_init_leds() primero");
        return false;
    }
    return (gpio_get_level_safe(LED_YELLOW_GPIO) != 0);
}

// ===== SECCIÓN: CONTROL DE LED AZUL =====
/**
 * @brief Establece el estado del LED azul (encendido/apagado)
 * 
 * Función thread-safe que protege el acceso al GPIO con mutex.
 * Puede ser llamada desde cualquier tarea de forma segura.
 * 
 * @param state true para encender, false para apagar
 */
void gpio_set_blue(bool state) {
    if (!gpio_leds_initialized) {
        ESP_LOGW(TAG, "GPIO no inicializado, llamar gpio_init_leds() primero");
        return;
    }
    gpio_set_level_safe(LED_BLUE_GPIO, state ? 1 : 0);
}

/**
 * @brief Lee el estado actual del LED azul
 * 
 * Función thread-safe que protege el acceso al GPIO con mutex.
 * Puede ser llamada desde cualquier tarea de forma segura.
 * 
 * @return true si está encendido, false si está apagado o si el driver no está inicializado
 */
bool gpio_get_blue(void) {
    if (!gpio_leds_initialized) {
        ESP_LOGW(TAG, "GPIO no inicializado, llamar gpio_init_leds() primero");
        return false;
    }
    return (gpio_get_level_safe(LED_BLUE_GPIO) != 0);
}