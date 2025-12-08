/*
 * Gestor de Registros NVS
 * 
 * Este módulo proporciona funciones para gestionar registros persistentes
 * en la memoria flash del ESP32 usando NVS (Non-Volatile Storage).
 */

#ifndef NVS_MANAGER_H
#define NVS_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

// Nombre del namespace de NVS para este proyecto
#define NVS_NAMESPACE "registros"

// Nombre de la clave para el registro del ventilador
#define REGISTRO_FAN_KEY "registro_fan"

/**
 * @brief Inicializa el sistema NVS
 * 
 * Esta función debe ser llamada antes de usar cualquier otra función
 * del gestor de registros. Abre el namespace de NVS y lo prepara
 * para operaciones de lectura/escritura.
 * 
 * @return esp_err_t 
 *         - ESP_OK si la inicialización fue exitosa
 *         - Otro código de error en caso de fallo
 */
esp_err_t nvs_manager_init(void);

/**
 * @brief Cierra el sistema NVS
 * 
 * Debe ser llamada al finalizar el uso del gestor de registros
 * para liberar recursos correctamente.
 * 
 * @return esp_err_t 
 *         - ESP_OK si el cierre fue exitoso
 */
esp_err_t nvs_manager_deinit(void);

/**
 * @brief Crea o actualiza un registro en NVS
 * 
 * Guarda un valor entero en NVS con la clave especificada.
 * Si la clave ya existe, actualiza su valor.
 * 
 * @param key Nombre de la clave (ej: REGISTRO_FAN_KEY)
 * @param value Valor entero a guardar
 * @return esp_err_t 
 *         - ESP_OK si la operación fue exitosa
 *         - Otro código de error en caso de fallo
 */
esp_err_t nvs_create_registro(const char* key, int32_t value);

/**
 * @brief Lee un registro de NVS
 * 
 * Lee el valor entero almacenado en NVS para la clave especificada.
 * 
 * @param key Nombre de la clave a leer
 * @param value Puntero donde se guardará el valor leído
 * @return esp_err_t 
 *         - ESP_OK si la lectura fue exitosa
 *         - ESP_ERR_NVS_NOT_FOUND si la clave no existe
 *         - Otro código de error en caso de fallo
 */
esp_err_t nvs_read_registro(const char* key, int32_t* value);

/**
 * @brief Actualiza un registro existente en NVS
 * 
 * Actualiza el valor de una clave existente. Si la clave no existe,
 * la crea (equivalente a nvs_create_registro).
 * 
 * @param key Nombre de la clave a actualizar
 * @param value Nuevo valor entero
 * @return esp_err_t 
 *         - ESP_OK si la actualización fue exitosa
 *         - Otro código de error en caso de fallo
 */
esp_err_t nvs_update_registro(const char* key, int32_t value);

/**
 * @brief Borra un registro de NVS
 * 
 * Elimina la clave y su valor de NVS. Si la clave no existe,
 * la función retorna éxito (idempotente).
 * 
 * @param key Nombre de la clave a borrar
 * @return esp_err_t 
 *         - ESP_OK si el borrado fue exitoso o la clave no existía
 *         - Otro código de error en caso de fallo
 */
esp_err_t nvs_delete_registro(const char* key);

/**
 * @brief Verifica si un registro existe en NVS
 * 
 * Comprueba si una clave existe en NVS sin leer su valor.
 * 
 * @param key Nombre de la clave a verificar
 * @param exists Puntero donde se guardará el resultado (true si existe)
 * @return esp_err_t 
 *         - ESP_OK si la verificación fue exitosa
 *         - Otro código de error en caso de fallo
 */
esp_err_t nvs_registro_exists(const char* key, bool* exists);

#endif // NVS_MANAGER_H

