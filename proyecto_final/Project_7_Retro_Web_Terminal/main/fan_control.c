/**
 * @file fan_control.c
 * @brief Implementación del controlador de ventilador mediante PWM
 * @author Proyecto Final RTOS
 * @date 2024
 * 
 * @details Implementación del módulo de control del ventilador mediante PWM.
 * Este módulo gestiona el control de velocidad del ventilador en diferentes
 * modos de operación: OFF, MANUAL, AUTO_TEMP (automático por temperatura) y
 * SCHEDULE (control por horarios basado en registros).
 * 
 * @section hardware Hardware
 * - Ventilador: Controlado mediante PWM en GPIO 26 (LEDC Channel 2, Timer 1)
 * - Sensor PIR: El ventilador solo funciona si detecta presencia (excepto modo MANUAL)
 * - Sensor NTC: Utilizado para control automático por temperatura
 * 
 * @section features Características
 * - Control PWM de 8 bits (0-255 niveles)
 * - Frecuencia: 25kHz (adecuada para motores)
 * - Múltiples modos de operación con tareas independientes
 * - Integración con sistema de registros para control por horarios
 * - Verificación de presencia mediante sensor PIR (excepto modo MANUAL)
 * 
 * @section modes Modos de operación
 * - FAN_MODE_OFF: Ventilador apagado (0% PWM)
 * - FAN_MODE_MANUAL: Control manual por porcentaje (0-100%), ignora PIR
 * - FAN_MODE_AUTO_TEMP: Control automático basado en temperatura (15-25°C)
 * - FAN_MODE_SCHEDULE: Control por horarios usando registros guardados
 * 
 * ============================================================================
 * ARCHIVO: fan_control.c
 * ============================================================================
 * 
 * RESUMEN:
 * Implementación del módulo de control del ventilador mediante PWM.
 * Este módulo gestiona el control de velocidad del ventilador en diferentes
 * modos de operación: OFF, MANUAL, AUTO_TEMP (automático por temperatura) y
 * SCHEDULE (control por horarios basado en registros).
 * 
 * Hardware:
 * - Ventilador: Controlado mediante PWM en GPIO 26 (LEDC Channel 2, Timer 1)
 * - Sensor PIR: El ventilador solo funciona si detecta presencia (excepto modo MANUAL)
 * - Sensor NTC: Utilizado para control automático por temperatura
 * 
 * Características:
 * - Control PWM de 8 bits (0-255 niveles)
 * - Frecuencia: 25kHz (adecuada para motores)
 * - Múltiples modos de operación con tareas independientes
 * - Integración con sistema de registros para control por horarios
 * - Verificación de presencia mediante sensor PIR (excepto modo MANUAL)
 * 
 * Modos de operación:
 * - FAN_MODE_OFF: Ventilador apagado (0% PWM)
 * - FAN_MODE_MANUAL: Control manual por porcentaje (0-100%), ignora PIR
 * - FAN_MODE_AUTO_TEMP: Control automático basado en temperatura (15-25°C)
 * - FAN_MODE_SCHEDULE: Control por horarios usando registros guardados
 * 
 * ============================================================================
 * ÍNDICE DE SECCIONES:
 * ============================================================================
 * Sección 1: INCLUDES se encuentra en las líneas 48 a 60
 * Sección 2: DEFINICIONES Y CONSTANTES se encuentra en las líneas 62 a 75
 * Sección 3: VARIABLES GLOBALES Y ESTADO se encuentra en las líneas 77 a 85
 * Sección 4: FUNCIONES INTERNAS DE CONTROL se encuentra en las líneas 87 a 95
 * Sección 5: FUNCIONES PÚBLICAS DE CONTROL se encuentra en las líneas 97 a 269
 * Sección 6: TAREA DE CONTROL AUTOMÁTICO POR TEMPERATURA se encuentra en las líneas 271 a 330
 * Sección 7: TAREA DE CONTROL POR HORARIOS (REGISTROS) se encuentra en las líneas 332 a 387
 * ============================================================================
 * 
 * ============================================================================
 * RESUMEN DE TAREAS, COLAS Y SEMÁFOROS IMPLEMENTADOS:
 * ============================================================================
 * 
 * === TAREAS (TASKS) ===
 * 
 * 1. fan_auto_temp_task (Sección 6)
 *    - Nombre: "fan_auto_temp"
 *    - Stack: 4096 bytes
 *    - Prioridad: 5 (alta)
 *    - Función: Lee la temperatura periódicamente y actualiza el ventilador
 *              cuando está en modo FAN_MODE_AUTO_TEMP
 *    - Propósito: Control automático del ventilador basado en temperatura
 *    - Estado: Loop infinito, actualiza cada 1 segundo
 *    - Flujo:
 *      1. Verifica que el modo sea AUTO_TEMP
 *      2. Lee temperatura del sensor NTC
 *      3. Calcula porcentaje de velocidad basado en temperatura
 *      4. Actualiza PWM del ventilador
 * 
 * 2. fan_schedule_task (Sección 7)
 *    - Nombre: "fan_schedule"
 *    - Stack: 4096 bytes
 *    - Prioridad: 5 (alta)
 *    - Función: Verifica periódicamente los registros y actualiza el ventilador
 *              cuando está en modo FAN_MODE_SCHEDULE
 *    - Propósito: Control del ventilador basado en horarios guardados
 *    - Estado: Loop infinito, verifica cada 10 segundos
 *    - Flujo:
 *      1. Verifica que el modo sea SCHEDULE
 *      2. Obtiene día y hora actual
 *      3. Busca registro activo en registros.json
 *      4. Actualiza velocidad del ventilador según registro encontrado
 * 
 * === COLAS (QUEUES) ===
 * 
 * Ninguna en este módulo. Las colas se gestionan en otros módulos.
 * 
 * === SEMÁFOROS (MUTEXES) ===
 * 
 * Ninguno en este módulo. El acceso a variables globales es thread-safe
 * porque las funciones de control se llaman desde tareas específicas.
 * 
 * ============================================================================
 */

