/**
 * ============================================================================
 * ARCHIVO: web_server.c
 * ============================================================================
 * 
 * RESUMEN:
 * Implementación del servidor web HTTP del ESP32. Este módulo proporciona:
 * 
 * 1. Sistema de archivos SPIFFS:
 *    - Monta partición SPIFFS para servir archivos estáticos (HTML, CSS, JS)
 *    - Verifica que todos los archivos necesarios estén presentes
 * 
 * 2. Servidor HTTP con múltiples rutas:
 *    - Páginas web: login, dashboard, terminal, slider
 *    - API REST: /cmd (comandos), /temperature (datos JSON)
 *    - Autenticación: /login, /logout
 * 
 * 3. Sistema de autenticación y sesiones:
 *    - Login con usuario/contraseña (root/matrix123)
 *    - Gestión de sesiones basada en IP
 *    - Timeout automático después de 3 minutos de inactividad
 * 
 * 4. Procesamiento de comandos:
 *    - Sistema de colas para comandos GPIO (thread-safe)
 *    - Tarea dedicada para procesar comandos
 *    - Soporte para comandos: led y on/off, led b on/off, status, help, clear
 * 
 * 5. Gestión de temperatura:
 *    - Endpoint JSON para obtener temperatura actual
 *    - Datos actualizados desde la tarea de lectura del sensor
 * ============================================================================
 */

// ===== INCLUDES =====
#include "web_server.h"
#include "gpio_driver.h"
#include "ntc_sensor.h"
#include <esp_http_server.h>
#include <esp_spiffs.h>
#include <esp_log.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"

// ===== DEFINICIONES Y ESTRUCTURAS =====
static const char *TAG = "WEB_SERVER";

// ===== SECCIÓN: SISTEMA DE COLAS PARA COMANDOS =====
// Estructura para comandos GPIO
typedef struct {
    uint32_t command_id;  // ID único para emparejar comando-respuesta
    char command[100];
    char response[512];
} gpio_command_t;

// ===== SECCIÓN: GESTIÓN DE SESIONES =====
// Sistema de autenticación simple basado en IP
#define MAX_SESSIONS 5                    // Máximo número de sesiones simultáneas
#define SESSION_TIMEOUT_MS (3 * 60 * 1000) // Timeout: 3 minutos de inactividad
#define VALID_USER "root"                 // Usuario válido para login
#define VALID_PASS "matrix123"            // Contraseña válida para login

typedef struct {
    char ip[16];
    int64_t last_activity;
    bool authenticated;
} session_t;

// ===== ESTRUCTURA DE CONTEXTO: Encapsula el estado del servidor =====
// Esta estructura contiene todo el estado del servidor web que antes estaba en variables globales.
// El contexto se pasa a través de user_ctx en los handlers HTTP y como parámetro a las tareas.
// Esto elimina la necesidad de variables globales y mejora la modularidad del código.
typedef struct {
    httpd_handle_t server;
    QueueHandle_t gpio_command_queue;
    QueueHandle_t gpio_response_queue;
    SemaphoreHandle_t session_mutex;
    uint32_t command_id_counter;
    SemaphoreHandle_t command_id_mutex;
    session_t sessions[MAX_SESSIONS];
} webserver_context_t;

