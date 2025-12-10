/**
 * @file registros.c
 * @brief Implementación de la gestión de registros de horarios
 * @author Proyecto Final RTOS
 * @date 2024
 * 
 * @details Implementación de la gestión de registros de horarios del ventilador.
 * Este módulo proporciona funciones para almacenar y leer registros de forma
 * persistente en la partición SPIFFS del ESP32.
 * 
 * @section functionality Funcionalidades
 * - Creación automática del archivo registros.json si no existe
 * - Guardado de registros en formato JSON en /spiffs/registros.json
 * - Lectura de todos los registros almacenados
 * - Verificación de registros activos para el día y hora actual
 * - Persistencia de datos tras reinicio del ESP32
 * 
 * @section usage Uso
 * Los registros se utilizan desde el módulo web_server.c a través de los
 * endpoints HTTP GET /registros y POST /registros, y desde fan_control.c
 * para el modo SCHEDULE del ventilador.
 * 
 * @section format Formato de registro
 * @code{.json}
 * {
 *   "dia": "lunes",
 *   "hora": "14:30",
 *   "velocidad": 50,
 *   "id": "1234567890"
 * }
 * @endcode
 * 
 * ============================================================================
 * ARCHIVO: registros.c
 * ============================================================================
 * 
 * RESUMEN:
 * Implementación de la gestión de registros de horarios del ventilador.
 * Este módulo proporciona funciones para almacenar y leer registros de forma
 * persistente en la partición SPIFFS del ESP32.
 * 
 * Funcionalidades:
 * - Creación automática del archivo registros.json si no existe
 * - Guardado de registros en formato JSON en /spiffs/registros.json
 * - Lectura de todos los registros almacenados
 * - Verificación de registros activos para el día y hora actual
 * - Persistencia de datos tras reinicio del ESP32
 * 
 * Los registros se utilizan desde el módulo web_server.c a través de los
 * endpoints HTTP GET /registros y POST /registros, y desde fan_control.c
 * para el modo SCHEDULE del ventilador.
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
 * Sección 1: INCLUDES se encuentra en las líneas 44 a 60
 * Sección 2: DEFINICIONES Y CONSTANTES se encuentra en las líneas 62 a 63
 * Sección 3: FUNCIONES INTERNAS se encuentra en las líneas 65 a 76
 * Sección 4: FUNCIONES PÚBLICAS se encuentra en las líneas 78 a 325
 * ============================================================================
 * 
 * ============================================================================
 * RESUMEN DE TAREAS, COLAS Y SEMÁFOROS IMPLEMENTADOS:
 * ============================================================================
 * 
 * === TAREAS (TASKS) ===
 * 
 * Ninguna en este módulo. Las funciones se llaman desde otras tareas.
 * 
 * === COLAS (QUEUES) ===
 * 
 * Ninguna en este módulo.
 * 
 * === SEMÁFOROS (MUTEXES) ===
 * 
 * Ninguno en este módulo. El acceso a archivos SPIFFS es thread-safe
 * mediante las funciones estándar de C (fopen, fread, fwrite).
 * 
 * ============================================================================
 */

// ===== INCLUDES =====
#include "registros.h"

// ESP-IDF
#include <esp_log.h>
#include <cJSON.h>

// FreeRTOS
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Estándar C
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>

// ===== DEFINICIONES Y CONSTANTES =====
static const char *TAG = "REGISTROS";

// ===== FUNCIONES INTERNAS =====
/**
 * Obtiene el tiempo actual en milisegundos desde el inicio del sistema
 * @return Tiempo en milisegundos
 */
