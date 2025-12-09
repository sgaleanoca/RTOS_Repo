/**
 * ============================================================================
 * ARCHIVO: iot_client.c
 * ============================================================================
 * 
 * RESUMEN:
 * Implementación del cliente IoT para enviar datos del ESP32 al servidor Flask.
 * Usa tareas FreeRTOS, colas y semáforos para envío asíncrono de datos.
 * 
 * ============================================================================
 */

// ===== INCLUDES =====
#include "iot_client.h"

// ESP-IDF
#include <esp_log.h>
#include <esp_http_client.h>
#include <cJSON.h>
#include <esp_wifi.h>
#include <time.h>

// FreeRTOS
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

// Estándar C
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

// ===== DEFINICIONES Y CONSTANTES =====
static const char *TAG = "IOT_CLIENT";

// Configuración de colas
#define TEMP_QUEUE_SIZE 10
#define REGISTRO_QUEUE_SIZE 10

// Configuración de tareas
#define IOT_TASK_STACK_SIZE 8192
#define IOT_TASK_PRIORITY 5

// Timeouts
#define HTTP_TIMEOUT_MS 5000
#define QUEUE_TIMEOUT_MS 1000

// URLs de endpoints
#define TEMP_ENDPOINT "/api/iot/temperature"
#define REGISTRO_ENDPOINT "/api/iot/registro"

// ===== VARIABLES GLOBALES =====
static char server_url[256] = {0};
static bool client_initialized = false;

// Colas para datos
static QueueHandle_t temperature_queue = NULL;
static QueueHandle_t registro_queue = NULL;

// Semáforo para sincronización de envío HTTP
static SemaphoreHandle_t http_mutex = NULL;

// ===== FUNCIONES INTERNAS =====

/**
 * Construye la URL completa del endpoint
 */
static void build_url(char *buffer, size_t buffer_size, const char *endpoint) {
    snprintf(buffer, buffer_size, "%s%s", server_url, endpoint);
}

/**
 * Envía datos HTTP POST al servidor
 * @param url: URL completa del endpoint
 * @param json_body: Cuerpo JSON a enviar
 * @return true si éxito, false si error
 */
static bool send_http_post(const char *url, const char *json_body) {
    if (http_mutex == NULL) {
        ESP_LOGE(TAG, "Mutex HTTP no inicializado");
        return false;
    }

    // Tomar mutex para evitar envíos simultáneos
    if (xSemaphoreTake(http_mutex, pdMS_TO_TICKS(HTTP_TIMEOUT_MS)) != pdTRUE) {
        ESP_LOGW(TAG, "Timeout esperando mutex HTTP");
        return false;
    }

    esp_http_client_config_t config = {
        .url = url,
        .timeout_ms = HTTP_TIMEOUT_MS,
        .method = HTTP_METHOD_POST,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Error inicializando cliente HTTP");
        xSemaphoreGive(http_mutex);
        return false;
    }

    // Configurar headers
    esp_http_client_set_header(client, "Content-Type", "application/json");

    // Enviar datos
    esp_err_t err = esp_http_client_set_post_field(client, json_body, strlen(json_body));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error configurando POST field: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        xSemaphoreGive(http_mutex);
        return false;
    }

    // Ejecutar petición
    err = esp_http_client_perform(client);
    int status_code = esp_http_client_get_status_code(client);

    bool success = (err == ESP_OK && status_code == 200);
    
    if (success) {
        ESP_LOGI(TAG, "Datos enviados correctamente a %s (status: %d)", url, status_code);
    } else {
        ESP_LOGW(TAG, "Error enviando datos a %s (err: %s, status: %d)", 
                 url, esp_err_to_name(err), status_code);
    }

    esp_http_client_cleanup(client);
    xSemaphoreGive(http_mutex);
    
    return success;
}

/**
 * Tarea que procesa y envía datos de temperatura
 */
static void temperature_sender_task(void *pvParameters) {
    iot_temperature_data_t temp_data;
    char url[512];
    char json_buffer[256];
    
    ESP_LOGI(TAG, "Tarea de envío de temperatura iniciada");
    
    build_url(url, sizeof(url), TEMP_ENDPOINT);
    
    while (1) {
        // Esperar datos en la cola
        if (xQueueReceive(temperature_queue, &temp_data, pdMS_TO_TICKS(QUEUE_TIMEOUT_MS)) == pdTRUE) {
            // Crear JSON
            cJSON *json = cJSON_CreateObject();
            cJSON_AddNumberToObject(json, "temperature", temp_data.temperature);
            cJSON_AddNumberToObject(json, "timestamp", temp_data.timestamp);
            
            char *json_string = cJSON_Print(json);
            if (json_string != NULL) {
                // Enviar al servidor
                send_http_post(url, json_string);
                free(json_string);
            }
            
            cJSON_Delete(json);
        }
    }
}

