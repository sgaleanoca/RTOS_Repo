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
 * 
 * ============================================================================
 * ÍNDICE DE SECCIONES:
 * ============================================================================
 * Sección 1: INCLUDES se encuentra en las líneas 34 a 62
 * Sección 2: DEFINICIONES Y CONSTANTES se encuentra en las líneas 64 a 76
 * Sección 3: ESTRUCTURAS DE DATOS se encuentra en las líneas 78 a 103
 * Sección 4: UTILIDADES GENERALES se encuentra en las líneas 105 a 112
 * Sección 5: TAREA DE PROCESAMIENTO DE COMANDOS se encuentra en las líneas 114 a 188
 * Sección 6: TAREA DE GESTIÓN DE SESIONES se encuentra en las líneas 190 a 218
 * Sección 7: FUNCIONES DE GESTIÓN DE SESIONES se encuentra en las líneas 220 a 318
 * Sección 8: UTILIDADES DE ARCHIVOS se encuentra en las líneas 323 a 358
 * Sección 9: HANDLERS HTTP se encuentra en las líneas 360 a 769
 * Sección 10: INICIALIZACIÓN DE SPIFFS se encuentra en las líneas 771 a 869
 * Sección 11: REGISTRO DE RUTAS HTTP se encuentra en las líneas 871 a 928
 * Sección 12: INICIALIZACIÓN DEL SERVIDOR se encuentra en las líneas 930 a 1010
 * ============================================================================
 */

// ===== INCLUDES =====
// Headers locales
#include "web_server.h"
#include "gpio_driver.h"
#include "ntc_sensor.h"

// ESP-IDF
#include <esp_http_server.h>
#include <esp_spiffs.h>
#include <esp_log.h>

// FreeRTOS
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

// LWIP (red)
#include "lwip/inet.h"
#include "lwip/sockets.h"

// Estándar C
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include <math.h>

// ===== DEFINICIONES Y CONSTANTES =====
static const char *TAG = "WEB_SERVER";

// Configuración de sesiones
#define MAX_SESSIONS 5                    // Máximo número de sesiones simultáneas
#define SESSION_TIMEOUT_MS (3 * 60 * 1000) // Timeout: 3 minutos de inactividad
#define VALID_USER "root"                 // Usuario válido para login
#define VALID_PASS "matrix123"            // Contraseña válida para login

// Configuración de colas
#define GPIO_QUEUE_SIZE 10                // Tamaño de las colas de comandos
#define CMD_RESPONSE_TIMEOUT_MS 500       // Timeout para recibir respuesta de comando
#define CMD_RESPONSE_MAX_ATTEMPTS 10      // Intentos máximos para recibir respuesta

// ===== ESTRUCTURAS DE DATOS =====
// Estructura para comandos GPIO
typedef struct {
    uint32_t command_id;  // ID único para emparejar comando-respuesta
    char command[100];
    char response[512];
} gpio_command_t;

// Estructura de sesión de usuario
typedef struct {
    char ip[16];
    int64_t last_activity;
    bool authenticated;
} session_t;

// Estructura de contexto del servidor web
// Encapsula todo el estado del servidor para evitar variables globales
typedef struct {
    httpd_handle_t server;
    QueueHandle_t gpio_command_queue;
    QueueHandle_t gpio_response_queue;
    SemaphoreHandle_t session_mutex;
    uint32_t command_id_counter;
    SemaphoreHandle_t command_id_mutex;
    session_t sessions[MAX_SESSIONS];
} webserver_context_t;

// ===== SECCIÓN: UTILIDADES GENERALES =====
/**
 * Obtiene el tiempo actual en milisegundos desde el inicio del sistema
 * @return Tiempo en milisegundos
 */
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

/**
 * Obtiene la dirección IP del cliente desde la request HTTP
 * @param req: Request HTTP
 * @param ip_str: Buffer donde se almacenará la IP
 * @param len: Tamaño del buffer
 */
static void get_client_ip(httpd_req_t *req, char *ip_str, size_t len) {
    struct sockaddr_in *addr = (struct sockaddr_in *)req->sess_ctx;
    if (addr && addr->sin_family == AF_INET) {
        inet_ntoa_r(addr->sin_addr, ip_str, len);
        return;
    }
    
    // Fallback: IP genérica basada en el puntero de la request
    // (útil en entornos SoftAP donde todas las conexiones vienen de la misma red)
    snprintf(ip_str, len, "192.168.4.%d", (int)((uintptr_t)req % 255) + 1);
}

