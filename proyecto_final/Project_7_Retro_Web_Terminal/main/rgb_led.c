/**
 * @file rgb_led.c
 * @brief Implementación del controlador de LED RGB
 * @author Proyecto Final RTOS
 * @date 2025
 * 
 * @details Implementación del controlador de LED RGB (actualmente solo canal verde).
 * Este módulo gestiona el control PWM del LED RGB mediante el periférico LEDC
 * del ESP32.
 * 
 * @section hardware Hardware
 * - LED Verde: GPIO 27 (PWM mediante LEDC Channel 1, Timer 0)
 * 
 * @section features Características
 * - Control PWM de 8 bits (0-255 niveles)
 * - Frecuencia: 5kHz (adecuada para LEDs sin parpadeo visible)
 * - Interfaz simple con porcentaje (0-100%)
 * - Validación de parámetros y manejo de errores
 * - Control automático basado en sensor PIR (LED al 100% con movimiento, apagado sin movimiento)
 * 
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
 * - Control automático basado en sensor PIR (LED al 100% con movimiento, apagado sin movimiento)
 * 
 * ============================================================================
 * ÍNDICE DE SECCIONES:
 * ============================================================================
 * Sección 1: INCLUDES se encuentra en las líneas 33 a 40
 * Sección 2: DEFINICIONES Y CONSTANTES se encuentra en las líneas 42 a 50
 * Sección 3: VARIABLES ESTÁTICAS se encuentra en las líneas 52 a 53
 * Sección 4: FUNCIONES DE INICIALIZACIÓN se encuentra en las líneas 55 a 104
 * Sección 5: FUNCIONES DE CONTROL DEL LED se encuentra en las líneas 106 a 145
 * Sección 6: FUNCIONES DE CONTROL POR PIR se encuentra en las líneas 147 a 220
 * ============================================================================
 * 
 * ============================================================================
 * RESUMEN DE TAREAS, COLAS Y SEMÁFOROS IMPLEMENTADOS:
 * ============================================================================
 * 
 * === TAREAS (TASKS) ===
 * 
 * 1. rgb_led_pir_task (creada por rgb_led_start_pir_control())
 *    - Nombre: "rgb_led_pir"
 *    - Stack: 4096 bytes
 *    - Prioridad: 5
 *    - Función: Monitorea el sensor PIR y controla el LED automáticamente
 *    - Comportamiento: LED al 100% con movimiento, LED apagado sin movimiento
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

// FreeRTOS
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Headers locales
#include "pir_driver.h"

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
static bool pir_led_task_running = false; // Flag para la tarea de control por PIR

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

// ===== FUNCIONES DE CONTROL POR PIR =====
/**
 * @brief Tarea de FreeRTOS que monitorea el sensor PIR y controla el LED
 * 
 * Esta tarea lee periódicamente el estado del sensor PIR y controla el LED:
 * - Si hay movimiento detectado: LED al 100% de brillo
 * - Si no hay movimiento: LED apagado (0%)
 * 
 * La tarea se ejecuta cada 200ms para tener una respuesta rápida a los cambios.
 * 
 * @param pvParameters Parámetros de la tarea (no usado)
 */
static void rgb_led_pir_task(void *pvParameters)
{
    bool last_motion_state = false;
    
    ESP_LOGI(TAG, "Tarea de control LED por PIR iniciada");
    
    while (1) {
        // Leer estado actual del sensor PIR
        bool motion_detected = pir_is_motion_active();
        
        // Solo actualizar el LED si el estado cambió (evitar actualizaciones innecesarias)
        if (motion_detected != last_motion_state) {
            if (motion_detected) {
                // Movimiento detectado: encender LED al 100%
                rgb_set_green_percent(100);
                ESP_LOGI(TAG, "✓ Movimiento detectado - LED encendido al 100%%");
            } else {
                // No hay movimiento: apagar LED
                rgb_set_green_percent(0);
                ESP_LOGI(TAG, "✗ Sin movimiento - LED apagado");
            }
            last_motion_state = motion_detected;
        }
        
        // Esperar 200ms antes de la siguiente lectura
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

/**
 * @brief Inicia la tarea de control automático del LED basado en el sensor PIR
 * 
 * Crea una tarea de FreeRTOS que monitorea continuamente el sensor PIR y controla
 * el LED automáticamente:
 * - LED al 100% cuando se detecta movimiento
 * - LED apagado cuando no hay movimiento
 * 
 * Requisitos:
 * - rgb_led_init() debe haber sido llamado
 * - pir_init() debe haber sido llamado
 * 
 * La tarea se ejecuta de forma independiente y monitorea el PIR continuamente.
 * 
 * @note Esta función solo puede ser llamada una vez. Si se llama múltiples veces,
 *       solo creará la tarea la primera vez.
 */
void rgb_led_start_pir_control(void)
{
    if (pir_led_task_running) {
        ESP_LOGW(TAG, "Tarea de control LED por PIR ya está ejecutándose");
        return;
    }
    
    if (!rgb_led_initialized) {
        ESP_LOGE(TAG, "LED RGB no inicializado. Llamar rgb_led_init() primero.");
        return;
    }
    
    ESP_LOGI(TAG, "Iniciando tarea de control LED por PIR...");
    
    // Crear tarea de FreeRTOS
    BaseType_t result = xTaskCreate(
        rgb_led_pir_task,        // Función de la tarea
        "rgb_led_pir",           // Nombre de la tarea
        4096,                    // Stack size (4KB)
        NULL,                    // Parámetros (no usado)
        5,                       // Prioridad (media)
        NULL                     // Handle de la tarea (no necesario)
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Error al crear tarea de control LED por PIR");
        return;
    }
    
    pir_led_task_running = true;
    ESP_LOGI(TAG, "Tarea de control LED por PIR creada correctamente");
}
