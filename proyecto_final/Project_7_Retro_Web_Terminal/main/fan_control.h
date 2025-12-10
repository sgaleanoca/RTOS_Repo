/**
 * @file fan_control.h
 * @brief Controlador de ventilador mediante PWM para ESP32
 * @author Proyecto Final RTOS
 * @date 2024
 * 
 * @details Header file para el módulo de control del ventilador mediante PWM.
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
 * ARCHIVO: fan_control.h
 * ============================================================================
 * 
 * RESUMEN:
 * Header file para el módulo de control del ventilador mediante PWM.
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
 * Sección 1: CONFIGURACIÓN DE PINES Y PWM se encuentra en las líneas 45 a 50
 * Sección 2: CONFIGURACIÓN DE TEMPERATURAS se encuentra en las líneas 52 a 54
 * Sección 3: ESTRUCTURAS DE DATOS se encuentra en las líneas 56 a 61
 * Sección 4: MODOS DE OPERACIÓN se encuentra en las líneas 63 a 68
 * Sección 5: PROTOTIPOS DE FUNCIONES se encuentra en las líneas 70 a 108
 * ============================================================================
 */

#ifndef FAN_CONTROL_H
#define FAN_CONTROL_H

#include <stdint.h>
#include <stdbool.h>

// ===== CONFIGURACIÓN DE PINES Y PWM =====
// Configuración del hardware para control PWM del ventilador
#define FAN_PWM_PIN         26              // Pin GPIO para el control PWM del ventilador (gate del MOSFET)
#define FAN_LEDC_TIMER      LEDC_TIMER_1    // Timer LEDC a usar (diferente al del LED RGB)
#define FAN_LEDC_CHANNEL    LEDC_CHANNEL_2   // Canal LEDC para el ventilador
#define FAN_DUTY_RES        LEDC_TIMER_8_BIT // Resolución de 8 bits (0-255)
#define FAN_FREQUENCY       (25000)         // Frecuencia en Hz, 25kHz es común para motores

// ===== CONFIGURACIÓN DE TEMPERATURAS =====
// Rangos de temperatura para control automático
#define FAN_TEMP_MIN_C      15.0f           // Temperatura mínima para ENCENDER el ventilador (0% PWM)
#define FAN_TEMP_MAX_C      25.0f           // Temperatura máxima para 100% PWM
#define FAN_TEMP_HYSTERESIS 1.0f            // Histéresis para el punto de apagado (opcional)

// ===== ESTRUCTURAS DE DATOS =====
/**
 * Estructura para el modo programado (legacy, actualmente se usan registros)
 * @note Esta estructura se mantiene por compatibilidad, pero el modo SCHEDULE
 *       ahora funciona basado en registros guardados en SPIFFS
 */
typedef struct {
    uint8_t start_hour; // Hora de inicio (0-23)
    uint8_t end_hour;   // Hora de fin (0-23)
    float min_temp;     // Temperatura mínima para encender dentro del horario
} fan_schedule_t;

// ===== MODOS DE OPERACIÓN =====
/**
 * Modos de operación del ventilador
 */
typedef enum {
    FAN_MODE_OFF,       // Apagado forzado (0% PWM)
    FAN_MODE_MANUAL,    // Control manual por porcentaje (0-100%), ignora PIR
    FAN_MODE_AUTO_TEMP, // Control automático por temperatura (15-25°C), requiere PIR
    FAN_MODE_SCHEDULE   // Control por horario usando registros guardados, requiere PIR
} fan_mode_t;

// ===== PROTOTIPOS DE FUNCIONES =====

/**
 * @brief Inicializa el PWM para el control del ventilador
 * 
 * Configura el timer LEDC y el canal PWM para controlar el ventilador.
 * Esta función debe llamarse una vez durante la inicialización del sistema
 * antes de usar cualquier otra función de este módulo.
 * 
 * Proceso de inicialización:
 * 1. Configura el timer LEDC con resolución de 8 bits y frecuencia de 25kHz
 * 2. Configura el canal LEDC para el GPIO del ventilador
 * 3. Inicia el ventilador en modo OFF (0% PWM)
 */
void fan_init(void);