// ===== INCLUDES =====
// Headers locales
#include "fan_control.h"
#include "ntc_sensor.h"
#include "registros.h"
#include "time_sync.h"
#include "pir_driver.h"

// ESP-IDF
#include "esp_log.h"
#include "driver/ledc.h"

// FreeRTOS
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Estándar C
#include <math.h>
#include <string.h>

// ===== DEFINICIONES Y CONSTANTES =====
static const char *TAG = "FAN_CONTROL";

// ===== VARIABLES GLOBALES Y ESTADO =====
static fan_mode_t current_mode = FAN_MODE_OFF;
static uint8_t manual_percent = 0;
static uint8_t current_pwm_percent = 0;
static fan_schedule_t fan_schedule = {
    .start_hour = 10, // Default 10:00
    .end_hour = 16,   // Default 16:00
    .min_temp = FAN_TEMP_MIN_C
};

// ===== FUNCIONES INTERNAS DE CONTROL =====

/**
 * @brief Establece el ciclo de trabajo PWM del ventilador.
 * @param percent Porcentaje de 0 a 100.
 * @note En modo MANUAL, el ventilador funciona independientemente del PIR.
 *       En otros modos, el ventilador solo se activará si el sensor PIR detecta presencia.
 *       Si no hay presencia, el ventilador se apagará automáticamente.
 */
static void set_fan_pwm(uint8_t percent) {
    if (percent > 100) percent = 100;
    
    // En modo MANUAL, ignorar el PIR y aplicar directamente el porcentaje
    if (current_mode == FAN_MODE_MANUAL) {
        ESP_LOGD(TAG, "Modo MANUAL: Ignorando PIR, aplicando porcentaje directamente");
    } else {
        // Verificar si el sensor PIR detecta presencia (solo en modos no-manuales)
        bool motion_detected = pir_is_motion_active();
        
        // Si no hay presencia detectada, forzar el ventilador a 0%
        if (!motion_detected) {
            percent = 0;
            if (current_pwm_percent > 0) {
                ESP_LOGI(TAG, "⚠️  PIR: No hay presencia detectada, apagando ventilador");
            }
        } else {
            ESP_LOGD(TAG, "✓ PIR: Presencia detectada, ventilador puede funcionar");
        }
    }
    
    // Mapeo del porcentaje a la resolución de 8 bits (0-255)
    uint32_t max_duty = (1 << FAN_DUTY_RES) - 1; // 255 para 8 bits
    uint32_t duty = (percent * max_duty) / 100;
    
    // Actualizar PWM
    ledc_set_duty(LEDC_LOW_SPEED_MODE, FAN_LEDC_CHANNEL, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, FAN_LEDC_CHANNEL);

    current_pwm_percent = percent;
    bool motion_detected = (current_mode != FAN_MODE_MANUAL) ? pir_is_motion_active() : true;
    ESP_LOGD(TAG, "PWM actualizado a %d%% (Duty: %lu) - Modo: %d - PIR: %s", 
             percent, duty, current_mode, motion_detected ? "Presencia/Ignorado" : "Sin presencia");
}