/**
 * Tarea que procesa y envía registros
 */
static void registro_sender_task(void *pvParameters) {
    iot_registro_data_t registro;
    char url[512];
    
    ESP_LOGI(TAG, "Tarea de envío de registros iniciada");
    
    build_url(url, sizeof(url), REGISTRO_ENDPOINT);
    
    while (1) {
        // Esperar datos en la cola
        if (xQueueReceive(registro_queue, &registro, pdMS_TO_TICKS(QUEUE_TIMEOUT_MS)) == pdTRUE) {
            // Crear JSON
            cJSON *json = cJSON_CreateObject();
            cJSON_AddStringToObject(json, "dia", registro.dia);
            cJSON_AddStringToObject(json, "hora", registro.hora);
            cJSON_AddNumberToObject(json, "velocidad", registro.velocidad);
            
            char *json_string = cJSON_Print(json);
            if (json_string != NULL) {
                // Enviar al servidor
                send_http_post(url, json_string);
                free(json_string);
            }
            
            cJSON_Delete(json);
        }
    }
}

// ===== FUNCIONES PÚBLICAS =====

void iot_client_init(const char *server_url_param) {
    if (client_initialized) {
        ESP_LOGW(TAG, "Cliente IoT ya inicializado");
        return;
    }

    ESP_LOGI(TAG, "Inicializando cliente IoT...");

    // Copiar URL del servidor
    if (server_url_param == NULL || strlen(server_url_param) == 0) {
        ESP_LOGE(TAG, "URL del servidor no válida");
        return;
    }
    
    strncpy(server_url, server_url_param, sizeof(server_url) - 1);
    server_url[sizeof(server_url) - 1] = '\0';
    
    ESP_LOGI(TAG, "Servidor configurado: %s", server_url);

    // Crear mutex para sincronización HTTP
    http_mutex = xSemaphoreCreateMutex();
    if (http_mutex == NULL) {
        ESP_LOGE(TAG, "Error creando mutex HTTP");
        return;
    }

    // Crear colas
    temperature_queue = xQueueCreate(TEMP_QUEUE_SIZE, sizeof(iot_temperature_data_t));
    registro_queue = xQueueCreate(REGISTRO_QUEUE_SIZE, sizeof(iot_registro_data_t));

    if (temperature_queue == NULL || registro_queue == NULL) {
        ESP_LOGE(TAG, "Error creando colas");
        return;
    }

    // Crear tareas
    BaseType_t ret1 = xTaskCreate(temperature_sender_task, 
                                  "iot_temp_sender", 
                                  IOT_TASK_STACK_SIZE, 
                                  NULL, 
                                  IOT_TASK_PRIORITY, 
                                  NULL);

    BaseType_t ret2 = xTaskCreate(registro_sender_task, 
                                  "iot_registro_sender", 
                                  IOT_TASK_STACK_SIZE, 
                                  NULL, 
                                  IOT_TASK_PRIORITY, 
                                  NULL);

    if (ret1 != pdPASS || ret2 != pdPASS) {
        ESP_LOGE(TAG, "Error creando tareas");
        return;
    }

    client_initialized = true;
    ESP_LOGI(TAG, "Cliente IoT inicializado correctamente");
}

bool iot_send_temperature(iot_temperature_data_t *temp_data) {
    if (!client_initialized || temperature_queue == NULL) {
        ESP_LOGW(TAG, "Cliente IoT no inicializado");
        return false;
    }

    if (temp_data == NULL) {
        ESP_LOGE(TAG, "Datos de temperatura NULL");
        return false;
    }

    // Enviar a la cola (sin bloquear si está llena)
    if (xQueueSend(temperature_queue, temp_data, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Cola de temperatura llena, descartando dato");
        return false;
    }

    return true;
}

bool iot_send_registro(iot_registro_data_t *registro) {
    if (!client_initialized || registro_queue == NULL) {
        ESP_LOGW(TAG, "Cliente IoT no inicializado");
        return false;
    }

    if (registro == NULL) {
        ESP_LOGE(TAG, "Datos de registro NULL");
        return false;
    }

    // Enviar a la cola (sin bloquear si está llena)
    if (xQueueSend(registro_queue, registro, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Cola de registros llena, descartando dato");
        return false;
    }

    return true;
}

bool iot_client_is_ready(void) {
    return client_initialized;
}
