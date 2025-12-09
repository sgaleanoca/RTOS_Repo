/**
 * ============================================================================
 * ARCHIVO: iot_client.h
 * ============================================================================
 * 
 * RESUMEN:
 * Cliente IoT para enviar datos del ESP32 al servidor Flask en Raspberry Pi.
 * Este módulo implementa un sistema completo de envío de datos usando:
 * - Tareas FreeRTOS para procesamiento asíncrono
 * - Colas para comunicación entre tareas
 * - Semáforos para sincronización
 * - Cliente HTTP para enviar datos al servidor
 * 
 * Datos enviados:
 * - Temperatura en tiempo real (cada segundo)
 * - Registros de horarios cuando se guardan
 * 
 * ============================================================================
 */

#ifndef IOT_CLIENT_H
#define IOT_CLIENT_H

#include <stdbool.h>
#include <stdint.h>

// ===== ESTRUCTURAS DE DATOS =====
/**
 * Estructura para datos de temperatura a enviar
 */
typedef struct {
    float temperature;
    uint32_t timestamp;
} iot_temperature_data_t;

/**
 * Estructura para datos de registro a enviar
 */
typedef struct {
    char dia[16];
    char hora[16];
    int velocidad;
} iot_registro_data_t;

// ===== PROTOTIPOS DE FUNCIONES =====

/**
 * Inicializa el cliente IoT
 * - Configura la URL del servidor Flask
 * - Crea colas y semáforos
 * - Inicia tareas de envío de datos
 * 
 * @param server_url: URL del servidor Flask (ej: "http://192.168.4.1:5000")
 */
void iot_client_init(const char *server_url);

/**
 * Envía datos de temperatura al servidor
 * Publica los datos en la cola para que la tarea los procese
 * 
 * @param temp_data: Datos de temperatura a enviar
 * @return true si se pudo agregar a la cola, false en caso contrario
 */
bool iot_send_temperature(iot_temperature_data_t *temp_data);

/**
 * Envía un registro al servidor
 * Publica los datos en la cola para que la tarea los procese
 * 
 * @param registro: Datos del registro a enviar
 * @return true si se pudo agregar a la cola, false en caso contrario
 */
bool iot_send_registro(iot_registro_data_t *registro);

/**
 * Obtiene el estado del cliente IoT
 * @return true si está inicializado y funcionando, false en caso contrario
 */
bool iot_client_is_ready(void);

#endif // IOT_CLIENT_H
