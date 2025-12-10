#ifndef FAN_CONTROL_H
#define FAN_CONTROL_H

#include <stdint.h>
#include <stdbool.h>

// --- Configuración de Pines y PWM ---
#define FAN_PWM_PIN         26              // Pin GPIO para el control PWM del ventilador (gate del MOSFET)
#define FAN_LEDC_TIMER      LEDC_TIMER_1    // Usaremos un timer diferente para el ventilador
#define FAN_LEDC_CHANNEL    LEDC_CHANNEL_2
#define FAN_DUTY_RES        LEDC_TIMER_8_BIT // Resolución de 8 bits (0-255)
#define FAN_FREQUENCY       (25000)         // Frecuencia en Hz, 25kHz es común para motores

// --- Configuración de Temperaturas ---
#define FAN_TEMP_MIN_C      15.0f           // Temperatura mínima para ENCENDER el ventilador (0% PWM)
#define FAN_TEMP_MAX_C      25.0f           // Temperatura máxima para 100% PWM
#define FAN_TEMP_HYSTERESIS 1.0f            // Histéresis para el punto de apagado (opcional)

// --- Estructura para el Modo Programado ---
typedef struct {
    uint8_t start_hour; // Hora de inicio (0-23)
    uint8_t end_hour;   // Hora de fin (0-23)
    float min_temp;     // Temperatura mínima para encender dentro del horario
} fan_schedule_t;

// --- Modos de Operación del Ventilador ---
typedef enum {
    FAN_MODE_OFF,       // Apagado forzado (0% PWM)
    FAN_MODE_MANUAL,    // Control manual por porcentaje
    FAN_MODE_AUTO_TEMP, // Control automático por temperatura (22°C-36°C)
    FAN_MODE_SCHEDULE   // Control por horario con chequeo de temperatura
} fan_mode_t;

// --- Funciones Públicas ---

/**
 * @brief Inicializa el PWM para el control del ventilador.
 */
void fan_init(void);

/**
 * @brief Establece el modo de operación del ventilador.
 * @param mode Modo a establecer.
 */
void fan_set_mode(fan_mode_t mode);

/**
 * @brief Obtiene el modo de operación actual.
 * @return Modo actual.
 */
fan_mode_t fan_get_mode(void);

/**
 * @brief Establece el porcentaje de velocidad en modo manual.
 * @param percent Porcentaje de 0 a 100.
 */
void fan_set_manual_percent(uint8_t percent);

/**
 * @brief Establece la configuración para el modo programado.
 * @param schedule Configuración de horario y temperatura mínima.
 */
void fan_set_schedule(const fan_schedule_t *schedule);

/**
 * @brief Actualiza la velocidad del ventilador en modo automático por temperatura.
 * Debe ser llamada periódicamente con la temperatura actual.
 * @param current_temp Temperatura actual en Celsius.
 */
void fan_update_auto_temp(float current_temp);

/**
 * @brief Actualiza la velocidad del ventilador en modo programado.
 * Debe ser llamada periódicamente con la temperatura y la hora actual.
 * @param current_temp Temperatura actual en Celsius.
 * @param current_hour Hora actual (0-23).
 */
void fan_update_schedule(float current_temp, uint8_t current_hour);

/**
 * @brief Obtiene el porcentaje de velocidad PWM actual.
 * @return Porcentaje de 0 a 100.
 */
uint8_t fan_get_current_percent(void);

/**
 * @brief Inicia la tarea de control automático por temperatura.
 * Esta tarea lee la temperatura periódicamente y actualiza el ventilador
 * cuando está en modo FAN_MODE_AUTO_TEMP.
 * Requiere que ntc_sensor_init() y ntc_start_reading_task() hayan sido llamados.
 */
void fan_start_auto_temp_task(void);

/**
 * @brief Inicia la tarea de control por horarios (registros).
 * Esta tarea verifica periódicamente los registros guardados y activa el ventilador
 * cuando coincide el día y hora actual con algún registro.
 * Cuando está en modo FAN_MODE_SCHEDULE, el ventilador solo funciona según los registros.
 * Requiere que time_sync_init() haya sido llamado y la hora esté sincronizada.
 */
void fan_start_schedule_task(void);

/**
 * @brief Actualiza el ventilador basado en registros activos.
 * Debe ser llamada periódicamente cuando el ventilador está en modo SCHEDULE.
 * Verifica si hay un registro activo para el momento actual y ajusta la velocidad.
 */
void fan_update_from_registros(void);

#endif // FAN_CONTROL_H