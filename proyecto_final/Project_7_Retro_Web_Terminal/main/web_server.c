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
 *    - API REST: /cmd (comandos), /temperature (datos JSON), /registros (gestión de registros)
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
 * 6. Gestión de registros de horarios:
 *    - Endpoints HTTP para guardar y leer registros (/registros)
 *    - Utiliza el módulo registros.c para almacenamiento persistente en SPIFFS
 *    - Los registros se guardan en /spiffs/registros.json
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
 * 
 * ============================================================================
 * RESUMEN DE TAREAS, COLAS Y SEMÁFOROS IMPLEMENTADOS:
 * ============================================================================
 * 
 * === TAREAS (TASKS) ===
 * 
 * 1. gpio_command_task_wrapper (Sección 5)
 *    - Nombre: "gpio_cmd_task"
 *    - Stack: 4096 bytes
 *    - Prioridad: 5 (alta)
 *    - Función: Wrapper que adapta el contexto del servidor web y llama a 
 *               terminal_command_task() del módulo terminal_commands.c
 *    - Propósito: Procesar comandos de la terminal web de forma asíncrona
 *    - Estado: Loop infinito, bloquea esperando comandos de la cola
 * 
 * 2. session_management_task (Sección 6)
 *    - Nombre: "session_mgmt"
 *    - Stack: 2048 bytes
 *    - Prioridad: 3 (media)
 *    - Función: Verifica periódicamente las sesiones y expira las inactivas
 *    - Propósito: Gestión automática del ciclo de vida de sesiones
 *    - Estado: Loop infinito, verifica cada 2 segundos
 *    - Acciones: Expira sesiones inactivas > 3 minutos, apaga LEDs automáticamente
 * 
 * === COLAS (QUEUES) ===
 * 
 * 1. gpio_command_queue
 *    - Tipo: QueueHandle_t (FreeRTOS queue)
 *    - Tamaño: GPIO_QUEUE_SIZE (10 elementos)
 *    - Elemento: gpio_command_t (estructura con ID, comando y respuesta)
 *    - Dirección: Handler HTTP → Tarea de procesamiento
 *    - Uso: El handler HTTP /cmd envía comandos aquí
 *    - Operaciones: xQueueSend() desde handler, xQueueReceive() desde tarea
 *    - Thread-safe: Sí, permite comunicación segura entre tareas
 * 
 * 2. gpio_response_queue
 *    - Tipo: QueueHandle_t (FreeRTOS queue)
 *    - Tamaño: GPIO_QUEUE_SIZE (10 elementos)
 *    - Elemento: gpio_command_t (mismo comando con respuesta generada)
 *    - Dirección: Tarea de procesamiento → Handler HTTP
 *    - Uso: La tarea terminal_command_task envía respuestas aquí
 *    - Operaciones: xQueueSend() desde tarea, xQueueReceive() desde handler
 *    - Thread-safe: Sí, permite comunicación asíncrona entre tareas
 * 
 * === SEMÁFOROS (MUTEXES) ===
 * 
 * 1. session_mutex
 *    - Tipo: SemaphoreHandle_t (FreeRTOS mutex)
 *    - Protege: Array ctx->sessions[] (MAX_SESSIONS = 5)
 *    - Uso: Protege acceso concurrente a sesiones de múltiples requests HTTP
 *    - Operaciones: xSemaphoreTake() antes de leer/escribir, xSemaphoreGive() después
 *    - Timeout: Variable (100ms en verificaciones, portMAX_DELAY en creación)
 *    - Funciones que lo usan:
 *      * find_or_create_session() - Crear/buscar sesiones
 *      * is_authenticated() - Verificar autenticación
 *      * session_management_task() - Expirar sesiones inactivas
 * 
 * 2. command_id_mutex
 *    - Tipo: SemaphoreHandle_t (FreeRTOS mutex)
 *    - Protege: ctx->command_id_counter (contador de IDs únicos)
 *    - Uso: Garantiza generación atómica de IDs únicos para comandos
 *    - Operaciones: xSemaphoreTake() antes de incrementar, xSemaphoreGive() después
 *    - Timeout: portMAX_DELAY (espera indefinidamente)
 *    - Funciones que lo usan:
 *      * cmd_get_handler() - Genera ID único para cada comando
 *    - Propósito: Permite emparejar comandos-respuestas cuando hay múltiples comandos simultáneos
 * 
 * ============================================================================
 * FLUJO DE COMUNICACIÓN:
 * ============================================================================
 * 
 * Cliente Web → Handler HTTP (/cmd)
 *   ↓ [xQueueSend] gpio_command_queue
 * Tarea: terminal_command_task
 *   ↓ [process_terminal_command()]
 *   ↓ [xQueueSend] gpio_response_queue
 * Handler HTTP (/cmd)
 *   ↓ [xQueueReceive]
 * Cliente Web ← Respuesta
 * 
 * ============================================================================
 */

// ===== INCLUDES =====
// Headers locales
#include "web_server.h"
#include "gpio_driver.h"
#include "ntc_sensor.h"
#include "registros.h"
#include "time_sync.h"
#include "terminal_commands.h"
#include "fan_control.h"
#include "pir_driver.h"