/**
 * Busca una sesión existente o crea una nueva para una IP
 * @param ctx: Contexto del servidor web
 * @param ip: Dirección IP del cliente
 * @return Puntero a la sesión o NULL si no hay slots disponibles
 */
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

/**
 * Verifica si el cliente está autenticado y actualiza la última actividad
 * @param ctx: Contexto del servidor web
 * @param req: Request HTTP
 * @return true si está autenticado, false en caso contrario
 */
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

// ===== SECCIÓN: UTILIDADES DE ARCHIVOS =====
/**
 * Sirve un archivo estático desde SPIFFS al cliente HTTP
 * Lee el archivo en chunks y lo envía de forma eficiente
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

// --- Handlers de Páginas Web ---

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

/**
 * Handler para GET /dashboard
 * Requiere autenticación, redirige a login si no está autenticado
 */
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

/**
 * Handler para GET /terminal
 * Requiere autenticación, redirige a login si no está autenticado
 */
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

/**
 * Handler para GET /slider
 * Requiere autenticación, redirige a login si no está autenticado
 */
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

// --- Handlers de Archivos Estáticos ---

/**
 * Handler para GET /style.css
 * Sirve el archivo CSS desde SPIFFS
 */
static esp_err_t style_get_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "GET /style.css");
    return send_file_from_spiffs(req, "/spiffs/style.css", "text/css");
}

/**
 * Handler para GET /script.js
 * Sirve el archivo JavaScript desde SPIFFS
 */
static esp_err_t script_get_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "GET /script.js");
    return send_file_from_spiffs(req, "/spiffs/script.js", "application/javascript");
}

/**
 * Handler para GET /favicon.ico
 * Responde con 204 No Content para evitar errores 404
 */