// Obtener tiempo actual en milisegundos
static int64_t get_time_ms(void) {
    return (int64_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

// ===== SECCIÓN: TAREA DE PROCESAMIENTO DE COMANDOS =====
/**
 * Tarea de FreeRTOS que procesa comandos GPIO recibidos desde la web
 * Lee comandos de la cola, los ejecuta y envía respuestas
 * 
 * Comandos soportados:
 * - led y on/off : Control LED amarillo
 * - led b on/off : Control LED azul
 * - led all on/off : Control ambos LEDs
 * - status : Estado de los LEDs
 * - help : Lista de comandos disponibles
 * - clear : Limpiar pantalla (manejado en frontend)
 */
static void gpio_command_task(void *pvParameters) {
    webserver_context_t *ctx = (webserver_context_t *)pvParameters;
    gpio_command_t cmd;
    ESP_LOGI(TAG, "Tarea de procesamiento de comandos GPIO iniciada");
    
    while (1) {
        // Esperar comando de la cola (bloqueante)
        if (xQueueReceive(ctx->gpio_command_queue, &cmd, portMAX_DELAY) == pdTRUE) {
            // Procesar comando (mantener el command_id para la respuesta)
            if (strcmp(cmd.command, "led y on") == 0) {
                gpio_set_yellow(true);
                strcpy(cmd.response, "[OK] LED amarillo encendido.");
            } else if (strcmp(cmd.command, "led y off") == 0) {
                gpio_set_yellow(false);
                strcpy(cmd.response, "[OK] LED amarillo apagado.");
            } else if (strcmp(cmd.command, "led b on") == 0) {
                gpio_set_blue(true);
                strcpy(cmd.response, "[OK] LED azul encendido.");
            } else if (strcmp(cmd.command, "led b off") == 0) {
                gpio_set_blue(false);
                strcpy(cmd.response, "[OK] LED azul apagado.");
            } else if (strcmp(cmd.command, "led all on") == 0) {
                gpio_set_yellow(true);
                gpio_set_blue(true);
                strcpy(cmd.response, "[OK] Ambos LEDs encendidos.");
            } else if (strcmp(cmd.command, "led all off") == 0) {
                gpio_set_yellow(false);
                gpio_set_blue(false);
                strcpy(cmd.response, "[OK] Ambos LEDs apagados.");
            } else if (strcmp(cmd.command, "status") == 0) {
                const char *estadoAmarillo = gpio_get_yellow() ? "ON" : "OFF";
                const char *estadoAzul = gpio_get_blue() ? "ON" : "OFF";
                snprintf(cmd.response, sizeof(cmd.response), 
                         "Estado de los LEDs:\n  - Amarillo: %s\n  - Azul:     %s",
                         estadoAmarillo, estadoAzul);
            } else if (strcmp(cmd.command, "help") == 0) {
                strcpy(cmd.response, 
                       "Comandos disponibles:\n\n"
                       "  --- Control Individual ---\n"
                       "  led y on          - Enciende el LED amarillo.\n"
                       "  led y off         - Apaga el LED amarillo.\n"
                       "  led b on          - Enciende el LED azul.\n"
                       "  led b off         - Apaga el LED azul.\n\n"
                       "  --- Control General ---\n"
                       "  led all on        - Enciende ambos LEDs.\n"
                       "  led all off       - Apaga ambos LEDs.\n\n"
                       "  --- Sistema ---\n"
                       "  status            - Muestra el estado de los LEDs.\n"
                       "  help              - Muestra esta lista.\n"
                       "  clear             - Limpia la pantalla.");
            } else {
                snprintf(cmd.response, sizeof(cmd.response), 
                         "[?] Comando no reconocido: '%s'. Escribe 'help' para ver la lista.", cmd.command);
            }
            
            // Enviar respuesta a la cola de respuestas (mantener el command_id)
            if (xQueueSend(ctx->gpio_response_queue, &cmd, pdMS_TO_TICKS(100)) != pdTRUE) {
                ESP_LOGW(TAG, "Error al enviar respuesta a la cola");
            }
        }
    }
}

// ===== SECCIÓN: TAREA DE GESTIÓN DE SESIONES =====
/**
 * Tarea que verifica periódicamente las sesiones y expira las inactivas
 * Apaga los LEDs cuando una sesión expira por timeout
 */
static void session_management_task(void *pvParameters) {
    webserver_context_t *ctx = (webserver_context_t *)pvParameters;
    ESP_LOGI(TAG, "Tarea de gestión de sesiones iniciada");
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(2000)); // Verificar cada 2 segundos
        
        int64_t now = get_time_ms();
        
        // Proteger acceso a sesiones con mutex
        if (ctx->session_mutex != NULL && xSemaphoreTake(ctx->session_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            for (int i = 0; i < MAX_SESSIONS; i++) {
                if (ctx->sessions[i].authenticated && 
                    (now - ctx->sessions[i].last_activity > SESSION_TIMEOUT_MS)) {
                    ctx->sessions[i].authenticated = false;
                    gpio_set_yellow(false);
                    gpio_set_blue(false);
                    ESP_LOGI(TAG, "Sesión expirada para IP %s. LEDs apagados.", ctx->sessions[i].ip);
                }
            }
            xSemaphoreGive(ctx->session_mutex);
        }
    }
}

// Obtener IP del cliente desde la request
static void get_client_ip(httpd_req_t *req, char *ip_str, size_t len) {
    // Obtener la dirección remota usando la API de ESP-IDF
    struct sockaddr_in *addr = (struct sockaddr_in *)req->sess_ctx;
    if (addr && addr->sin_family == AF_INET) {
        inet_ntoa_r(addr->sin_addr, ip_str, len);
        return;
    }
    
    // Fallback: usar una IP genérica si no se puede obtener
    // En un entorno SoftAP, todas las conexiones vienen de la misma red
    // Por simplicidad, usamos una IP única basada en el puntero de la request
    snprintf(ip_str, len, "192.168.4.%d", (int)((uintptr_t)req % 255) + 1);
}

// Buscar o crear sesión para una IP
static session_t* find_or_create_session(webserver_context_t *ctx, const char *ip) {
    int64_t now = get_time_ms();
    session_t *session = NULL;
    
    // Proteger acceso con mutex
    if (ctx->session_mutex != NULL && xSemaphoreTake(ctx->session_mutex, portMAX_DELAY) == pdTRUE) {
        // Buscar sesión existente
        for (int i = 0; i < MAX_SESSIONS; i++) {
            if (strcmp(ctx->sessions[i].ip, ip) == 0) {
                // Verificar timeout
                if (now - ctx->sessions[i].last_activity > SESSION_TIMEOUT_MS) {
                    ctx->sessions[i].authenticated = false;
                    ESP_LOGI(TAG, "Sesión expirada para IP %s", ip);
                }
                session = &ctx->sessions[i];
                break;
            }
        }
        
        // Si no se encontró, buscar slot libre
        if (session == NULL) {
            for (int i = 0; i < MAX_SESSIONS; i++) {
                if (!ctx->sessions[i].authenticated || (now - ctx->sessions[i].last_activity > SESSION_TIMEOUT_MS)) {
                    strncpy(ctx->sessions[i].ip, ip, sizeof(ctx->sessions[i].ip) - 1);
                    ctx->sessions[i].ip[sizeof(ctx->sessions[i].ip) - 1] = '\0';
                    ctx->sessions[i].last_activity = now;
                    ctx->sessions[i].authenticated = false;
                    session = &ctx->sessions[i];
                    break;
                }
            }
        }
        
        xSemaphoreGive(ctx->session_mutex);
    }
    
    return session; // NULL si no hay slots disponibles
}

static bool is_authenticated(webserver_context_t *ctx, httpd_req_t *req) {
    char ip[16] = {0};
    get_client_ip(req, ip, sizeof(ip));
    
    session_t *session = find_or_create_session(ctx, ip);
    if (!session) {
        return false;
    }
    
    // Proteger acceso con mutex
    bool authenticated = false;
    if (ctx->session_mutex != NULL && xSemaphoreTake(ctx->session_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (!session->authenticated) {
            authenticated = false;
        } else {
            // Actualizar última actividad
            int64_t now = get_time_ms();
            if (now - session->last_activity > SESSION_TIMEOUT_MS) {
                session->authenticated = false;
                gpio_set_yellow(false);
                gpio_set_blue(false);
                ESP_LOGI(TAG, "Sesión expirada para IP %s. LEDs apagados.", ip);
                authenticated = false;
            } else {
                session->last_activity = now;
                authenticated = true;
            }
        }
        xSemaphoreGive(ctx->session_mutex);
    }
    
    return authenticated;
}

// ===== SECCIÓN: UTILIDADES =====
/**
 * Función auxiliar para servir archivos estáticos desde SPIFFS
 * Lee el archivo en chunks y lo envía al cliente HTTP
 * 
 * @param req: Request HTTP
 * @param filepath: Ruta del archivo en SPIFFS (ej: "/spiffs/index.html")
 * @param type: Content-Type HTTP (ej: "text/html", "text/css")
 * @return ESP_OK si éxito, ESP_FAIL si error
 */
esp_err_t send_file_from_spiffs(httpd_req_t *req, const char *filepath, const char *type) {
    ESP_LOGI(TAG, "Intentando abrir archivo: %s", filepath);
    FILE *fd = fopen(filepath, "r");
    if (!fd) {
        ESP_LOGE(TAG, "No se encuentra el archivo: %s (errno: %d)", filepath, errno);
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, type);
    char chunk[1024];
    size_t chunksize;
    size_t total_sent = 0;
    while ((chunksize = fread(chunk, 1, sizeof(chunk), fd)) > 0) {
        if (httpd_resp_send_chunk(req, chunk, chunksize) != ESP_OK) {
            ESP_LOGE(TAG, "Error al enviar chunk del archivo: %s", filepath);
            fclose(fd);
            return ESP_FAIL;
        }
        total_sent += chunksize;
    }
    fclose(fd);
    httpd_resp_send_chunk(req, NULL, 0); // Finalizar respuesta
    ESP_LOGI(TAG, "Archivo enviado exitosamente: %s (%d bytes)", filepath, total_sent);
    return ESP_OK;
}

// ===== SECCIÓN: HANDLERS HTTP =====

/**
 * Handler para GET / (raíz)
 * Si el usuario está autenticado, redirige al dashboard
 * Si no, muestra la página de login
 */
static esp_err_t root_get_handler(httpd_req_t *req) {
    webserver_context_t *ctx = (webserver_context_t *)req->user_ctx;
    ESP_LOGI(TAG, "GET / - URI: %s", req->uri);
    esp_err_t ret;
    if (is_authenticated(ctx, req)) {
        // Redirigir al dashboard si está autenticado
        ESP_LOGI(TAG, "Usuario autenticado, redirigiendo a /dashboard");
        httpd_resp_set_hdr(req, "Location", "/dashboard");
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    } else {
        ESP_LOGI(TAG, "Usuario no autenticado, sirviendo login.html");
        ret = send_file_from_spiffs(req, "/spiffs/login.html", "text/html");
    }
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error al servir archivo HTML desde SPIFFS");
        // Enviar una respuesta de error básica
        httpd_resp_set_type(req, "text/html");
        httpd_resp_send(req, "<html><body><h1>Error: No se pudo cargar la página</h1></body></html>", HTTPD_RESP_USE_STRLEN);
    }
    
    return ret;
}

// GET /style.css
static esp_err_t style_get_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "GET /style.css");
    return send_file_from_spiffs(req, "/spiffs/style.css", "text/css");
}

