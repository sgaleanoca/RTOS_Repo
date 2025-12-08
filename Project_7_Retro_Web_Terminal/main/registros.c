/**
 * ============================================================================
 * ARCHIVO: registros.c
 * ============================================================================
 * 
 * RESUMEN:
 * Implementación de la gestión de registros de horarios del ventilador.
 * Las funciones guardan y leen registros desde el archivo registros.json
 * en la partición SPIFFS.
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
 * Crea el archivo registros.json si no existe
 * Inicializa con un array JSON vacío []
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
 * Agrega un registro al archivo registros.json
 * @param dia: Día de la semana (ej: "lunes")
 * @param hora: Hora en formato HH:MM (ej: "14:30")
 * @param velocidad: Velocidad del ventilador (0-100)
 * @return true si éxito, false si error
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
 * Lee todos los registros del archivo registros.json
 * @return String JSON con todos los registros (debe ser liberado con free())
 *         Retorna "[]" si hay error o el archivo está vacío
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