/**
 * @brief Calcula el porcentaje de velocidad basado en la temperatura.
 * @param temp_c Temperatura actual en Celsius.
 * @return Porcentaje de 0 a 100.
 */
static uint8_t calculate_temp_percent(float temp_c) {
    if (temp_c <= FAN_TEMP_MIN_C) {
        return 0; // Apagado
    } else if (temp_c >= FAN_TEMP_MAX_C) {
        return 100; // Máxima velocidad
    } else {
        // Mapeo lineal entre FAN_TEMP_MIN_C y FAN_TEMP_MAX_C
        float range_temp = FAN_TEMP_MAX_C - FAN_TEMP_MIN_C;
        float actual_temp_diff = temp_c - FAN_TEMP_MIN_C;
        
        // pct = ((temp - temp_min) / (temp_max - temp_min)) * 100
        uint8_t percent = (uint8_t)roundf((actual_temp_diff / range_temp) * 100.0f);
        
        // Evitar que el mapeo devuelva un valor muy pequeño (usar histéresis si es necesario)
        if (percent > 0 && percent < 5) {
             percent = 5; // Mínimo para asegurar movimiento (puede ser ajustado)
        }
        
        return percent;
    }
}

// ===== FUNCIONES PÚBLICAS DE CONTROL =====

void fan_init(void) {
    ESP_LOGI(TAG, "Inicializando PWM para control del ventilador...");
    
    // 1. Configuración del Timer
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .timer_num        = FAN_LEDC_TIMER,
        .duty_resolution  = FAN_DUTY_RES,
        .freq_hz          = FAN_FREQUENCY,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // 2. Configuración del Canal
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = FAN_LEDC_CHANNEL,
        .timer_sel      = FAN_LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = FAN_PWM_PIN,
        .duty           = 0,
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
    
    // Inicia en modo OFF (0% PWM)
    set_fan_pwm(0);
    current_mode = FAN_MODE_OFF;
    ESP_LOGI(TAG, "Ventilador inicializado en GPIO %d (8-bit, %dkHz). Modo: OFF", FAN_PWM_PIN, FAN_FREQUENCY/1000);
}

void fan_set_mode(fan_mode_t mode) {
    if (current_mode != mode) {
        current_mode = mode;
        ESP_LOGI(TAG, "Cambiando modo de operación a: %d", mode);
        
        // Forzar actualización inmediata al cambiar de modo
        switch (mode) {
            case FAN_MODE_OFF:
                set_fan_pwm(0);
                break;
            case FAN_MODE_MANUAL:
                // Aplicar el último valor manual establecido
                set_fan_pwm(manual_percent);
                break;
            case FAN_MODE_AUTO_TEMP:
                // Este modo se actualiza a través de fan_update_auto_temp()
                break;
            case FAN_MODE_SCHEDULE:
                // Modo horario: verificar registros inmediatamente
                // La tarea de horarios se encargará de mantenerlo actualizado
                fan_update_from_registros();
                break;
        }
    }
}

fan_mode_t fan_get_mode(void) {
    return current_mode;
}

void fan_set_manual_percent(uint8_t percent) {
    if (percent > 100) percent = 100;
    manual_percent = percent;
    
    // Solo aplicar si estamos en modo MANUAL
    // Si estamos en modo SCHEDULE, no cambiar (el horario tiene prioridad)
    if (current_mode == FAN_MODE_MANUAL) {
        set_fan_pwm(manual_percent);
        ESP_LOGI(TAG, "Porcentaje manual establecido a: %u%%", percent);
    } else if (current_mode == FAN_MODE_SCHEDULE) {
        ESP_LOGW(TAG, "Intento de cambiar velocidad manual en modo horario ignorado. Cambia a modo manual primero.");
    }
}

void fan_set_schedule(const fan_schedule_t *schedule) {
    fan_schedule = *schedule;
    ESP_LOGI(TAG, "Horario de control establecido: %u:00 a %u:00 (Min Temp: %.1f°C)", 
             fan_schedule.start_hour, fan_schedule.end_hour, fan_schedule.min_temp);
}

