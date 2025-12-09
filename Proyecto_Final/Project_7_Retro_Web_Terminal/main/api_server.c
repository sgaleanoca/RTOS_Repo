/**
 * ============================================================================
 * ARCHIVO: api_server.c
 * ============================================================================
 * 
 * RESUMEN:
 * Implementación del servidor API REST del ESP32. Este módulo proporciona
 * endpoints REST livianos para controlar el hardware sin servir HTML/CSS/JS.
 * 
 * Endpoints REST disponibles:
 * - GET /api/temperature → {"temperature": 25.5}
 * - GET /api/time → {"time": "2024-01-15T14:30:00", "timestamp": 1705327800}
 * - GET /api/logs → [{"dia": "lunes", "hora": "14:30", "velocidad": 50}, ...]
 * - POST /api/terminal → {"command": "led y on"} → {"response": "LED amarillo encendido"}
 * 
 * Arquitectura:
 * - Solo API REST, sin servir archivos estáticos
 * - Respuestas en formato JSON
 * - Sin sistema de archivos SPIFFS para frontend
 * - Sin autenticación (se maneja en Flask/Raspberry Pi)
 * 
 * ============================================================================
 */

// ===== INCLUDES =====
// Headers locales
#include "api_server.h"
#include "gpio_driver.h"
#include "ntc_sensor.h"
#include "registros.h"
#include "terminal_commands.h"

// ESP-IDF
#include <esp_http_server.h>
#include <esp_log.h>
#include <cJSON.h>
#include <esp_sntp.h>
#include <time.h>

// FreeRTOS
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

// Estándar C
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

// ===== DEFINICIONES Y CONSTANTES =====
static const char *TAG = "API_SERVER";

// ===== VARIABLES GLOBALES =====
static httpd_handle_t api_server = NULL;

// Colas para comandos de terminal
static QueueHandle_t gpio_command_queue = NULL;
static QueueHandle_t gpio_response_queue = NULL;
static SemaphoreHandle_t command_id_mutex = NULL;
static uint32_t command_id_counter = 0;

// ===== HANDLERS HTTP =====

/**
 * Handler GET /api/temperature
 * Devuelve temperatura actual del sensor NTC en formato JSON
 * Respuesta: {"temperature": 25.5} o {"error": "No data available"}
 */
static esp_err_t api_temperature_get_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "GET /api/temperature");
    
    ntc_data_t temp_data = ntc_get_current_temperature();
    
    cJSON *root = cJSON_CreateObject();
    
    if (temp_data.temperature_c < -900.0 || isnan(temp_data.temperature_c) || !isfinite(temp_data.temperature_c)) {
        cJSON_AddStringToObject(root, "error", "No data available");
    } else {
        cJSON_AddNumberToObject(root, "temperature", temp_data.temperature_c);
    }
    
    char *json_string = cJSON_Print(root);
    cJSON_Delete(root);
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
    httpd_resp_send(req, json_string, HTTPD_RESP_USE_STRLEN);
    free(json_string);
    
    return ESP_OK;
}

/**
 * Handler GET /api/time
 * Devuelve la hora actual del sistema en formato JSON
 * Respuesta: {"time": "2024-01-15T14:30:00", "timestamp": 1705327800}
 */
static esp_err_t api_time_get_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "GET /api/time");
    
    time_t now = time(NULL);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%dT%H:%M:%S", &timeinfo);
    
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "time", time_str);
    cJSON_AddNumberToObject(root, "timestamp", (double)now);
    
    char *json_string = cJSON_Print(root);
    cJSON_Delete(root);
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
    httpd_resp_send(req, json_string, HTTPD_RESP_USE_STRLEN);
    free(json_string);
    
    return ESP_OK;
}

/**
 * Handler GET /api/logs
 * Devuelve todos los registros de horarios en formato JSON
 * Respuesta: [{"dia": "lunes", "hora": "14:30", "velocidad": 50}, ...]
 */
static esp_err_t api_logs_get_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "GET /api/logs");
    
    char *json = leer_registros_json();
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json);
    free(json);
    
    return ESP_OK;
}

/**
 * Handler POST /api/terminal
 * Recibe un comando de terminal y retorna la respuesta
 * Body esperado: {"command": "led y on"}
 * Respuesta: {"response": "LED amarillo encendido"}
 */