/**
 * @brief Establece el modo de operación del ventilador
 * 
 * Cambia el modo de operación del ventilador y aplica la configuración correspondiente.
 * Al cambiar de modo, se actualiza inmediatamente la velocidad del ventilador.
 * 
 * @param mode Modo a establecer (FAN_MODE_OFF, FAN_MODE_MANUAL, FAN_MODE_AUTO_TEMP, FAN_MODE_SCHEDULE)
 */
void fan_set_mode(fan_mode_t mode);

/**
 * @brief Obtiene el modo de operación actual
 * 
 * @return Modo actual del ventilador
 */
fan_mode_t fan_get_mode(void);

/**
 * @brief Establece el porcentaje de velocidad en modo manual
 * 
 * Solo tiene efecto si el ventilador está en modo FAN_MODE_MANUAL.
 * En modo MANUAL, el ventilador ignora el sensor PIR y funciona directamente.
 * 
 * @param percent Porcentaje de velocidad (0-100)
 */
void fan_set_manual_percent(uint8_t percent);

/**
 * @brief Establece la configuración para el modo programado (legacy)
 * 
 * @note Esta función se mantiene por compatibilidad, pero el modo SCHEDULE
 *       ahora funciona basado en registros guardados en SPIFFS
 * 
 * @param schedule Configuración de horario y temperatura mínima
 */
void fan_set_schedule(const fan_schedule_t *schedule);

/**
 * @brief Actualiza la velocidad del ventilador en modo automático por temperatura
 * 
 * Calcula el porcentaje de velocidad basado en la temperatura actual
 * usando un mapeo lineal entre FAN_TEMP_MIN_C y FAN_TEMP_MAX_C.
 * Solo tiene efecto si el ventilador está en modo FAN_MODE_AUTO_TEMP.
 * 
 * @param current_temp Temperatura actual en Celsius
 */
void fan_update_auto_temp(float current_temp);

/**
 * @brief Actualiza la velocidad del ventilador en modo programado (legacy)
 * 
 * @note Esta función se mantiene por compatibilidad, pero el modo SCHEDULE
 *       ahora funciona basado en registros. Usar fan_update_from_registros().
 * 
 * @param current_temp Temperatura actual en Celsius
 * @param current_hour Hora actual (0-23)
 */
void fan_update_schedule(float current_temp, uint8_t current_hour);

/**
 * @brief Obtiene el porcentaje de velocidad PWM actual
 * 
 * @return Porcentaje de velocidad actual (0-100)
 */
uint8_t fan_get_current_percent(void);

/**
 * @brief Inicia la tarea de control automático por temperatura
 * 
 * Crea una tarea de FreeRTOS que lee la temperatura periódicamente (cada 1 segundo)
 * y actualiza el ventilador automáticamente cuando está en modo FAN_MODE_AUTO_TEMP.
 * 
 * Requisitos:
 * - ntc_sensor_init() debe haber sido llamado
 * - ntc_start_reading_task() debe haber sido llamado
 * 
 * La tarea se ejecuta de forma independiente y monitorea la temperatura continuamente.
 */
void fan_start_auto_temp_task(void);

/**
 * @brief Inicia la tarea de control por horarios (registros)
 * 
 * Crea una tarea de FreeRTOS que verifica periódicamente los registros guardados
 * (cada 10 segundos) y activa el ventilador cuando coincide el día y hora actual
 * con algún registro guardado en SPIFFS.
 * 
 * Requisitos:
 * - time_sync_init() debe haber sido llamado
 * - La hora debe estar sincronizada (hora_sincronizada() == true)
 * - Los registros deben estar guardados en /spiffs/registros.json
 * 
 * Cuando está en modo FAN_MODE_SCHEDULE, el ventilador solo funciona según los registros.
 */
void fan_start_schedule_task(void);

/**
 * @brief Actualiza el ventilador basado en registros activos
 * 
 * Verifica si hay un registro activo para el día y hora actuales y ajusta
 * la velocidad del ventilador según el registro encontrado.
 * 
 * Solo tiene efecto si el ventilador está en modo FAN_MODE_SCHEDULE.
 * Si no hay registro activo, el ventilador se apaga (0% PWM).
 * 
 * Requisitos:
 * - La hora debe estar sincronizada
 * - Debe poder obtener día y hora actual
 */
void fan_update_from_registros(void);

#endif // FAN_CONTROL_H