static int64_t get_time_ms(void) {
    return (int64_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

// ===== FUNCIONES PÚBLICAS =====
/**
 * Crea el archivo registros.json en SPIFFS si no existe
 * Inicializa el archivo con un array JSON vacío []
 * Esta función debe llamarse durante la inicialización del servidor web
 * para asegurar que el archivo existe antes de intentar leer/escribir registros
 */
void crear_archivo_si_no_existe(void) {
    FILE *f = fopen("/spiffs/registros.json", "r");
    if (f == NULL) {
        ESP_LOGW(TAG, "registros.json no existe. Creando...");
        f = fopen("/spiffs/registros.json", "w");
        if (f == NULL) {
            ESP_LOGE(TAG, "Error creando registros.json");
            return;
        }
        fprintf(f, "[]");  // JSON vacío
        fclose(f);
        ESP_LOGI(TAG, "Archivo registros.json creado correctamente.");
    } else {
        fclose(f);
    }
}

/**
 * Agrega un registro al archivo registros.json en SPIFFS
 * Lee el archivo completo, agrega el nuevo registro al array JSON,
 * y guarda el archivo actualizado de forma persistente.
 * 
 * @param dia: Día de la semana (ej: "lunes", "martes", etc.)
 * @param hora: Hora en formato HH:MM (ej: "14:30")
 * @param velocidad: Velocidad del ventilador (0-100)
 * @return true si éxito, false si error (archivo no existe, error de escritura, etc.)
 * 
 * Nota: El registro incluye un ID único basado en timestamp para compatibilidad
 * con el frontend que espera un campo "id" en cada registro.
 */
bool agregar_registro(const char *dia, const char *hora, int velocidad) {
    FILE *f = fopen("/spiffs/registros.json", "r");
    if (!f) {
        ESP_LOGE(TAG, "No se puede abrir registros.json en modo lectura");
        return false;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    if (size < 0) {
        fclose(f);
        ESP_LOGE(TAG, "Error obteniendo tamaño del archivo");
        return false;
    }

    char *buffer = malloc(size + 1);
    if (!buffer) {
        fclose(f);
        ESP_LOGE(TAG, "Error al asignar memoria");
        return false;
    }

    size_t read_size = fread(buffer, 1, size, f);
    fclose(f);
    buffer[read_size] = '\0';

    cJSON *root = cJSON_Parse(buffer);
    free(buffer);

    if (!root) {
        ESP_LOGE(TAG, "Error parseando JSON");
        return false;
    }

    // Asegurar que root es un array
    if (!cJSON_IsArray(root)) {
        cJSON_Delete(root);
        root = cJSON_CreateArray();
    }

    cJSON *nuevo = cJSON_CreateObject();
    cJSON_AddStringToObject(nuevo, "dia", dia);
    cJSON_AddStringToObject(nuevo, "hora", hora);
    cJSON_AddNumberToObject(nuevo, "velocidad", velocidad);
    
    // Agregar timestamp para compatibilidad con frontend
    // Usar un ID único basado en el tiempo actual
    int64_t timestamp_ms = get_time_ms();
    char timestamp[32];
    snprintf(timestamp, sizeof(timestamp), "%" PRId64, timestamp_ms);
    cJSON_AddStringToObject(nuevo, "id", timestamp);
    
    cJSON_AddItemToArray(root, nuevo);

    char *json_str = cJSON_Print(root);
    cJSON_Delete(root);

    if (!json_str) {
        ESP_LOGE(TAG, "Error generando JSON string");
        return false;
    }

    f = fopen("/spiffs/registros.json", "w");
    if (!f) {
        ESP_LOGE(TAG, "Error abriendo archivo para escribir");
        free(json_str);
        return false;
    }

    fwrite(json_str, 1, strlen(json_str), f);
    fclose(f);
    free(json_str);

    ESP_LOGI(TAG, "Registro guardado correctamente: %s %s velocidad=%d", dia, hora, velocidad);
    return true;
}

/**
 * Lee todos los registros del archivo registros.json desde SPIFFS
 * Retorna un string JSON con el array completo de registros almacenados.
 * 
 * @return String JSON con todos los registros (debe ser liberado con free())
 *         Retorna "[]" (array vacío) si hay error, el archivo no existe o está vacío
 * 
 * Formato del JSON retornado:
 * [
 *   {"dia": "lunes", "hora": "14:30", "velocidad": 50, "id": "1234567890"},
 *   {"dia": "martes", "hora": "08:00", "velocidad": 75, "id": "1234567891"},
 *   ...
 * ]
 */
char *leer_registros_json(void) {
    FILE *f = fopen("/spiffs/registros.json", "r");
    if (!f) {
        ESP_LOGW(TAG, "No se puede abrir registros.json, retornando array vacío");
        return strdup("[]");
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    if (size <= 0) {
        fclose(f);
        return strdup("[]");
    }

    char *buffer = malloc(size + 1);
    if (!buffer) {
        fclose(f);
        return strdup("[]");
    }

    size_t read_size = fread(buffer, 1, size, f);
    fclose(f);
    buffer[read_size] = '\0';

    // Validar que sea JSON válido
    cJSON *test = cJSON_Parse(buffer);
    if (!test) {
        ESP_LOGW(TAG, "JSON inválido en registros.json, retornando array vacío");
        free(buffer);
        return strdup("[]");
    }
    cJSON_Delete(test);

    return buffer;
}

/**
 * Convierte el nombre del día en español a minúsculas normalizado
 * @param dia: Día en español (puede tener mayúsculas)
 * @param output: Buffer donde se almacenará el día normalizado
 * @param output_size: Tamaño del buffer
 */
static void normalizar_dia(const char *dia, char *output, size_t output_size) {
    size_t i = 0;
    while (dia[i] != '\0' && i < output_size - 1) {
        if (dia[i] >= 'A' && dia[i] <= 'Z') {
            output[i] = dia[i] - 'A' + 'a';
        } else {
            output[i] = dia[i];
        }
        i++;
    }
    output[i] = '\0';
}

/**
 * Verifica si hay un registro activo para el día y hora actuales
 * Lee todos los registros desde SPIFFS y busca coincidencias exactas
 * de día y hora. Si encuentra una coincidencia, retorna la velocidad
 * del registro.
 * 
 * @param dia_actual: Día actual en español (ej: "lunes", "martes", etc.)
 * @param hora_actual: Hora actual en formato HH:MM (ej: "14:30")
 * @return registro_activo_t con la velocidad si hay registro activo, 
 *         o velocidad=0 y activo=false si no hay coincidencia
 */
registro_activo_t verificar_registro_activo(const char *dia_actual, const char *hora_actual) {
    registro_activo_t resultado = {.velocidad = 0, .activo = false};
    
    // Leer todos los registros
    char *json_str = leer_registros_json();
    if (!json_str) {
        return resultado;
    }
    
    cJSON *root = cJSON_Parse(json_str);
    free(json_str);
    
    if (!root || !cJSON_IsArray(root)) {
        if (root) cJSON_Delete(root);
        return resultado;
    }
    
    // Normalizar día actual para comparación
    char dia_normalizado[32];
    normalizar_dia(dia_actual, dia_normalizado, sizeof(dia_normalizado));
    
    // Buscar en todos los registros
    int array_size = cJSON_GetArraySize(root);
    ESP_LOGI(TAG, "📋 Buscando registro activo. Total registros en archivo: %d", array_size);
    
    if (array_size == 0) {
        ESP_LOGW(TAG, "⚠ No hay registros guardados en el archivo");
    }
    
    for (int i = 0; i < array_size; i++) {
        cJSON *item = cJSON_GetArrayItem(root, i);
        if (!item) continue;
        
        cJSON *dia_item = cJSON_GetObjectItem(item, "dia");
        cJSON *hora_item = cJSON_GetObjectItem(item, "hora");
        cJSON *velocidad_item = cJSON_GetObjectItem(item, "velocidad");
        
        if (!dia_item || !hora_item || !velocidad_item ||
            !cJSON_IsString(dia_item) || !cJSON_IsString(hora_item) || !cJSON_IsNumber(velocidad_item)) {
            ESP_LOGW(TAG, "⚠ Registro %d tiene campos inválidos, saltando", i);
            continue;
        }
        
        // Normalizar día del registro
        char dia_registro[32];
        normalizar_dia(dia_item->valuestring, dia_registro, sizeof(dia_registro));
        
        ESP_LOGI(TAG, "  [%d] Comparando: día='%s' vs '%s' | hora='%s' vs '%s' | velocidad=%d%%",
                 i, dia_normalizado, dia_registro, hora_actual, hora_item->valuestring, velocidad_item->valueint);
        
        // Comparar día y hora (comparación exacta de HH:MM)
        bool dia_coincide = (strcmp(dia_normalizado, dia_registro) == 0);
        bool hora_coincide = (strcmp(hora_actual, hora_item->valuestring) == 0);
        
        if (dia_coincide && hora_coincide) {
            // Coincidencia encontrada
            resultado.velocidad = velocidad_item->valueint;
            resultado.activo = true;
            ESP_LOGI(TAG, "✅ ¡COINCIDENCIA ENCONTRADA! Registro %d: %s %s -> velocidad=%d%%", 
                     i, dia_actual, hora_actual, resultado.velocidad);
            break;
        } else {
            ESP_LOGI(TAG, "  ❌ No coincide: día=%s, hora=%s", 
                     dia_coincide ? "✓" : "✗", hora_coincide ? "✓" : "✗");
        }
    }
    
    if (!resultado.activo) {
        ESP_LOGW(TAG, "❌ No se encontró registro activo para '%s' '%s'", dia_actual, hora_actual);
    }
    
    cJSON_Delete(root);
    return resultado;
}
