/**
 * ============================================================================
 * ARCHIVO: rgb_led.c
 * ============================================================================
 * 
 * RESUMEN:
 * Implementación del controlador de LED RGB (actualmente solo canal verde).
 * Este módulo gestiona el control PWM del LED RGB mediante el periférico LEDC
 * del ESP32.
 * 
 * Hardware:
 * - LED Verde: GPIO 27 (PWM mediante LEDC Channel 1, Timer 0)
 * 
 * Características:
 * - Control PWM de 8 bits (0-255 niveles)
 * - Frecuencia: 5kHz (adecuada para LEDs sin parpadeo visible)
 * - Interfaz simple con porcentaje (0-100%)
 * - Validación de parámetros y manejo de errores
 * 
 * ============================================================================
 * ÍNDICE DE SECCIONES:
 * ============================================================================
 * Sección 1: INCLUDES se encuentra en las líneas 33 a 40
 * Sección 2: DEFINICIONES Y CONSTANTES se encuentra en las líneas 42 a 50
 * Sección 3: VARIABLES ESTÁTICAS se encuentra en las líneas 52 a 53
 * Sección 4: FUNCIONES DE INICIALIZACIÓN se encuentra en las líneas 55 a 104
 * Sección 5: FUNCIONES DE CONTROL DEL LED se encuentra en las líneas 106 a 145
 * ============================================================================
 * 
 * ============================================================================
 * RESUMEN DE TAREAS, COLAS Y SEMÁFOROS IMPLEMENTADOS:
 * ============================================================================
 * 
 * === TAREAS (TASKS) ===
 * 
 * Ninguna en este módulo. Las funciones se llaman directamente desde otras tareas.
 * 
 * === COLAS (QUEUES) ===
 * 
 * Ninguna en este módulo.
 * 
 * === SEMÁFOROS (MUTEXES) ===
 * 
 * Ninguno en este módulo. El acceso a las funciones es thread-safe porque
 * las funciones de LEDC son thread-safe.
 * 
 * ============================================================================
 */

// ===== INCLUDES =====
#include "rgb_led.h"

// ESP-IDF
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"

// ===== DEFINICIONES Y CONSTANTES =====
static const char *TAG = "RGB_LED";

// Configuración de hardware
#define GPIO_GREEN      27              // GPIO para LED verde
#define LEDC_TIMER      LEDC_TIMER_0    // Timer LEDC a usar
#define LEDC_MODE       LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL_G  LEDC_CHANNEL_1  // Canal LEDC para LED verde
#define LEDC_DUTY_RES   LEDC_TIMER_8_BIT  // Resolución de 8 bits (0-255)
#define LEDC_FREQUENCY  5000            // Frecuencia PWM en Hz

// ===== VARIABLES ESTÁTICAS =====
static bool rgb_led_initialized = false;  // Flag de inicialización

// ===== FUNCIONES DE INICIALIZACIÓN =====
/**
 * @brief Inicializa el controlador PWM para el LED RGB verde
 * 
 * Configura el timer LEDC y el canal PWM para controlar el LED verde.
 * Esta función debe llamarse una vez durante la inicialización del sistema
 * antes de usar cualquier otra función de este módulo.
 * 
 * Proceso de inicialización:
 * 1. Configura el timer LEDC con resolución de 8 bits y frecuencia de 5kHz
 * 2. Configura el canal LEDC para el GPIO del LED verde
 * 3. Inicia el LED con brillo 0% (apagado)
 * 
 * @return true si la inicialización fue exitosa, false en caso contrario
 */
bool rgb_led_init(void)
{
    if (rgb_led_initialized) {
        ESP_LOGW(TAG, "RGB LED ya está inicializado");
        return true;
    }

    ESP_LOGI(TAG, "Inicializando PWM para LED verde en GPIO %d...", GPIO_GREEN);
    
    // Configurar timer LEDC
    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LEDC_DUTY_RES,
        .freq_hz = LEDC_FREQUENCY,
        .speed_mode = LEDC_MODE,
        .timer_num = LEDC_TIMER,
        .clk_cfg = LEDC_AUTO_CLK
    };
    esp_err_t err = ledc_timer_config(&ledc_timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error configurando timer PWM: %s (0x%x)", esp_err_to_name(err), err);
        return false;
    }

    // Configurar canal LEDC para LED verde
    ledc_channel_config_t ledc_channel = {
        .channel    = LEDC_CHANNEL_G,
        .duty       = 0,  // Iniciar apagado
        .gpio_num   = GPIO_GREEN,
        .speed_mode = LEDC_MODE,
        .hpoint     = 0,
        .timer_sel  = LEDC_TIMER,
        .intr_type  = LEDC_INTR_DISABLE
    };
    err = ledc_channel_config(&ledc_channel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error configurando canal PWM: %s (0x%x)", esp_err_to_name(err), err);
        return false;
    }

    rgb_led_initialized = true;
    ESP_LOGI(TAG, "LED verde inicializado correctamente en GPIO %d (8-bit, %dkHz)", 
             GPIO_GREEN, LEDC_FREQUENCY / 1000);
    
    return true;
}

// ===== FUNCIONES DE CONTROL DEL LED =====
/**
 * @brief Establece el brillo del LED verde como porcentaje
 * 
 * Convierte el porcentaje (0-100) a valor PWM (0-255) y actualiza el LED.
 * Valores fuera de rango se limitan automáticamente a 0-100.
 * 
 * Nota: Esta función requiere que rgb_led_init() haya sido llamada previamente.
 * Si no está inicializado, la función no realizará ninguna acción.
 * 
 * @param percent Porcentaje de brillo (0-100). Valores >100 se limitan a 100.
 */
void rgb_set_green_percent(uint8_t percent)
{
    if (!rgb_led_initialized) {
        ESP_LOGW(TAG, "RGB LED no inicializado. Llamar rgb_led_init() primero.");
        return;
    }

    // Limitar porcentaje al rango válido
    if (percent > 100) {
        percent = 100;
    }
    
    // Convertir porcentaje a valor PWM (0-255)
    uint32_t duty = ((uint32_t)percent * 255) / 100;
    
    // Actualizar PWM del LED
    esp_err_t err = ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_G, duty);
    if (err == ESP_OK) {
        err = ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_G);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error actualizando PWM: %s (0x%x)", esp_err_to_name(err), err);
        return;
    }
    
    ESP_LOGD(TAG, "LED verde establecido a %d%% (duty: %lu)", percent, duty);
}
