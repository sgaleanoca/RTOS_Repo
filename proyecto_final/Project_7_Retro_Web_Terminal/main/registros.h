/**
 * ============================================================================
 * ARCHIVO: registros.h
 * ============================================================================
 * 
 * RESUMEN:
 * Header file para la gestión de registros de horarios del ventilador.
 * Proporciona funciones para guardar y leer registros desde SPIFFS de forma
 * persistente. Los registros se almacenan en /spiffs/registros.json.
 * 
 * Este módulo es utilizado por:
 * - web_server.c: Para manejar los endpoints HTTP GET /registros y POST /registros
 * - fan_control.c: Para verificar registros activos en modo SCHEDULE
 * 
 * Formato de registro:
 * {
 *   "dia": "lunes",
 *   "hora": "14:30",
 *   "velocidad": 50,
 *   "id": "1234567890"
 * }
 * 
 * ============================================================================
 * ÍNDICE DE SECCIONES:
 * ============================================================================
 * Sección 1: PROTOTIPOS DE FUNCIONES se encuentra en las líneas 25 a 58
 * ============================================================================
 */

#ifndef REGISTROS_H
#define REGISTROS_H

#include <stdbool.h>

/**
 * Crea el archivo registros.json si no existe
 * Inicializa con un array JSON vacío []
 */
void crear_archivo_si_no_existe(void);

/**
 * Agrega un registro al archivo registros.json
 * @param dia: Día de la semana (ej: "lunes")
 * @param hora: Hora en formato HH:MM (ej: "14:30")
 * @param velocidad: Velocidad del ventilador (0-100)
 * @return true si éxito, false si error
 */
bool agregar_registro(const char *dia, const char *hora, int velocidad);

/**
 * Lee todos los registros del archivo registros.json
 * @return String JSON con todos los registros (debe ser liberado con free())
 *         Retorna "[]" si hay error o el archivo está vacío
 */
char *leer_registros_json(void);

/**
 * Estructura para representar un registro activo
 */
typedef struct {
    int velocidad;  // Velocidad del ventilador (0-100)
    bool activo;    // Si hay un registro activo para el momento actual
} registro_activo_t;

/**
 * Verifica si hay un registro activo para el día y hora actuales
 * @param dia_actual: Día actual en español (ej: "lunes", "martes", etc.)
 * @param hora_actual: Hora actual en formato HH:MM (ej: "14:30")
 * @return registro_activo_t con la velocidad si hay registro activo, o velocidad=0 y activo=false si no
 */
registro_activo_t verificar_registro_activo(const char *dia_actual, const char *hora_actual);

#endif // REGISTROS_H