// ESP-IDF
#include <esp_http_server.h>
#include <esp_spiffs.h>
#include <esp_log.h>
#include <cJSON.h>

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
#include <inttypes.h>
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
// Nota: gpio_command_t está definido en terminal_commands.h

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
    
    // ===== COLAS (QUEUES) =====
    // Las colas permiten comunicación thread-safe entre tareas
    // gpio_command_queue: Cola para enviar comandos desde el handler HTTP a la tarea de procesamiento
    //   - Tipo: QueueHandle_t (FreeRTOS queue)
    //   - Tamaño: GPIO_QUEUE_SIZE (10 elementos)
    //   - Elemento: gpio_command_t (comando con ID, texto y buffer de respuesta)
    //   - Uso: El handler HTTP /cmd envía comandos aquí, la tarea terminal_command_task los recibe
    //   - Thread-safe: Sí, FreeRTOS garantiza acceso seguro desde múltiples tareas
    QueueHandle_t gpio_command_queue;
    
    // gpio_response_queue: Cola para recibir respuestas de la tarea de procesamiento
    //   - Tipo: QueueHandle_t (FreeRTOS queue)
    //   - Tamaño: GPIO_QUEUE_SIZE (10 elementos)
    //   - Elemento: gpio_command_t (mismo comando con respuesta generada)
    //   - Uso: La tarea terminal_command_task envía respuestas aquí, el handler HTTP las recibe
    //   - Thread-safe: Sí, permite comunicación asíncrona entre tareas
    QueueHandle_t gpio_response_queue;
    
    // ===== SEMÁFOROS (MUTEXES) =====
    // Los mutexes protegen recursos compartidos de acceso concurrente
    // session_mutex: Protege el array de sesiones de acceso concurrente
    //   - Tipo: SemaphoreHandle_t (FreeRTOS mutex)
    //   - Uso: Protege ctx->sessions[] de race conditions
    //   - Necesario porque: Múltiples requests HTTP pueden acceder simultáneamente a las sesiones
    //   - Operaciones: xSemaphoreTake() antes de leer/escribir, xSemaphoreGive() después
    //   - Ejemplo: find_or_create_session(), is_authenticated(), session_management_task()
    SemaphoreHandle_t session_mutex;
    
    uint32_t command_id_counter;  // Contador para IDs únicos de comandos
    
    // command_id_mutex: Protege el contador de IDs de comandos
    //   - Tipo: SemaphoreHandle_t (FreeRTOS mutex)
    //   - Uso: Protege ctx->command_id_counter de race conditions
    //   - Necesario porque: Múltiples requests HTTP pueden generar IDs simultáneamente
    //   - Operaciones: xSemaphoreTake() antes de incrementar, xSemaphoreGive() después
    //   - Ejemplo: cmd_get_handler() genera IDs únicos para emparejar comandos-respuestas
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
 * Wrapper para la tarea de procesamiento de comandos
 * Esta función adapta el contexto del servidor web al contexto de terminal
 * y llama a la función del módulo terminal_commands
 * 
 * ===== EXPLICACIÓN DE LA TAREA =====
 * Esta función crea un wrapper que adapta el contexto del servidor web
 * al formato esperado por terminal_command_task(). La tarea real se ejecuta
 * en terminal_commands.c, pero este wrapper permite mantener la modularidad
 * del código.
 * 
 * La tarea real (terminal_command_task) se ejecuta en un loop infinito:
 * 1. Espera comandos de gpio_command_queue (bloqueante, portMAX_DELAY)
 * 2. Procesa el comando usando process_terminal_command()
 * 3. Envía la respuesta a gpio_response_queue
 * 
 * Esta arquitectura permite que el handler HTTP no se bloquee esperando
 * el procesamiento del comando, mejorando la capacidad de respuesta del servidor.
 */
static void gpio_command_task_wrapper(void *pvParameters) {
    webserver_context_t *ctx = (webserver_context_t *)pvParameters;
    
    // Crear estructura de contexto para terminal_commands
    // Solo necesita las colas, que ya están en el contexto del servidor
    struct {
        QueueHandle_t gpio_command_queue;
        QueueHandle_t gpio_response_queue;
    } terminal_ctx = {
        .gpio_command_queue = ctx->gpio_command_queue,
        .gpio_response_queue = ctx->gpio_response_queue
    };
    
    // Llamar a la función del módulo terminal_commands
    terminal_command_task(&terminal_ctx);
}