void fan_update_auto_temp(float current_temp) {
    // Solo actualizar si estamos en modo AUTO_TEMP
    // Si estamos en modo SCHEDULE, no cambiar (el horario tiene prioridad)
    if (current_mode == FAN_MODE_AUTO_TEMP) {
        uint8_t new_percent = calculate_temp_percent(current_temp);
        
        // Solo actualizar si hay un cambio significativo o si está apagado/encendido
        if (new_percent != current_pwm_percent) {
            set_fan_pwm(new_percent);
            ESP_LOGI(TAG, "Modo Auto-Temp: Temp: %.1f°C -> PWM: %u%%", current_temp, new_percent);
        } else {
            ESP_LOGD(TAG, "Modo Auto-Temp: Temp: %.1f°C -> PWM sin cambios", current_temp);
        }
    }
    // Si estamos en modo SCHEDULE, no hacer nada (el horario controla)
}

void fan_update_schedule(float current_temp, uint8_t current_hour) {
    // Esta función se mantiene por compatibilidad, pero ahora el modo SCHEDULE
    // funciona basado en registros, no en horarios simples
    // La función fan_update_from_registros() es la que realmente controla el modo SCHEDULE
    ESP_LOGD(TAG, "fan_update_schedule() llamada (legacy, usar fan_update_from_registros())");
}

/**
 * @brief Actualiza el ventilador basado en registros activos.
 * Verifica si hay un registro activo para el momento actual y ajusta la velocidad.
 * Solo funciona cuando el ventilador está en modo FAN_MODE_SCHEDULE.
 * Durante el modo horario, el ventilador solo funciona según los registros,
 * ignorando otros modos (manual, temperatura).
 */
void fan_update_from_registros(void) {
    if (current_mode != FAN_MODE_SCHEDULE) {
        return; // Solo procesar si estamos en modo SCHEDULE
    }
    
    // Verificar si la hora está sincronizada
    if (!hora_sincronizada()) {
        ESP_LOGW(TAG, "Hora no sincronizada, no se puede verificar registros");
        // Si no hay hora, apagar el ventilador por seguridad
        if (current_pwm_percent > 0) {
            set_fan_pwm(0);
            ESP_LOGI(TAG, "Ventilador apagado: hora no sincronizada");
        }
        return;
    }
    
    // Obtener día y hora actual
    char dia_actual[32];
    char hora_actual[16];
    
    if (!obtener_dia_actual(dia_actual, sizeof(dia_actual)) ||
        !obtener_hora_actual(hora_actual, sizeof(hora_actual))) {
        ESP_LOGW(TAG, "No se pudo obtener día/hora actual (hora sincronizada: %s)", 
                 hora_sincronizada() ? "sí" : "no");
        if (current_pwm_percent > 0) {
            set_fan_pwm(0);
        }
        return;
    }
    
    // Log de depuración cada vez que se verifica (cada verificación para diagnóstico)
    ESP_LOGI(TAG, "🔍 Verificando registros - Día: '%s', Hora: '%s'", dia_actual, hora_actual);
    
    // Verificar si hay un registro activo
    registro_activo_t registro = verificar_registro_activo(dia_actual, hora_actual);
    
    uint8_t new_percent = 0;
    if (registro.activo) {
        new_percent = registro.velocidad;
        ESP_LOGI(TAG, "✓ Registro activo encontrado: %s %s -> velocidad=%d%%", 
                 dia_actual, hora_actual, new_percent);
    } else {
        // No hay registro activo, apagar el ventilador
        new_percent = 0;
        if (current_pwm_percent > 0) {
            ESP_LOGI(TAG, "✗ No hay registro activo para %s %s, apagando ventilador", 
                     dia_actual, hora_actual);
        }
    }
    
    // Actualizar PWM solo si hay cambio
    if (new_percent != current_pwm_percent) {
        set_fan_pwm(new_percent);
    }
}

uint8_t fan_get_current_percent(void) {
    return current_pwm_percent;
}

// ===== TAREA DE CONTROL AUTOMÁTICO POR TEMPERATURA =====

// Configuración de la tarea
#define FAN_AUTO_TEMP_TASK_STACK_SIZE 4096
#define FAN_AUTO_TEMP_TASK_PRIORITY 5
#define FAN_AUTO_TEMP_UPDATE_INTERVAL_MS 1000  // Actualizar cada segundo