static esp_err_t favicon_get_handler(httpd_req_t *req) {
    httpd_resp_set_status(req, "204 No Content");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

// --- Handlers de Autenticación ---

// --- Handlers de API ---

/**
 * Handler para GET /temperature
 * Devuelve la temperatura actual en formato JSON
 * Respuesta: {"temperature": 25.5} o {"error": "No data available"}
 * Requiere autenticación
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

/**
 * Decodifica una cadena URL-encoded (convierte %20 y + a espacios)
 * @param str: Cadena a decodificar (se modifica in-place)
 */
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
 * Convierte una cadena a minúsculas (modifica in-place)
 * @param str: Cadena a convertir
 */
static void str_to_lower(char *str) {
    for (int i = 0; str[i]; i++) {
        if (str[i] >= 'A' && str[i] <= 'Z') {
            str[i] = str[i] - 'A' + 'a';
        }
    }
}

/**
 * Extrae un parámetro de una cadena form-urlencoded
 * @param content: Cadena con los parámetros (ej: "user=root&pass=matrix123")
 * @param param_name: Nombre del parámetro a extraer (ej: "user")
 * @param output: Buffer donde se almacenará el valor
 * @param output_size: Tamaño del buffer
 * @return true si se encontró el parámetro, false en caso contrario
 */
static bool extract_form_param(const char *content, const char *param_name, char *output, size_t output_size) {
    char search_str[64];
    snprintf(search_str, sizeof(search_str), "%s=", param_name);
    char *param_start = strstr(content, search_str);
    
    if (!param_start) {
        return false;
    }
    
    param_start += strlen(search_str);
    char *param_end = strchr(param_start, '&');
    
    if (param_end) {
        size_t param_len = param_end - param_start;
        if (param_len >= output_size) {
            param_len = output_size - 1;
        }
        strncpy(output, param_start, param_len);
        output[param_len] = '\0';
    } else {
        strncpy(output, param_start, output_size - 1);
        output[output_size - 1] = '\0';
    }
    
    url_decode(output);
    return true;
}

/**
 * Handler para GET /cmd?c=comando
 * Procesa comandos desde la terminal web
 * Envía el comando a la cola y espera la respuesta
 * Requiere autenticación
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
                str_to_lower(cmd);
                
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
                    
                    // Intentar recibir respuesta (múltiples intentos para manejar respuestas de otros comandos)
                    for (int attempts = 0; attempts < CMD_RESPONSE_MAX_ATTEMPTS && !response_received; attempts++) {
                        if (ctx->gpio_response_queue != NULL && 
                            xQueueReceive(ctx->gpio_response_queue, &response, pdMS_TO_TICKS(CMD_RESPONSE_TIMEOUT_MS)) == pdTRUE) {
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
    
    // Parsear parámetros del formulario (user=xxx&pass=yyy)
    char user[50] = {0};
    char pass[50] = {0};
    
    extract_form_param(content, "user", user, sizeof(user));
    extract_form_param(content, "pass", pass, sizeof(pass));
    
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

/**
 * Handler para GET /logout
 * Cierra la sesión del usuario y apaga los LEDs
 */
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

// ===== SECCIÓN: INICIALIZACIÓN DE SPIFFS =====
/**
 * Verifica que todos los archivos necesarios estén presentes en SPIFFS
 * @return Número de archivos encontrados
 */
static int verify_spiffs_files(void) {
    ESP_LOGI(TAG, "Verificando archivos en SPIFFS...");
    const char *files_to_check[] = {
        "/spiffs/index.html",
        "/spiffs/login.html",
        "/spiffs/dashboard.html",
        "/spiffs/slider.html",
        "/spiffs/style.css",
        "/spiffs/script.js"
    };
    const int num_files = sizeof(files_to_check) / sizeof(files_to_check[0]);
    int files_found = 0;
    
    for (int i = 0; i < num_files; i++) {
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
    } else if (files_found < num_files) {
        ESP_LOGW(TAG, "Advertencia: Solo %d de %d archivos encontrados", files_found, num_files);
    } else {
        ESP_LOGI(TAG, "✓ Todos los archivos encontrados correctamente (%d/%d)", files_found, num_files);
    }
    
    return files_found;
}

/**
 * Inicializa y monta el sistema de archivos SPIFFS
 * @return ESP_OK si éxito, ESP_FAIL si error
 */
static esp_err_t init_spiffs(void) {
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
        return ESP_FAIL;
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
    
    return ESP_OK;
}

// ===== SECCIÓN: REGISTRO DE RUTAS HTTP =====
/**
 * Registra todas las rutas HTTP del servidor web
 * @param ctx: Contexto del servidor web
 * @return ESP_OK si todas las rutas se registraron correctamente, ESP_FAIL en caso contrario
 */
static esp_err_t register_http_routes(webserver_context_t *ctx) {
    esp_err_t ret = ESP_OK;
    
    // Estructura auxiliar para simplificar el registro
    typedef struct {
        const char *uri;
        httpd_method_t method;
        esp_err_t (*handler)(httpd_req_t *);
        const char *name;
    } route_config_t;
    
    // Configuración de todas las rutas
    route_config_t routes[] = {
        // Páginas principales
        {"/", HTTP_GET, root_get_handler, "root"},
        {"/dashboard", HTTP_GET, dashboard_get_handler, "dashboard"},
        {"/terminal", HTTP_GET, terminal_get_handler, "terminal"},
        {"/slider", HTTP_GET, slider_get_handler, "slider"},
        
        // Archivos estáticos
        {"/style.css", HTTP_GET, style_get_handler, "style.css"},
        {"/script.js", HTTP_GET, script_get_handler, "script.js"},
        {"/favicon.ico", HTTP_GET, favicon_get_handler, "favicon.ico"},
        
        // Autenticación
        {"/login", HTTP_POST, login_post_handler, "login"},
        {"/logout", HTTP_GET, logout_get_handler, "logout"},
        
        // API
        {"/cmd", HTTP_GET, cmd_get_handler, "cmd"},
        {"/temperature", HTTP_GET, temperature_get_handler, "temperature"},
    };
    
    // Registrar cada ruta
    for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); i++) {
        httpd_uri_t uri_config = {
            .uri = routes[i].uri,
            .method = routes[i].method,
            .handler = routes[i].handler,
            .user_ctx = ctx
        };
        
        if (httpd_register_uri_handler(ctx->server, &uri_config) != ESP_OK) {
            ESP_LOGE(TAG, "Error al registrar ruta %s", routes[i].name);
            ret = ESP_FAIL;
        } else {
            ESP_LOGI(TAG, "Ruta %s registrada", routes[i].name);
        }
    }
    
    return ret;
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
    ctx.gpio_command_queue = xQueueCreate(GPIO_QUEUE_SIZE, sizeof(gpio_command_t));
    ctx.gpio_response_queue = xQueueCreate(GPIO_QUEUE_SIZE, sizeof(gpio_command_t));
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
    if (init_spiffs() != ESP_OK) {
        return;
    }
    
    // Verificar que todos los archivos necesarios estén presentes
    verify_spiffs_files();

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
    
    // Registrar todas las rutas HTTP
    if (register_http_routes(&ctx) != ESP_OK) {
        ESP_LOGW(TAG, "Algunas rutas no se pudieron registrar");
    }
    
    ESP_LOGI(TAG, "Servidor Web iniciado correctamente en puerto %d", config.server_port);
}