// GET /script.js
static esp_err_t script_get_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "GET /script.js");
    return send_file_from_spiffs(req, "/spiffs/script.js", "application/javascript");
}

// GET /dashboard
static esp_err_t dashboard_get_handler(httpd_req_t *req) {
    webserver_context_t *ctx = (webserver_context_t *)req->user_ctx;
    ESP_LOGI(TAG, "GET /dashboard");
    if (!is_authenticated(ctx, req)) {
        httpd_resp_set_hdr(req, "Location", "/");
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }
    return send_file_from_spiffs(req, "/spiffs/dashboard.html", "text/html");
}

// GET /terminal
static esp_err_t terminal_get_handler(httpd_req_t *req) {
    webserver_context_t *ctx = (webserver_context_t *)req->user_ctx;
    ESP_LOGI(TAG, "GET /terminal");
    if (!is_authenticated(ctx, req)) {
        httpd_resp_set_hdr(req, "Location", "/");
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }
    return send_file_from_spiffs(req, "/spiffs/index.html", "text/html");
}

// GET /slider
static esp_err_t slider_get_handler(httpd_req_t *req) {
    webserver_context_t *ctx = (webserver_context_t *)req->user_ctx;
    ESP_LOGI(TAG, "GET /slider");
    if (!is_authenticated(ctx, req)) {
        httpd_resp_set_hdr(req, "Location", "/");
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }
    return send_file_from_spiffs(req, "/spiffs/slider.html", "text/html");
}