// ===== SECCIÓN: TAREA DE GESTIÓN DE SESIONES =====
/**
 * Tarea que verifica periódicamente las sesiones y expira las inactivas
 * Apaga los LEDs cuando una sesión expira por timeout
 * 
 * ===== EXPLICACIÓN DE LA TAREA =====
 * Esta es una tarea de FreeRTOS que se ejecuta de forma independiente
 * en segundo plano para gestionar el ciclo de vida de las sesiones.
 * 
 * Funcionamiento:
 * 1. Se ejecuta en un loop infinito (while(1))
 * 2. Espera 2 segundos entre cada verificación (vTaskDelay)
 * 3. Obtiene el tiempo actual y compara con last_activity de cada sesión
 * 4. Si una sesión está inactiva más de SESSION_TIMEOUT_MS (3 minutos):
 *    - Marca la sesión como no autenticada
 *    - Apaga los LEDs por seguridad
 *    - Registra el evento en el log
 * 
 * Uso del semáforo (mutex):
 * - session_mutex protege el acceso al array ctx->sessions[]
 * - xSemaphoreTake() adquiere el mutex antes de leer/escribir
 * - xSemaphoreGive() libera el mutex después de la operación
 * - Timeout de 100ms para evitar bloqueo indefinido si hay problemas
 * 
 * Prioridad: 3 (configurada en start_webserver())
 * Stack: 2048 bytes
 * 
 * Esta tarea es crítica para la seguridad del sistema, ya que:
 * - Previene sesiones huérfanas que consuman recursos
 * - Apaga los LEDs automáticamente cuando el usuario se desconecta
 * - Libera slots de sesión para nuevos usuarios
 */