/**
 * @brief Tarea que lee la temperatura periódicamente y actualiza el ventilador
 * cuando está en modo FAN_MODE_AUTO_TEMP.
 * Esta tarea se ejecuta de forma independiente y monitorea la temperatura
 * cada segundo para ajustar automáticamente la velocidad del ventilador.
 */
static void fan_auto_temp_task(void *pvParameters) {
    ESP_LOGI(TAG, "Tarea de control automático por temperatura iniciada");
    
    while (1) {
        // Solo actualizar si estamos en modo AUTO_TEMP
        if (current_mode == FAN_MODE_AUTO_TEMP) {
            // Obtener la temperatura actual del sensor
            ntc_data_t temp_data = ntc_get_current_temperature();
            
            // Verificar que la temperatura sea válida
            if (temp_data.temperature_c > -900.0 && 
                isfinite(temp_data.temperature_c) && 
                !isnan(temp_data.temperature_c)) {
                
                // Actualizar el ventilador basado en la temperatura
                fan_update_auto_temp(temp_data.temperature_c);
            } else {
                ESP_LOGW(TAG, "Temperatura no válida, esperando siguiente lectura...");
            }
        }
        
        // Esperar antes de la siguiente lectura
        vTaskDelay(pdMS_TO_TICKS(FAN_AUTO_TEMP_UPDATE_INTERVAL_MS));
    }
}

/**
 * @brief Inicia la tarea de control automático por temperatura.
 * Esta función crea una tarea de FreeRTOS que monitorea la temperatura
 * y actualiza el ventilador automáticamente cuando está en modo AUTO_TEMP.
 */
void fan_start_auto_temp_task(void) {
    BaseType_t ret = xTaskCreate(
        fan_auto_temp_task,
        "fan_auto_temp",
        FAN_AUTO_TEMP_TASK_STACK_SIZE,
        NULL,
        FAN_AUTO_TEMP_TASK_PRIORITY,
        NULL
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Error al crear tarea de control automático por temperatura");
    } else {
        ESP_LOGI(TAG, "Tarea de control automático por temperatura creada");
    }
}

// ===== TAREA DE CONTROL POR HORARIOS (REGISTROS) =====

// Configuración de la tarea
#define FAN_SCHEDULE_TASK_STACK_SIZE 4096
#define FAN_SCHEDULE_TASK_PRIORITY 5
#define FAN_SCHEDULE_UPDATE_INTERVAL_MS 10000  // Verificar cada 10 segundos (más frecuente para detectar cambios)

/**
 * @brief Tarea que verifica periódicamente los registros y actualiza el ventilador
 * cuando está en modo FAN_MODE_SCHEDULE.
 * Esta tarea se ejecuta de forma independiente y verifica los registros cada minuto
 * para activar el ventilador cuando coincide el día y hora con algún registro.
 */
static void fan_schedule_task(void *pvParameters) {
    ESP_LOGI(TAG, "Tarea de control por horarios (registros) iniciada");
    
    while (1) {
        // Solo actualizar si estamos en modo SCHEDULE
        if (current_mode == FAN_MODE_SCHEDULE) {
            ESP_LOGI(TAG, "⏰ Modo SCHEDULE activo, verificando registros...");
            fan_update_from_registros();
        } else {
            static int skip_counter = 0;
            if (skip_counter++ % 6 == 0) {  // Log cada minuto (6 * 10 segundos)
                ESP_LOGI(TAG, "⏭ Modo actual: %d (no es SCHEDULE=%d), saltando verificación", 
                         current_mode, FAN_MODE_SCHEDULE);
            }
        }
        
        // Esperar antes de la siguiente verificación (cada minuto)
        vTaskDelay(pdMS_TO_TICKS(FAN_SCHEDULE_UPDATE_INTERVAL_MS));
    }
}

/**
 * @brief Inicia la tarea de control por horarios (registros).
 * Esta función crea una tarea de FreeRTOS que verifica periódicamente los registros
 * guardados y activa el ventilador cuando coincide el día y hora actual con algún registro.
 * Cuando está en modo FAN_MODE_SCHEDULE, el ventilador solo funciona según los registros.
 */
void fan_start_schedule_task(void) {
    BaseType_t ret = xTaskCreate(
        fan_schedule_task,
        "fan_schedule",
        FAN_SCHEDULE_TASK_STACK_SIZE,
        NULL,
        FAN_SCHEDULE_TASK_PRIORITY,
        NULL
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Error al crear tarea de control por horarios");
    } else {
        ESP_LOGI(TAG, "Tarea de control por horarios (registros) creada");
    }
}