// GET /favicon.ico (evitar 404)
static esp_err_t favicon_get_handler(httpd_req_t *req) {
    httpd_resp_set_status(req, "204 No Content");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

/**
 * Handler para GET /temperature
 * Devuelve la temperatura actual en formato JSON
 * Respuesta: {"temperature": 25.5} o {"error": "No data available"}
 */
static esp_err_t temperature_get_handler(httpd_req_t *req) {
    webserver_context_t *ctx = (webserver_context_t *)req->user_ctx;
    if (!is_authenticated(ctx, req)) {
        httpd_resp_set_status(req, "401 Unauthorized");
        httpd_resp_send(req, "Unauthorized", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    // Obtener la temperatura actual almacenada por la tarea
    ntc_data_t temp_data = ntc_get_current_temperature();
    char response[128];
    int len;
    
    // Enviar la temperatura si es válida (no es -999.0)
    if (temp_data.temperature_c < -900.0 || isnan(temp_data.temperature_c) || !isfinite(temp_data.temperature_c)) {
        // Error en la lectura o datos no disponibles aún
        len = snprintf(response, sizeof(response), "{\"error\":\"No data available\"}");
    } else {
        // Enviar la temperatura siempre que sea un número válido
        // Usar formato más explícito para asegurar que sea JSON válido
        len = snprintf(response, sizeof(response), "{\"temperature\":%.1f}", temp_data.temperature_c);
        
        // Verificar que el JSON se formateó correctamente
        if (len >= sizeof(response)) {
            ESP_LOGE(TAG, "Buffer de respuesta demasiado pequeño!");
            len = snprintf(response, sizeof(response), "{\"error\":\"Buffer overflow\"}");
        }
    }
    
    // Configurar headers antes de enviar
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
    httpd_resp_set_hdr(req, "Pragma", "no-cache");
    httpd_resp_set_hdr(req, "Expires", "0");
    
    esp_err_t ret = httpd_resp_send(req, response, len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error al enviar respuesta de temperatura: %s", esp_err_to_name(ret));
        return ret;
    }
    
    return ESP_OK;
}

// Decodificar URL (simple, solo espacios)
static void url_decode(char *str) {
    char *src = str;
    char *dst = str;
    while (*src) {
        if (*src == '%' && src[1] && src[2]) {
            // Decodificar %20 a espacio
            if (src[1] == '2' && src[2] == '0') {
                *dst++ = ' ';
                src += 3;
            } else {
                *dst++ = *src++;
            }
        } else if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

/**
 * Handler para GET /cmd?c=comando
 * Procesa comandos desde la terminal web
 * Envía el comando a la cola y espera la respuesta
 */
static esp_err_t cmd_get_handler(httpd_req_t *req) {
    webserver_context_t *ctx = (webserver_context_t *)req->user_ctx;
    char ip[16] = {0};
    get_client_ip(req, ip, sizeof(ip));
    
    if (!is_authenticated(ctx, req)) {
        httpd_resp_set_status(req, "401 Unauthorized");
        httpd_resp_send(req, "[ERROR] No estás autenticado o la sesión ha expirado.", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    char buf[200];
    size_t buf_len = httpd_req_get_url_query_len(req) + 1;
    
    if (buf_len > 1 && buf_len < sizeof(buf)) {
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            char cmd[100];
            if (httpd_query_key_value(buf, "c", cmd, sizeof(cmd)) == ESP_OK) {
                url_decode(cmd);
                
                // Convertir a minúsculas
                for (int i = 0; cmd[i]; i++) {
                    if (cmd[i] >= 'A' && cmd[i] <= 'Z') {
                        cmd[i] = cmd[i] - 'A' + 'a';
                    }
                }
                
                // Si es "clear", no necesita procesamiento en el backend
                if (strcmp(cmd, "clear") == 0) {
                    httpd_resp_send(req, "[OK] Pantalla limpiada.", HTTPD_RESP_USE_STRLEN);
                    return ESP_OK;
                }
                
                // Obtener ID único para el comando
                uint32_t cmd_id = 0;
                if (ctx->command_id_mutex != NULL && xSemaphoreTake(ctx->command_id_mutex, portMAX_DELAY) == pdTRUE) {
                    cmd_id = ++ctx->command_id_counter;
                    xSemaphoreGive(ctx->command_id_mutex);
                }
                
                // Enviar comando a la cola para procesamiento por la tarea
                gpio_command_t command;
                command.command_id = cmd_id;
                strncpy(command.command, cmd, sizeof(command.command) - 1);
                command.command[sizeof(command.command) - 1] = '\0';
                
                if (ctx->gpio_command_queue != NULL && 
                    xQueueSend(ctx->gpio_command_queue, &command, pdMS_TO_TICKS(1000)) == pdTRUE) {
                    // Esperar respuesta de la tarea (con timeout)
                    gpio_command_t response;
                    bool response_received = false;
                    
                    // Intentar recibir respuesta hasta 3 veces (para manejar respuestas de otros comandos)
                    for (int attempts = 0; attempts < 10 && !response_received; attempts++) {
                        if (ctx->gpio_response_queue != NULL && 
                            xQueueReceive(ctx->gpio_response_queue, &response, pdMS_TO_TICKS(500)) == pdTRUE) {
                            // Verificar que la respuesta corresponde a nuestro comando
                            if (response.command_id == cmd_id) {
                                httpd_resp_send(req, response.response, HTTPD_RESP_USE_STRLEN);
                                response_received = true;
                                return ESP_OK;
                            } else {
                                // Respuesta de otro comando, devolverla a la cola
                                xQueueSendToFront(ctx->gpio_response_queue, &response, 0);
                            }
                        }
                    }
                    
                    // Timeout o error al recibir respuesta
                    httpd_resp_send(req, "[ERROR] Timeout esperando respuesta del sistema.", HTTPD_RESP_USE_STRLEN);
                    return ESP_OK;
                } else {
                    // Error al enviar a la cola
                    httpd_resp_send(req, "[ERROR] Sistema ocupado. Intenta de nuevo.", HTTPD_RESP_USE_STRLEN);
                    return ESP_OK;
                }
            }
        }
    }
    httpd_resp_send(req, "[?] No se recibió ningún comando.", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/**
 * Handler para POST /login
 * Valida credenciales y crea una sesión autenticada
 * Parámetros: user=xxx&pass=yyy (form-urlencoded)
 */
static esp_err_t login_post_handler(httpd_req_t *req) {
    webserver_context_t *ctx = (webserver_context_t *)req->user_ctx;
    char content[200];
    size_t recv_size = sizeof(content) - 1;
    
    int ret = httpd_req_recv(req, content, recv_size);
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    content[ret] = '\0';
    
    // Parsear user=xxx&pass=yyy
    char user[50] = {0};
    char pass[50] = {0};
    
    char *user_start = strstr(content, "user=");
    char *pass_start = strstr(content, "pass=");
    
    if (user_start) {
        user_start += 5; // Saltar "user="
        char *user_end = strchr(user_start, '&');
        if (user_end) {
            size_t user_len = user_end - user_start;
            if (user_len < sizeof(user)) {
                strncpy(user, user_start, user_len);
                user[user_len] = '\0';
            }
        } else {
            strncpy(user, user_start, sizeof(user) - 1);
        }
        url_decode(user);
    }
    
    if (pass_start) {
        pass_start += 5; // Saltar "pass="
        strncpy(pass, pass_start, sizeof(pass) - 1);
        url_decode(pass);
    }
    
    // Validar credenciales
    if (strcmp(user, VALID_USER) == 0 && strcmp(pass, VALID_PASS) == 0) {
        char ip[16] = {0};
        get_client_ip(req, ip, sizeof(ip));
        
        session_t *session = find_or_create_session(ctx, ip);
        if (session) {
            // Proteger acceso con mutex
            if (ctx->session_mutex != NULL && xSemaphoreTake(ctx->session_mutex, portMAX_DELAY) == pdTRUE) {
                session->authenticated = true;
                session->last_activity = get_time_ms();
                xSemaphoreGive(ctx->session_mutex);
            }
            ESP_LOGI(TAG, "Login exitoso desde IP: %s", ip);
            httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        }
    }
    
    ESP_LOGW(TAG, "Intento de login fallido. User: %s", user);
    httpd_resp_set_status(req, "401 Unauthorized");
    httpd_resp_send(req, "[ERROR] Usuario o contraseña incorrectos.", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// GET /logout
static esp_err_t logout_get_handler(httpd_req_t *req) {
    webserver_context_t *ctx = (webserver_context_t *)req->user_ctx;
    char ip[16] = {0};
    get_client_ip(req, ip, sizeof(ip));
    
    session_t *session = find_or_create_session(ctx, ip);
    if (session) {
        // Proteger acceso con mutex
        if (ctx->session_mutex != NULL && xSemaphoreTake(ctx->session_mutex, portMAX_DELAY) == pdTRUE) {
            session->authenticated = false;
            xSemaphoreGive(ctx->session_mutex);
        }
    }
    
    gpio_set_yellow(false);
    gpio_set_blue(false);
    
    ESP_LOGI(TAG, "Logout desde IP: %s. LEDs apagados.", ip);
    
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

// Handler catch-all para debugging (debe registrarse al final)
static esp_err_t catch_all_handler(httpd_req_t *req) {
    ESP_LOGW(TAG, "URI no manejada: %s (método: %d)", req->uri, req->method);
    httpd_resp_send_404(req);
    return ESP_FAIL;
}

// ===== SECCIÓN: INICIALIZACIÓN DEL SERVIDOR =====
/**
 * Función principal que inicia el servidor web
 * 
 * Proceso de inicialización:
 * 1. Crea estructura de contexto para encapsular el estado (sin variables globales)
 * 2. Crea mutex y colas para comandos y sesiones
 * 3. Crea tareas para procesamiento de comandos y gestión de sesiones (pasa contexto)
 * 4. Monta el sistema de archivos SPIFFS
 * 5. Verifica que todos los archivos necesarios estén presentes
 * 6. Configura e inicia el servidor HTTP
 * 7. Registra todas las rutas y handlers (pasa contexto a través de user_ctx)
 */
void start_webserver(void) {
    // Crear estructura de contexto estática para almacenar el estado del servidor
    // Esta estructura encapsula todo el estado que antes estaba en variables globales
    static webserver_context_t ctx = {0};
    
    // 0. Inicializar sesiones
    memset(ctx.sessions, 0, sizeof(ctx.sessions));
    
    // 0.1. Crear mutex para sesiones
    ctx.session_mutex = xSemaphoreCreateMutex();
    if (ctx.session_mutex == NULL) {
        ESP_LOGE(TAG, "Error al crear mutex para sesiones");
        return;
    }
    
    // 0.2. Crear mutex para IDs de comandos
    ctx.command_id_mutex = xSemaphoreCreateMutex();
    if (ctx.command_id_mutex == NULL) {
        ESP_LOGE(TAG, "Error al crear mutex para IDs de comandos");
        return;
    }
    
    // 0.3. Crear colas para comandos GPIO
    ctx.gpio_command_queue = xQueueCreate(10, sizeof(gpio_command_t));
    ctx.gpio_response_queue = xQueueCreate(10, sizeof(gpio_command_t));
    if (ctx.gpio_command_queue == NULL || ctx.gpio_response_queue == NULL) {
        ESP_LOGE(TAG, "Error al crear colas para comandos GPIO");
        return;
    }
    ESP_LOGI(TAG, "Colas de comandos GPIO creadas");
    
    // 0.4. Crear tarea de procesamiento de comandos GPIO (pasar contexto)
    xTaskCreate(gpio_command_task, "gpio_cmd_task", 4096, &ctx, 5, NULL);
    ESP_LOGI(TAG, "Tarea de comandos GPIO creada");
    
    // 0.5. Crear tarea de gestión de sesiones (pasar contexto)
    xTaskCreate(session_management_task, "session_mgmt", 2048, &ctx, 3, NULL);
    ESP_LOGI(TAG, "Tarea de gestión de sesiones creada");
    
    // 1. Iniciar SPIFFS
    // Primero intentar montar sin formatear
    esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = "storage",
      .max_files = 5,
      .format_if_mount_failed = false  // NO formatear automáticamente
    };
    
    ESP_LOGI(TAG, "Inicializando SPIFFS desde partición 'storage'...");
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    
    // Si falla el montaje, intentar formatear solo si es necesario
    if (ret == ESP_FAIL) {
        ESP_LOGW(TAG, "SPIFFS no se pudo montar, intentando formatear...");
        conf.format_if_mount_failed = true;
        ret = esp_vfs_spiffs_register(&conf);
        if (ret == ESP_OK) {
            ESP_LOGW(TAG, "SPIFFS formateado - los archivos deben ser flasheados nuevamente");
        }
    }
    
    if (ret != ESP_OK) {
        if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Partición SPIFFS 'storage' no encontrada");
            ESP_LOGE(TAG, "Verifica que la tabla de particiones incluya una partición 'storage' tipo spiffs");
        } else if (ret == ESP_ERR_INVALID_STATE) {
            ESP_LOGE(TAG, "SPIFFS ya está montado o no se puede desmontar");
        } else {
            ESP_LOGE(TAG, "Error al inicializar SPIFFS: %s (%d)", esp_err_to_name(ret), ret);
        }
        return;
    }
    ESP_LOGI(TAG, "SPIFFS montado correctamente en /spiffs");
    
    // Obtener información de SPIFFS
    size_t total = 0, used = 0;
    ret = esp_spiffs_info("storage", &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error al obtener información de SPIFFS (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "SPIFFS: %d KB total, %d KB usado (%.1f%%)", 
                 total / 1024, used / 1024, (used * 100.0f) / total);
    }
    
    // Verificar que los archivos existan en SPIFFS
    ESP_LOGI(TAG, "Verificando archivos en SPIFFS...");
    const char *files_to_check[] = {"/spiffs/index.html", "/spiffs/login.html", 
                                    "/spiffs/dashboard.html", "/spiffs/slider.html", 
                                    "/spiffs/style.css", "/spiffs/script.js"};
    int files_found = 0;
    for (int i = 0; i < 6; i++) {
        FILE *f = fopen(files_to_check[i], "r");
        if (f) {
            // Obtener tamaño del archivo
            fseek(f, 0, SEEK_END);
            long size = ftell(f);
            fclose(f);
            ESP_LOGI(TAG, "✓ Archivo encontrado: %s (%ld bytes)", files_to_check[i], size);
            files_found++;
        } else {
            ESP_LOGE(TAG, "✗ Archivo NO encontrado: %s (errno: %d - %s)", 
                     files_to_check[i], errno, strerror(errno));
        }
    }
    
    if (files_found == 0) {
        ESP_LOGE(TAG, "==========================================");
        ESP_LOGE(TAG, "ERROR CRÍTICO: Ningún archivo encontrado en SPIFFS");
        ESP_LOGE(TAG, "Los archivos deben ser flasheados a la partición SPIFFS");
        ESP_LOGE(TAG, "Ejecuta: ./flash_all.sh [PORT] o idf.py flash");
        ESP_LOGE(TAG, "==========================================");
    } else if (files_found < 6) {
        ESP_LOGW(TAG, "Advertencia: Solo %d de 6 archivos encontrados", files_found);
    } else {
        ESP_LOGI(TAG, "✓ Todos los archivos encontrados correctamente (%d/6)", files_found);
    }

    // 2. Configurar Server
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 12;
    config.max_open_sockets = 7;
    config.lru_purge_enable = true; // Habilitar purga de conexiones inactivas

    esp_err_t httpd_ret = httpd_start(&ctx.server, &config);
    if (httpd_ret != ESP_OK) {
        ESP_LOGE(TAG, "Error al iniciar el servidor web: %s", esp_err_to_name(httpd_ret));
        return;
    }
    
    ESP_LOGI(TAG, "Servidor HTTP iniciado, registrando rutas...");
    
    // Registrar rutas (pasar contexto a través de user_ctx)
    httpd_uri_t root_uri = { 
        .uri = "/", 
        .method = HTTP_GET, 
        .handler = root_get_handler,
        .user_ctx = &ctx
    };
    if (httpd_register_uri_handler(ctx.server, &root_uri) != ESP_OK) {
        ESP_LOGE(TAG, "Error al registrar ruta /");
    } else {
        ESP_LOGI(TAG, "Ruta / registrada");
    }

    httpd_uri_t css_uri = { 
        .uri = "/style.css", 
        .method = HTTP_GET, 
        .handler = style_get_handler,
        .user_ctx = &ctx
    };
    if (httpd_register_uri_handler(ctx.server, &css_uri) != ESP_OK) {
        ESP_LOGE(TAG, "Error al registrar ruta /style.css");
    } else {
        ESP_LOGI(TAG, "Ruta /style.css registrada");
    }
    
    httpd_uri_t js_uri = { 
        .uri = "/script.js", 
        .method = HTTP_GET, 
        .handler = script_get_handler,
        .user_ctx = &ctx
    };
    if (httpd_register_uri_handler(ctx.server, &js_uri) != ESP_OK) {
        ESP_LOGE(TAG, "Error al registrar ruta /script.js");
    } else {
        ESP_LOGI(TAG, "Ruta /script.js registrada");
    }

    httpd_uri_t cmd_uri = { 
        .uri = "/cmd", 
        .method = HTTP_GET, 
        .handler = cmd_get_handler,
        .user_ctx = &ctx
    };
    if (httpd_register_uri_handler(ctx.server, &cmd_uri) != ESP_OK) {
        ESP_LOGE(TAG, "Error al registrar ruta /cmd");
    } else {
        ESP_LOGI(TAG, "Ruta /cmd registrada");
    }

    httpd_uri_t login_uri = { 
        .uri = "/login", 
        .method = HTTP_POST, 
        .handler = login_post_handler,
        .user_ctx = &ctx
    };
    if (httpd_register_uri_handler(ctx.server, &login_uri) != ESP_OK) {
        ESP_LOGE(TAG, "Error al registrar ruta /login");
    } else {
        ESP_LOGI(TAG, "Ruta /login registrada");
    }
    
    httpd_uri_t logout_uri = { 
        .uri = "/logout", 
        .method = HTTP_GET, 
        .handler = logout_get_handler,
        .user_ctx = &ctx
    };
    if (httpd_register_uri_handler(ctx.server, &logout_uri) != ESP_OK) {
        ESP_LOGE(TAG, "Error al registrar ruta /logout");
    } else {
        ESP_LOGI(TAG, "Ruta /logout registrada");
    }

    httpd_uri_t temperature_uri = { 
        .uri = "/temperature", 
        .method = HTTP_GET, 
        .handler = temperature_get_handler,
        .user_ctx = &ctx
    };
    if (httpd_register_uri_handler(ctx.server, &temperature_uri) != ESP_OK) {
        ESP_LOGE(TAG, "Error al registrar ruta /temperature");
    } else {
        ESP_LOGI(TAG, "Ruta /temperature registrada");
    }

    httpd_uri_t dashboard_uri = { 
        .uri = "/dashboard", 
        .method = HTTP_GET, 
        .handler = dashboard_get_handler,
        .user_ctx = &ctx
    };
    if (httpd_register_uri_handler(ctx.server, &dashboard_uri) != ESP_OK) {
        ESP_LOGE(TAG, "Error al registrar ruta /dashboard");
    } else {
        ESP_LOGI(TAG, "Ruta /dashboard registrada");
    }

    httpd_uri_t terminal_uri = { 
        .uri = "/terminal", 
        .method = HTTP_GET, 
        .handler = terminal_get_handler,
        .user_ctx = &ctx
    };
    if (httpd_register_uri_handler(ctx.server, &terminal_uri) != ESP_OK) {
        ESP_LOGE(TAG, "Error al registrar ruta /terminal");
    } else {
        ESP_LOGI(TAG, "Ruta /terminal registrada");
    }

    httpd_uri_t slider_uri = { 
        .uri = "/slider", 
        .method = HTTP_GET, 
        .handler = slider_get_handler,
        .user_ctx = &ctx
    };
    if (httpd_register_uri_handler(ctx.server, &slider_uri) != ESP_OK) {
        ESP_LOGE(TAG, "Error al registrar ruta /slider");
    } else {
        ESP_LOGI(TAG, "Ruta /slider registrada");
    }

    // Handler para favicon.ico (evitar 404)
    httpd_uri_t favicon_uri = {
        .uri = "/favicon.ico",
        .method = HTTP_GET,
        .handler = favicon_get_handler,
        .user_ctx = &ctx
    };
    if (httpd_register_uri_handler(ctx.server, &favicon_uri) != ESP_OK) {
        ESP_LOGW(TAG, "No se pudo registrar /favicon.ico (no crítico)");
    } else {
        ESP_LOGI(TAG, "Ruta /favicon.ico registrada");
    }
    
    // Registrar handler catch-all con wildcard (debe ser el último)
    // Nota: Para usar wildcard, necesitamos habilitar uri_match_fn
    // Por ahora, no lo registramos para evitar conflictos
    // httpd_uri_t catch_all_uri = {
    //     .uri = "/*",
    //     .method = HTTP_GET,
    //     .handler = catch_all_handler,
    //     .user_ctx = NULL
    // };
    
    ESP_LOGI(TAG, "Servidor Web Iniciado correctamente en puerto %d", config.server_port);
    ESP_LOGI(TAG, "Total de handlers registrados: 10 (/, /style.css, /script.js, /cmd, /login, /logout, /temperature, /dashboard, /terminal, /slider)");
}