static void session_management_task(void *pvParameters) {
    webserver_context_t *ctx = (webserver_context_t *)pvParameters;
    ESP_LOGI(TAG, "Tarea de gestión de sesiones iniciada");
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(2000)); // Verificar cada 2 segundos
        
        int64_t now = get_time_ms();
        
        // Proteger acceso a sesiones con mutex
        // xSemaphoreTake() adquiere el mutex con timeout de 100ms
        // Si no se puede adquirir en 100ms, se omite esta iteración
        if (ctx->session_mutex != NULL && xSemaphoreTake(ctx->session_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            // Iterar sobre todas las sesiones posibles
            for (int i = 0; i < MAX_SESSIONS; i++) {
                // Verificar si la sesión está autenticada y ha expirado
                if (ctx->sessions[i].authenticated && 
                    (now - ctx->sessions[i].last_activity > SESSION_TIMEOUT_MS)) {
                    // Expirar la sesión
                    ctx->sessions[i].authenticated = false;
                    // Apagar LEDs por seguridad cuando la sesión expira
                    gpio_set_yellow(false);
                    gpio_set_blue(false);
                    ESP_LOGI(TAG, "Sesión expirada para IP %s. LEDs apagados.", ctx->sessions[i].ip);
                }
            }
            // Liberar el mutex después de terminar las operaciones
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
 * 
 * ===== USO DE SEMÁFORO (MUTEX) =====
 * Esta función accede al array ctx->sessions[] que es compartido entre:
 * - Múltiples handlers HTTP (pueden ejecutarse simultáneamente)
 * - La tarea session_management_task (verifica timeouts periódicamente)
 * 
 * El mutex session_mutex garantiza acceso exclusivo:
 * - xSemaphoreTake() adquiere el mutex (portMAX_DELAY = espera indefinidamente)
 * - Solo una tarea puede acceder a sessions[] a la vez
 * - xSemaphoreGive() libera el mutex al finalizar
 * 
 * Sin el mutex, habría race conditions:
 * - Dos handlers HTTP podrían crear sesiones duplicadas
 * - La tarea de gestión podría expirar una sesión mientras se está usando
 * - Corrupción de datos en el array de sesiones
 */
static session_t* find_or_create_session(webserver_context_t *ctx, const char *ip) {
    int64_t now = get_time_ms();
    session_t *session = NULL;
    
    // ===== ADQUIRIR MUTEX =====
    // Proteger acceso con mutex antes de leer/escribir sessions[]
    // portMAX_DELAY: esperar indefinidamente hasta adquirir el mutex
    // Esto es seguro porque las operaciones son rápidas y el mutex se libera rápidamente
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
        
        // ===== LIBERAR MUTEX =====
        // Liberar el mutex después de terminar todas las operaciones en sessions[]
        // Es crítico liberar el mutex, de lo contrario otras tareas quedarían bloqueadas
        xSemaphoreGive(ctx->session_mutex);
    }
    
    return session; // NULL si no hay slots disponibles
}

/**
 * Verifica si el cliente está autenticado y actualiza la última actividad
 * @param ctx: Contexto del servidor web
 * @param req: Request HTTP
 * @return true si está autenticado, false en caso contrario
 * 
 * ===== USO DE SEMÁFORO (MUTEX) =====
 * Esta función lee y modifica session->authenticated y session->last_activity
 * que son compartidos con:
 * - Otros handlers HTTP (múltiples requests simultáneos)
 * - La tarea session_management_task (expira sesiones)
 * 
 * El mutex session_mutex protege estas operaciones:
 * - Lectura de session->authenticated (verificar estado)
 * - Escritura de session->last_activity (actualizar timestamp)
 * - Escritura de session->authenticated (expirar sesión si timeout)
 * 
 * Timeout de 100ms: si el mutex está ocupado, espera máximo 100ms
 * Si no se puede adquirir, retorna false (sesión no autenticada por seguridad)
 */
static bool is_authenticated(webserver_context_t *ctx, httpd_req_t *req) {
    char ip[16] = {0};
    get_client_ip(req, ip, sizeof(ip));
    
    session_t *session = find_or_create_session(ctx, ip);
    if (!session) {
        return false;
    }
    
    // ===== ADQUIRIR MUTEX =====
    // Proteger acceso con mutex antes de leer/escribir campos de la sesión
    // Timeout de 100ms: si no se puede adquirir rápidamente, asumir no autenticado
    // Esto previene bloqueos largos en el handler HTTP
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
        // ===== LIBERAR MUTEX =====
        // Liberar el mutex después de terminar las operaciones en la sesión
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
    return send_file_from_spiffs(req, "/spiffs/terminal.html", "text/html");
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
 * Handler para GET /registros
 * Devuelve todos los registros almacenados en formato JSON desde SPIFFS
 * Utiliza la función leer_registros_json() del módulo registros.c
 * Respuesta: Array JSON con todos los registros [{"dia": "...", "hora": "...", "velocidad": ...}, ...]
 * Requiere autenticación
 */
static esp_err_t registros_get_handler(httpd_req_t *req) {
    webserver_context_t *ctx = (webserver_context_t *)req->user_ctx;
    if (!is_authenticated(ctx, req)) {
        httpd_resp_set_status(req, "401 Unauthorized");
        httpd_resp_send(req, "Unauthorized", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    // Leer registros desde SPIFFS usando el módulo registros.c
    char *json = leer_registros_json();
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json);
    free(json);
    
    return ESP_OK;
}

/**
 * Handler para POST /registros
 * Agrega un nuevo registro al archivo registros.json en SPIFFS
 * Utiliza la función agregar_registro() del módulo registros.c
 * Body esperado: {"dia": "lunes", "hora": "14:30", "velocidad": 50}
 * El registro se guarda de forma persistente en /spiffs/registros.json
 * Requiere autenticación
 */
static esp_err_t registros_post_handler(httpd_req_t *req) {
    webserver_context_t *ctx = (webserver_context_t *)req->user_ctx;
    if (!is_authenticated(ctx, req)) {
        httpd_resp_set_status(req, "401 Unauthorized");
        httpd_resp_send(req, "Unauthorized", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    char buf[256];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, "Error recibiendo datos", HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }
    buf[len] = '\0';
    
    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        ESP_LOGE(TAG, "Error parseando JSON del POST /registros");
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, "JSON inválido", HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }
    
    cJSON *dia_item = cJSON_GetObjectItem(root, "dia");
    cJSON *hora_item = cJSON_GetObjectItem(root, "hora");
    cJSON *velocidad_item = cJSON_GetObjectItem(root, "velocidad");
    
    if (!dia_item || !hora_item || !velocidad_item ||
        !cJSON_IsString(dia_item) || !cJSON_IsString(hora_item) || !cJSON_IsNumber(velocidad_item)) {
        ESP_LOGE(TAG, "Campos faltantes o inválidos en JSON");
        cJSON_Delete(root);
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, "Campos requeridos: dia (string), hora (string), velocidad (number)", HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }
    
    const char *dia = dia_item->valuestring;
    const char *hora = hora_item->valuestring;
    int velocidad = velocidad_item->valueint;
    
    // Guardar registro en SPIFFS usando el módulo registros.c
    bool ok = agregar_registro(dia, hora, velocidad);
    cJSON_Delete(root);
    
    if (ok) {
        httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    } else {
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_send(req, "Error guardando registro", HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }
}

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
 * Handler para POST /fan/mode
 * Establece el modo de operación del ventilador
 * Body esperado: {"mode": "manual"} o {"mode": "off"} o {"mode": "temperature"}
 * Requiere autenticación
 */
static esp_err_t fan_mode_post_handler(httpd_req_t *req) {
    webserver_context_t *ctx = (webserver_context_t *)req->user_ctx;
    if (!is_authenticated(ctx, req)) {
        httpd_resp_set_status(req, "401 Unauthorized");
        httpd_resp_send(req, "Unauthorized", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    char buf[128];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, "Error recibiendo datos", HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }
    buf[len] = '\0';
    
    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        ESP_LOGE(TAG, "Error parseando JSON del POST /fan/mode");
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, "JSON inválido", HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }
    
    cJSON *mode_item = cJSON_GetObjectItem(root, "mode");
    if (!mode_item || !cJSON_IsString(mode_item)) {
        ESP_LOGE(TAG, "Campo 'mode' faltante o inválido en JSON");
        cJSON_Delete(root);
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, "Campo requerido: mode (string)", HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }
    
    const char *mode_str = mode_item->valuestring;
    fan_mode_t mode;
    
    if (strcmp(mode_str, "off") == 0) {
        mode = FAN_MODE_OFF;
    } else if (strcmp(mode_str, "manual") == 0) {
        mode = FAN_MODE_MANUAL;
    } else if (strcmp(mode_str, "temperature") == 0) {
        mode = FAN_MODE_AUTO_TEMP;
    } else if (strcmp(mode_str, "schedule") == 0) {
        mode = FAN_MODE_SCHEDULE;
    } else {
        ESP_LOGE(TAG, "Modo inválido: %s", mode_str);
        cJSON_Delete(root);
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, "Modo inválido. Valores válidos: off, manual, temperature, schedule", HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }
    
    // Establecer el modo en el ventilador
    fan_set_mode(mode);
    cJSON_Delete(root);
    
    ESP_LOGI(TAG, "Modo del ventilador cambiado a: %s", mode_str);
    httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/**
 * Handler para POST /fan/manual
 * Establece el porcentaje de velocidad en modo manual
 * Body esperado: {"percent": 50} (0-100)
 * Requiere autenticación
 */
static esp_err_t fan_manual_post_handler(httpd_req_t *req) {
    webserver_context_t *ctx = (webserver_context_t *)req->user_ctx;
    if (!is_authenticated(ctx, req)) {
        httpd_resp_set_status(req, "401 Unauthorized");
        httpd_resp_send(req, "Unauthorized", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    char buf[128];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, "Error recibiendo datos", HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }
    buf[len] = '\0';
    
    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        ESP_LOGE(TAG, "Error parseando JSON del POST /fan/manual");
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, "JSON inválido", HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }
    
    cJSON *percent_item = cJSON_GetObjectItem(root, "percent");
    if (!percent_item || !cJSON_IsNumber(percent_item)) {
        ESP_LOGE(TAG, "Campo 'percent' faltante o inválido en JSON");
        cJSON_Delete(root);
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, "Campo requerido: percent (number 0-100)", HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }
    
    int percent = percent_item->valueint;
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    
    // Establecer el porcentaje manual
    fan_set_manual_percent((uint8_t)percent);
    
    // Asegurar que el modo esté en MANUAL
    if (fan_get_mode() != FAN_MODE_MANUAL) {
        fan_set_mode(FAN_MODE_MANUAL);
    }
    
    cJSON_Delete(root);
    
    ESP_LOGI(TAG, "Porcentaje manual del ventilador establecido a: %d%%", percent);
    httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/**
 * Handler para GET /fan/status
 * Devuelve el estado actual del ventilador en formato JSON
 * Respuesta: {"mode": "manual", "percent": 50}
 * Requiere autenticación
 */
static esp_err_t fan_status_get_handler(httpd_req_t *req) {
    webserver_context_t *ctx = (webserver_context_t *)req->user_ctx;
    if (!is_authenticated(ctx, req)) {
        httpd_resp_set_status(req, "401 Unauthorized");
        httpd_resp_send(req, "Unauthorized", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    fan_mode_t mode = fan_get_mode();
    uint8_t percent = fan_get_current_percent();
    
    const char *mode_str;
    switch (mode) {
        case FAN_MODE_OFF:
            mode_str = "off";
            break;
        case FAN_MODE_MANUAL:
            mode_str = "manual";
            break;
        case FAN_MODE_AUTO_TEMP:
            mode_str = "temperature";
            break;
        case FAN_MODE_SCHEDULE:
            mode_str = "schedule";
            break;
        default:
            mode_str = "unknown";
            break;
    }
    
    char response[128];
    int len = snprintf(response, sizeof(response), "{\"mode\":\"%s\",\"percent\":%d}", mode_str, percent);
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response, len);
    
    return ESP_OK;
}

/**
 * Handler para GET /pir/status
 * Devuelve el estado actual del sensor PIR en formato JSON
 * Respuesta: {"motion": true} o {"motion": false}
 * Requiere autenticación
 */
static esp_err_t pir_status_get_handler(httpd_req_t *req) {
    webserver_context_t *ctx = (webserver_context_t *)req->user_ctx;
    if (!is_authenticated(ctx, req)) {
        httpd_resp_set_status(req, "401 Unauthorized");
        httpd_resp_send(req, "Unauthorized", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    // Leer el estado del PIR de forma segura
    // pir_is_motion_active() retorna false si el PIR no está inicializado
    // Esto es seguro y siempre retorna un valor válido (false si no está inicializado)
    bool motion_detected = pir_is_motion_active();
    
    // Preparar respuesta JSON
    char response[64];
    int len = snprintf(response, sizeof(response), "{\"motion\":%s}", motion_detected ? "true" : "false");
    
    // Verificar que el buffer sea suficiente y que la operación fue exitosa
    if (len < 0 || len >= (int)sizeof(response)) {
        ESP_LOGE(TAG, "Error formateando respuesta PIR (len=%d, size=%zu)", len, sizeof(response));
        // En caso de error, devolver un JSON válido con motion=false
        len = snprintf(response, sizeof(response), "{\"motion\":false}");
        if (len < 0 || len >= (int)sizeof(response)) {
            // Si aún falla, usar respuesta hardcodeada
            const char *fallback = "{\"motion\":false}";
            httpd_resp_set_type(req, "application/json");
            httpd_resp_send(req, fallback, HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        }
    }
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
    httpd_resp_send(req, response, len);
    
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
                
                // ===== GENERACIÓN DE ID ÚNICO CON MUTEX =====
                // El mutex command_id_mutex protege el contador de IDs
                // Esto es necesario porque múltiples requests HTTP pueden ejecutarse simultáneamente
                // y necesitamos garantizar que cada comando tenga un ID único
                uint32_t cmd_id = 0;
                if (ctx->command_id_mutex != NULL && xSemaphoreTake(ctx->command_id_mutex, portMAX_DELAY) == pdTRUE) {
                    // Incrementar contador de forma atómica (protegido por mutex)
                    cmd_id = ++ctx->command_id_counter;
                    xSemaphoreGive(ctx->command_id_mutex);
                }
                
                // ===== ENVÍO DE COMANDO A LA COLA =====
                // Preparar estructura de comando con ID único
                gpio_command_t command;
                command.command_id = cmd_id;
                strncpy(command.command, cmd, sizeof(command.command) - 1);
                command.command[sizeof(command.command) - 1] = '\0';
                
                // Enviar comando a la cola gpio_command_queue
                // xQueueSend() es thread-safe y bloquea si la cola está llena
                // Timeout de 1000ms: si la cola está llena por más de 1 segundo, falla
                // La tarea terminal_command_task recibe este comando y lo procesa
                if (ctx->gpio_command_queue != NULL && 
                    xQueueSend(ctx->gpio_command_queue, &command, pdMS_TO_TICKS(1000)) == pdTRUE) {
                    
                    // ===== RECEPCIÓN DE RESPUESTA DE LA COLA =====
                    // Esperar respuesta de la tarea terminal_command_task
                    // La respuesta viene en gpio_response_queue con el mismo command_id
                    gpio_command_t response;
                    bool response_received = false;
                    
                    // Intentar recibir respuesta (múltiples intentos para manejar respuestas de otros comandos)
                    // Esto es necesario porque múltiples comandos pueden estar en proceso simultáneamente
                    for (int attempts = 0; attempts < CMD_RESPONSE_MAX_ATTEMPTS && !response_received; attempts++) {
                        // xQueueReceive() recibe de la cola con timeout de 500ms
                        // Si no hay respuesta en 500ms, intenta de nuevo (hasta CMD_RESPONSE_MAX_ATTEMPTS veces)
                        if (ctx->gpio_response_queue != NULL && 
                            xQueueReceive(ctx->gpio_response_queue, &response, pdMS_TO_TICKS(CMD_RESPONSE_TIMEOUT_MS)) == pdTRUE) {
                            // Verificar que la respuesta corresponde a nuestro comando usando command_id
                            // Esto es crítico porque múltiples comandos pueden estar en proceso
                            if (response.command_id == cmd_id) {
                                // Respuesta correcta: enviar al cliente HTTP
                                httpd_resp_send(req, response.response, HTTPD_RESP_USE_STRLEN);
                                response_received = true;
                                return ESP_OK;
                            } else {
                                // Respuesta de otro comando: devolverla a la cola para que otro handler la reciba
                                // xQueueSendToFront() la coloca al frente para que se procese rápidamente
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

/**
 * Handler para GET /fan/diagnostic
 * Endpoint de diagnóstico que muestra el estado completo del sistema de registros
 * Útil para depurar problemas con el modo SCHEDULE
 * Requiere autenticación
 */
static esp_err_t fan_diagnostic_get_handler(httpd_req_t *req) {
    webserver_context_t *ctx = (webserver_context_t *)req->user_ctx;
    if (!is_authenticated(ctx, req)) {
        httpd_resp_set_status(req, "401 Unauthorized");
        httpd_resp_send(req, "Unauthorized", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    cJSON *root = cJSON_CreateObject();
    
    // 1. Estado de sincronización de hora
    bool hora_sync = hora_sincronizada();
    cJSON_AddBoolToObject(root, "hora_sincronizada", hora_sync);
    
    // 2. Día y hora actual
    char dia_actual[32] = "N/A";
    char hora_actual[16] = "N/A";
    bool dia_ok = obtener_dia_actual(dia_actual, sizeof(dia_actual));
    bool hora_ok = obtener_hora_actual(hora_actual, sizeof(hora_actual));
    
    cJSON_AddStringToObject(root, "dia_actual", dia_ok ? dia_actual : "ERROR");
    cJSON_AddStringToObject(root, "hora_actual", hora_ok ? hora_actual : "ERROR");
    
    // 3. Modo del ventilador
    fan_mode_t mode = fan_get_mode();
    const char *mode_str = "unknown";
    switch (mode) {
        case FAN_MODE_OFF: mode_str = "off"; break;
        case FAN_MODE_MANUAL: mode_str = "manual"; break;
        case FAN_MODE_AUTO_TEMP: mode_str = "temperature"; break;
        case FAN_MODE_SCHEDULE: mode_str = "schedule"; break;
    }
    cJSON_AddStringToObject(root, "fan_mode", mode_str);
    cJSON_AddBoolToObject(root, "modo_es_schedule", (mode == FAN_MODE_SCHEDULE));
    
    // 4. Velocidad actual
    cJSON_AddNumberToObject(root, "velocidad_actual", fan_get_current_percent());
    
    // 5. Registros guardados
    char *json_registros = leer_registros_json();
    cJSON *registros_array = cJSON_Parse(json_registros);
    if (registros_array && cJSON_IsArray(registros_array)) {
        cJSON_AddItemToObject(root, "registros", registros_array);
        int num_registros = cJSON_GetArraySize(registros_array);
        cJSON_AddNumberToObject(root, "total_registros", num_registros);
        
        // 6. Verificar si hay registro activo
        if (dia_ok && hora_ok && mode == FAN_MODE_SCHEDULE) {
            registro_activo_t registro = verificar_registro_activo(dia_actual, hora_actual);
            cJSON_AddBoolToObject(root, "registro_activo", registro.activo);
            cJSON_AddNumberToObject(root, "velocidad_registro_activo", registro.velocidad);
        } else {
            cJSON_AddBoolToObject(root, "registro_activo", false);
            cJSON_AddStringToObject(root, "registro_activo_error", 
                !dia_ok ? "No se pudo obtener día" :
                !hora_ok ? "No se pudo obtener hora" :
                "Modo no es SCHEDULE");
        }
    } else {
        cJSON_AddNumberToObject(root, "total_registros", 0);
        cJSON_AddStringToObject(root, "registros_error", "Error leyendo registros");
        if (registros_array) cJSON_Delete(registros_array);
    }
    free(json_registros);
    
    // 7. Información de tiempo completo
    if (hora_sync) {
        time_t now;
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);
        
        char fecha_hora[64];
        snprintf(fecha_hora, sizeof(fecha_hora), "%04d-%02d-%02d %02d:%02d:%02d",
                 timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                 timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        cJSON_AddStringToObject(root, "fecha_hora_completa", fecha_hora);
    }
    
    char *json_str = cJSON_Print(root);
    cJSON_Delete(root);
    
    if (!json_str) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_send(req, "Error generando JSON", HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, strlen(json_str));
    free(json_str);
    
    return ESP_OK;
}

/**
 * Handler para POST /time/set
 * Establece la hora manualmente
 * Body esperado: {"year": 2024, "month": 12, "day": 9, "hour": 14, "minute": 30, "second": 0}
 * Requiere autenticación
 */
static esp_err_t time_set_post_handler(httpd_req_t *req) {
    webserver_context_t *ctx = (webserver_context_t *)req->user_ctx;
    if (!is_authenticated(ctx, req)) {
        httpd_resp_set_status(req, "401 Unauthorized");
        httpd_resp_send(req, "Unauthorized", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    char buf[256];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, "Error recibiendo datos", HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }
    buf[len] = '\0';
    
    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        ESP_LOGE(TAG, "Error parseando JSON del POST /time/set");
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, "JSON inválido", HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }
    
    cJSON *year_item = cJSON_GetObjectItem(root, "year");
    cJSON *month_item = cJSON_GetObjectItem(root, "month");
    cJSON *day_item = cJSON_GetObjectItem(root, "day");
    cJSON *hour_item = cJSON_GetObjectItem(root, "hour");
    cJSON *minute_item = cJSON_GetObjectItem(root, "minute");
    cJSON *second_item = cJSON_GetObjectItem(root, "second");
    
    if (!year_item || !month_item || !day_item || !hour_item || !minute_item || !second_item ||
        !cJSON_IsNumber(year_item) || !cJSON_IsNumber(month_item) || !cJSON_IsNumber(day_item) ||
        !cJSON_IsNumber(hour_item) || !cJSON_IsNumber(minute_item) || !cJSON_IsNumber(second_item)) {
        ESP_LOGE(TAG, "Campos faltantes o inválidos en JSON");
        cJSON_Delete(root);
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, "Campos requeridos: year, month, day, hour, minute, second (todos números)", HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }
    
    int year = year_item->valueint;
    int month = month_item->valueint;
    int day = day_item->valueint;
    int hour = hour_item->valueint;
    int minute = minute_item->valueint;
    int second = second_item->valueint;
    
    // Validar rangos
    if (year < 2020 || year > 2100 ||
        month < 1 || month > 12 ||
        day < 1 || day > 31 ||
        hour < 0 || hour > 23 ||
        minute < 0 || minute > 59 ||
        second < 0 || second > 59) {
        cJSON_Delete(root);
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, "Valores fuera de rango válido", HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }
    
    bool ok = establecer_hora_manual(year, month, day, hour, minute, second);
    cJSON_Delete(root);
    
    if (ok) {
        ESP_LOGI(TAG, "Hora establecida manualmente desde web: %04d-%02d-%02d %02d:%02d:%02d",
                 year, month, day, hour, minute, second);
        httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
    } else {
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_send(req, "Error al establecer hora", HTTPD_RESP_USE_STRLEN);
    }
    
    return ESP_OK;
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
        "/spiffs/terminal.html",
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
        {"/registros", HTTP_GET, registros_get_handler, "registros_get"},
        {"/registros", HTTP_POST, registros_post_handler, "registros_post"},
        
        // API de control del ventilador
        {"/fan/mode", HTTP_POST, fan_mode_post_handler, "fan_mode"},
        {"/fan/manual", HTTP_POST, fan_manual_post_handler, "fan_manual"},
        {"/fan/status", HTTP_GET, fan_status_get_handler, "fan_status"},
        {"/fan/diagnostic", HTTP_GET, fan_diagnostic_get_handler, "fan_diagnostic"},
        
        // API de sensor PIR
        {"/pir/status", HTTP_GET, pir_status_get_handler, "pir_status"},
        
        // API de tiempo
        {"/time/set", HTTP_POST, time_set_post_handler, "time_set"},
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
    
    // ===== CREACIÓN DE SEMÁFOROS (MUTEXES) =====
    // 0.1. Crear mutex para sesiones
    // Este mutex protege el array ctx->sessions[] de acceso concurrente
    // Múltiples requests HTTP pueden acceder simultáneamente a las sesiones
    // El mutex garantiza que solo una tarea acceda a la vez
    ctx.session_mutex = xSemaphoreCreateMutex();
    if (ctx.session_mutex == NULL) {
        ESP_LOGE(TAG, "Error al crear mutex para sesiones");
        return;
    }
    ESP_LOGI(TAG, "Mutex de sesiones creado");
    
    // 0.2. Crear mutex para IDs de comandos
    // Este mutex protege ctx->command_id_counter de race conditions
    // Cuando múltiples requests HTTP generan comandos simultáneamente,
    // cada uno necesita un ID único, por lo que el incremento debe ser atómico
    ctx.command_id_mutex = xSemaphoreCreateMutex();
    if (ctx.command_id_mutex == NULL) {
        ESP_LOGE(TAG, "Error al crear mutex para IDs de comandos");
        return;
    }
    ESP_LOGI(TAG, "Mutex de IDs de comandos creado");
    
    // ===== CREACIÓN DE COLAS (QUEUES) =====
    // 0.3. Crear colas para comandos GPIO
    // Las colas permiten comunicación thread-safe entre el handler HTTP y la tarea de procesamiento
    // gpio_command_queue: Handler HTTP -> Tarea de procesamiento (comandos)
    // gpio_response_queue: Tarea de procesamiento -> Handler HTTP (respuestas)
    // Tamaño: GPIO_QUEUE_SIZE (10 elementos) - permite hasta 10 comandos en cola
    // Elemento: gpio_command_t (estructura con ID, comando y respuesta)
    ctx.gpio_command_queue = xQueueCreate(GPIO_QUEUE_SIZE, sizeof(gpio_command_t));
    ctx.gpio_response_queue = xQueueCreate(GPIO_QUEUE_SIZE, sizeof(gpio_command_t));
    if (ctx.gpio_command_queue == NULL || ctx.gpio_response_queue == NULL) {
        ESP_LOGE(TAG, "Error al crear colas para comandos GPIO");
        return;
    }
    ESP_LOGI(TAG, "Colas de comandos GPIO creadas (tamaño: %d)", GPIO_QUEUE_SIZE);
    
    // ===== CREACIÓN DE TAREAS (TASKS) =====
    // 0.4. Crear tarea de procesamiento de comandos GPIO
    // Esta tarea se ejecuta de forma independiente y procesa comandos de la cola
    // Parámetros:
    //   - gpio_command_task_wrapper: Función que se ejecutará (wrapper que llama a terminal_command_task)
    //   - "gpio_cmd_task": Nombre de la tarea (útil para debugging)
    //   - 4096: Tamaño del stack en bytes (suficiente para procesamiento de comandos)
    //   - &ctx: Contexto pasado a la tarea (puntero al webserver_context_t)
    //   - 5: Prioridad de la tarea (mayor número = mayor prioridad, rango típico: 1-10)
    //   - NULL: Handle de la tarea (no lo necesitamos, por eso NULL)
    // La tarea se ejecuta en un loop infinito esperando comandos de gpio_command_queue
    xTaskCreate(gpio_command_task_wrapper, "gpio_cmd_task", 4096, &ctx, 5, NULL);
    ESP_LOGI(TAG, "Tarea de comandos GPIO creada (stack: 4096, prioridad: 5)");
    
    // 0.5. Crear tarea de gestión de sesiones
    // Esta tarea verifica periódicamente las sesiones y expira las inactivas
    // Parámetros:
    //   - session_management_task: Función que se ejecutará
    //   - "session_mgmt": Nombre de la tarea
    //   - 2048: Tamaño del stack (menor que la anterior porque hace menos trabajo)
    //   - &ctx: Contexto con las sesiones y el mutex
    //   - 3: Prioridad más baja (no es crítica, solo mantenimiento)
    //   - NULL: Handle de la tarea
    // La tarea se ejecuta cada 2 segundos verificando timeouts de sesiones
    xTaskCreate(session_management_task, "session_mgmt", 2048, &ctx, 3, NULL);
    ESP_LOGI(TAG, "Tarea de gestión de sesiones creada (stack: 2048, prioridad: 3)");
    
    // 1. Iniciar SPIFFS
    if (init_spiffs() != ESP_OK) {
        return;
    }
    
    // Verificar que todos los archivos necesarios estén presentes
    verify_spiffs_files();
    
    // Inicializar sistema de registros: crear archivo registros.json si no existe
    // Utiliza la función del módulo registros.c para crear el archivo base en SPIFFS
    crear_archivo_si_no_existe();

    // 2. Configurar Server
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 17;  // Aumentado para incluir las nuevas rutas del ventilador
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