/**
 * ============================================================================
 * ARCHIVO: time_sync.h
 * ============================================================================
 * 
 * RESUMEN:
 * Header para la sincronización de tiempo usando SNTP (Simple Network Time Protocol).
 * Este módulo permite obtener la hora y fecha actual del sistema, necesaria
 * para el sistema de horarios del ventilador.
 * 
 * ============================================================================
 */

#ifndef TIME_SYNC_H
#define TIME_SYNC_H

#include <stdbool.h>
#include <stddef.h>

/**
 * Inicializa el servicio SNTP para sincronizar la hora del sistema
 * Debe llamarse después de que WiFi esté conectado
 */
void time_sync_init(void);

/**
 * Obtiene el día de la semana actual en español
 * @param dia_buffer: Buffer donde se almacenará el día (ej: "lunes", "martes")
 * @param buffer_size: Tamaño del buffer
 * @return true si éxito, false si la hora no está sincronizada
 */
bool obtener_dia_actual(char *dia_buffer, size_t buffer_size);

/**
 * Obtiene la hora actual en formato HH:MM
 * @param hora_buffer: Buffer donde se almacenará la hora (ej: "14:30")
 * @param buffer_size: Tamaño del buffer
 * @return true si éxito, false si la hora no está sincronizada
 */
bool obtener_hora_actual(char *hora_buffer, size_t buffer_size);

/**
 * Verifica si la hora del sistema está sincronizada
 * @return true si está sincronizada, false en caso contrario
 */
bool hora_sincronizada(void);

/**
 * Establece la hora manualmente (útil cuando SNTP no puede sincronizar)
 * @param year: Año (ej: 2024)
 * @param month: Mes (1-12)
 * @param day: Día (1-31)
 * @param hour: Hora (0-23)
 * @param minute: Minuto (0-59)
 * @param second: Segundo (0-59)
 * @return true si éxito, false si error
 */
bool establecer_hora_manual(int year, int month, int day, int hour, int minute, int second);

#endif // TIME_SYNC_H