static esp_err_t api_terminal_post_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "POST /api/terminal");
    
    char buf[256];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, "Error recibiendo datos", HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }
    buf[len] = '\0';
    
    // Parsear JSON
    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        ESP_LOGE(TAG, "Error parseando JSON");
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, "JSON inválido", HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }
    
    cJSON *command_item = cJSON_GetObjectItem(root, "command");
    if (!command_item || !cJSON_IsString(command_item)) {
        ESP_LOGE(TAG, "Campo 'command' faltante o inválido");
        cJSON_Delete(root);
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, "Campo 'command' requerido (string)", HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }
    
    const char *cmd = command_item->valuestring;
    
    // Procesar comando
    char response_text[256] = "[?] No se recibió ningún comando.";
    
    if (strlen(cmd) > 0) {
        // Convertir a minúsculas
        char cmd_lower[100];
        strncpy(cmd_lower, cmd, sizeof(cmd_lower) - 1);
        cmd_lower[sizeof(cmd_lower) - 1] = '\0';
        for (char *p = cmd_lower; *p; p++) {
            if (*p >= 'A' && *p <= 'Z') *p = *p - 'A' + 'a';
        }
        
        // Si es "clear", no necesita procesamiento
        if (strcmp(cmd_lower, "clear") == 0) {
            strncpy(response_text, "[OK] Pantalla limpiada.", sizeof(response_text) - 1);
        } else {
            // Enviar comando a la cola (si está disponible)
            if (gpio_command_queue != NULL && command_id_mutex != NULL) {
                uint32_t cmd_id = 0;
                if (xSemaphoreTake(command_id_mutex, portMAX_DELAY) == pdTRUE) {
                    cmd_id = ++command_id_counter;
                    xSemaphoreGive(command_id_mutex);
                }
                
                gpio_command_t command;
                command.command_id = cmd_id;
                strncpy(command.command, cmd_lower, sizeof(command.command) - 1);
                command.command[sizeof(command.command) - 1] = '\0';
                
                if (xQueueSend(gpio_command_queue, &command, pdMS_TO_TICKS(1000)) == pdTRUE) {
                    // Esperar respuesta
                    gpio_command_t response;
                    bool response_received = false;
                    
                    for (int attempts = 0; attempts < 10 && !response_received; attempts++) {
                        if (gpio_response_queue != NULL && 
                            xQueueReceive(gpio_response_queue, &response, pdMS_TO_TICKS(500)) == pdTRUE) {
                            if (response.command_id == cmd_id) {
                                strncpy(response_text, response.response, sizeof(response_text) - 1);
                                response_text[sizeof(response_text) - 1] = '\0';
                                response_received = true;
                                break;
                            } else {
                                xQueueSendToFront(gpio_response_queue, &response, 0);
                            }
                        }
                    }
                    
                    if (!response_received) {
                        strncpy(response_text, "[ERROR] Timeout esperando respuesta.", sizeof(response_text) - 1);
                    }
                } else {
                    strncpy(response_text, "[ERROR] Sistema ocupado.", sizeof(response_text) - 1);
                }
            }
        }
    }
    
    cJSON_Delete(root);
    
    // Construir respuesta JSON
    cJSON *response_json = cJSON_CreateObject();
    cJSON_AddStringToObject(response_json, "response", response_text);
    
    char *json_string = cJSON_Print(response_json);
    cJSON_Delete(response_json);
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_string, HTTPD_RESP_USE_STRLEN);
    free(json_string);
    
    return ESP_OK;
}

// ===== INICIALIZACIÓN =====

/**
 * Tarea wrapper para comandos de terminal
 */
static void gpio_command_task_wrapper(void *pvParameters) {
    struct {
        QueueHandle_t gpio_command_queue;
        QueueHandle_t gpio_response_queue;
    } terminal_ctx = {
        .gpio_command_queue = gpio_command_queue,
        .gpio_response_queue = gpio_response_queue
    };
    
    terminal_command_task(&terminal_ctx);
}

/**
 * Inicia el servidor API REST
 * Configura e inicia el servidor HTTP con endpoints REST únicamente
 * No monta SPIFFS ni sirve archivos estáticos
 */
void start_api_server(void) {
    ESP_LOGI(TAG, "Iniciando servidor API REST...");
    
    // Crear colas para comandos de terminal
    gpio_command_queue = xQueueCreate(10, sizeof(gpio_command_t));
    gpio_response_queue = xQueueCreate(10, sizeof(gpio_command_t));
    command_id_mutex = xSemaphoreCreateMutex();
    
    if (gpio_command_queue == NULL || gpio_response_queue == NULL || command_id_mutex == NULL) {
        ESP_LOGE(TAG, "Error al crear colas/mutex para comandos");
        return;
    }
    
    // Crear tarea de procesamiento de comandos
    xTaskCreate(gpio_command_task_wrapper, "gpio_cmd_task", 4096, NULL, 5, NULL);
    
    // Inicializar sistema de registros (usa SPIFFS solo para datos, no para frontend)
    crear_archivo_si_no_existe();
    
    // Configurar servidor HTTP
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 8;
    config.max_open_sockets = 5;
    config.lru_purge_enable = true;
    
    esp_err_t ret = httpd_start(&api_server, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error al iniciar servidor HTTP: %s", esp_err_to_name(ret));
        return;
    }
    
    // Registrar rutas API REST
    httpd_uri_t temperature_uri = {
        .uri = "/api/temperature",
        .method = HTTP_GET,
        .handler = api_temperature_get_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(api_server, &temperature_uri);
    
    httpd_uri_t time_uri = {
        .uri = "/api/time",
        .method = HTTP_GET,
        .handler = api_time_get_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(api_server, &time_uri);
    
    httpd_uri_t logs_uri = {
        .uri = "/api/logs",
        .method = HTTP_GET,
        .handler = api_logs_get_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(api_server, &logs_uri);
    
    httpd_uri_t terminal_uri = {
        .uri = "/api/terminal",
        .method = HTTP_POST,
        .handler = api_terminal_post_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(api_server, &terminal_uri);
    
    ESP_LOGI(TAG, "Servidor API REST iniciado correctamente en puerto %d", config.server_port);
    ESP_LOGI(TAG, "Endpoints disponibles:");
    ESP_LOGI(TAG, "  GET  /api/temperature");
    ESP_LOGI(TAG, "  GET  /api/time");
    ESP_LOGI(TAG, "  GET  /api/logs");
    ESP_LOGI(TAG, "  POST /api/terminal");
}
