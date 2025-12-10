/**
 * @file time_sync.c
 * @brief Implementación de la sincronización de tiempo mediante SNTP
 * @author Proyecto Final RTOS
 * @date 2024
 * 
 * @details Implementación de la sincronización de tiempo usando SNTP (Simple Network
 * Time Protocol). Este módulo permite obtener la hora y fecha actual del
 * sistema, necesaria para el sistema de horarios del ventilador.
 * 
 * @section functionality Funcionalidades
 * - Sincronización automática con servidores NTP (co.pool.ntp.org)
 * - Configuración de zona horaria para Colombia (UTC-5)
 * - Persistencia de hora en NVS para restaurar después de reinicio
 * - Establecimiento manual de hora cuando SNTP no está disponible
 * - Obtención de día de la semana y hora actual en formato español
 * 
 * @section features Características
 * - Restauración automática de hora desde NVS al reiniciar
 * - Fallback a hora manual si SNTP no puede sincronizar
 * - Soporte para zona horaria de Colombia (COT5)
 * - Validación de sincronización antes de usar funciones de tiempo
 * 
 * ============================================================================
 * ARCHIVO: time_sync.c
 * ============================================================================
 * 
 * RESUMEN:
 * Implementación de la sincronización de tiempo usando SNTP (Simple Network
 * Time Protocol). Este módulo permite obtener la hora y fecha actual del
 * sistema, necesaria para el sistema de horarios del ventilador.
 * 
 * Funcionalidades:
 * - Sincronización automática con servidores NTP (co.pool.ntp.org)
 * - Configuración de zona horaria para Colombia (UTC-5)
 * - Persistencia de hora en NVS para restaurar después de reinicio
 * - Establecimiento manual de hora cuando SNTP no está disponible
 * - Obtención de día de la semana y hora actual en formato español
 * 
 * Características:
 * - Restauración automática de hora desde NVS al reiniciar
 * - Fallback a hora manual si SNTP no puede sincronizar
 * - Soporte para zona horaria de Colombia (COT5)
 * - Validación de sincronización antes de usar funciones de tiempo
 * 
 * ============================================================================
 * ÍNDICE DE SECCIONES:
 * ============================================================================
 * Sección 1: INCLUDES se encuentra en las líneas 24 a 36
 * Sección 2: DEFINICIONES Y CONSTANTES se encuentra en las líneas 38 a 48
 * Sección 3: FUNCIONES INTERNAS se encuentra en las líneas 50 a 158
 * Sección 4: FUNCIONES PÚBLICAS se encuentra en las líneas 160 a 337
 * ============================================================================
 * 
 * ============================================================================
 * RESUMEN DE TAREAS, COLAS Y SEMÁFOROS IMPLEMENTADOS:
 * ============================================================================
 * 
 * === TAREAS (TASKS) ===
 * 
 * Ninguna en este módulo. SNTP funciona mediante callbacks y eventos.
 * 
 * === COLAS (QUEUES) ===
 * 
 * Ninguna en este módulo.
 * 
 * === SEMÁFOROS (MUTEXES) ===
 * 
 * Ninguno en este módulo. Las funciones de tiempo de C estándar son thread-safe.
 * 
 * ============================================================================
 */

// ===== INCLUDES =====
#include "time_sync.h"
#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "lwip/apps/sntp.h"
#include "nvs.h"
#include "esp_system.h"
#include "esp_timer.h"

// Estándar C
#include <time.h>
#include <string.h>
#include <sys/time.h>

// ===== DEFINICIONES Y CONSTANTES =====
static const char *TAG = "TIME_SYNC";
static const char *NVS_NAMESPACE = "time_sync";
static const char *NVS_KEY_TIME = "saved_time";
static bool sntp_initialized = false;
static bool time_synced = false;

// Nombres de días en español
static const char *dias_semana[] = {
    "domingo", "lunes", "martes", "miercoles", "jueves", "viernes", "sabado"
};

// ===== FUNCIONES INTERNAS =====

/**
 * Guarda la hora actual en NVS para restaurarla después de un reinicio
 */
static void guardar_hora_en_nvs(time_t timestamp) {
    nvs_handle_t nvs_handle;
    esp_err_t err;
    
    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Error abriendo NVS para guardar hora: %s", esp_err_to_name(err));
        return;
    }
    
    // Guardar timestamp
    err = nvs_set_i64(nvs_handle, NVS_KEY_TIME, (int64_t)timestamp);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Error guardando hora en NVS: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return;
    }
    
    // Guardar timestamp del momento actual (para referencia)
    // Nota: No guardamos boot_time porque no podemos calcular exactamente
    // el tiempo entre guardado y reinicio. Simplemente usaremos el uptime
    // actual al restaurar como aproximación.
    
    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Error haciendo commit en NVS: %s", esp_err_to_name(err));
    }
    
    nvs_close(nvs_handle);
    ESP_LOGI(TAG, "Hora guardada en NVS: %lld", (long long)timestamp);
}

/**
 * Carga la hora guardada en NVS y la restaura ajustada por el tiempo transcurrido
 * @return true si se restauró la hora, false si no había hora guardada
 */
static bool cargar_hora_desde_nvs(void) {
    nvs_handle_t nvs_handle;
    esp_err_t err;
    
    err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGD(TAG, "No se pudo abrir NVS para leer hora (puede ser primera vez): %s", esp_err_to_name(err));
        return false;
    }
    
    int64_t saved_time = 0;
    
    // Leer hora guardada
    err = nvs_get_i64(nvs_handle, NVS_KEY_TIME, &saved_time);
    if (err != ESP_OK) {
        ESP_LOGD(TAG, "No hay hora guardada en NVS: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return false;
    }
    
    nvs_close(nvs_handle);
    
    // Calcular tiempo transcurrido desde el último guardado
    // Usamos el uptime del sistema como aproximación del tiempo transcurrido
    // desde que se guardó la hora. Esto no es perfecto (no sabemos cuánto tiempo
    // pasó entre el guardado y el reinicio), pero es una aproximación razonable.
    int64_t uptime_seconds = esp_timer_get_time() / 1000000;  // Segundos desde boot
    
    // Restaurar hora ajustada: hora guardada + tiempo transcurrido desde boot
    // Nota: Esto asume que el sistema se reinició justo después de guardar,
    // lo cual no es cierto, pero es una aproximación útil.
    time_t restored_time = (time_t)(saved_time + uptime_seconds);
    
    struct timeval tv = {
        .tv_sec = restored_time,
        .tv_usec = 0
    };
    
    if (settimeofday(&tv, NULL) != 0) {
        ESP_LOGE(TAG, "Error al restaurar hora desde NVS");
        return false;
    }
    
    struct tm timeinfo;
    localtime_r(&restored_time, &timeinfo);
    ESP_LOGI(TAG, "Hora restaurada desde NVS: %04d-%02d-%02d %02d:%02d:%02d",
             timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    
    time_synced = true;
    return true;
}

/**
 * Callback que se ejecuta cuando SNTP sincroniza la hora
 */
static void sntp_sync_time_cb(struct timeval *tv) {
    time_synced = true;
    ESP_LOGI(TAG, "Hora sincronizada con servidor NTP (UTC)");
    
    // Guardar la hora sincronizada en NVS (en UTC)
    // La zona horaria se aplicará automáticamente cuando usemos localtime_r()
    if (tv && tv->tv_sec > 0) {
        guardar_hora_en_nvs(tv->tv_sec);
        
        // Mostrar hora UTC y hora local de Colombia
        struct tm timeinfo_utc, timeinfo_local;
        gmtime_r(&tv->tv_sec, &timeinfo_utc);
        localtime_r(&tv->tv_sec, &timeinfo_local);
        
        ESP_LOGI(TAG, "Hora UTC: %04d-%02d-%02d %02d:%02d:%02d",
                 timeinfo_utc.tm_year + 1900, timeinfo_utc.tm_mon + 1, timeinfo_utc.tm_mday,
                 timeinfo_utc.tm_hour, timeinfo_utc.tm_min, timeinfo_utc.tm_sec);
        ESP_LOGI(TAG, "Hora Colombia (UTC-5): %04d-%02d-%02d %02d:%02d:%02d",
                 timeinfo_local.tm_year + 1900, timeinfo_local.tm_mon + 1, timeinfo_local.tm_mday,
                 timeinfo_local.tm_hour, timeinfo_local.tm_min, timeinfo_local.tm_sec);
    }
}

/**
 * Inicializa el servicio SNTP para sincronizar la hora del sistema
 * Configura servidores NTP y espera a que la hora se sincronice
 */
void time_sync_init(void) {
    if (sntp_initialized) {
        ESP_LOGW(TAG, "SNTP ya está inicializado");
        return;
    }
    
    ESP_LOGI(TAG, "Inicializando SNTP...");
    
    // Configurar zona horaria para Colombia (UTC-5, sin horario de verano)
    // Formato: "COT5" (Colombia Time, UTC-5)
    setenv("TZ", "COT5", 1);  // Zona horaria de Colombia (UTC-5, sin horario de verano)
    tzset();
    
    // Configurar servidores NTP
    // Usar servidor de Colombia para obtener hora local directamente
    // pool.ntp.org también funciona pero pool.ntp.org para zona Colombia puede ser mejor
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("co.pool.ntp.org");
    
    // Registrar callback para cuando se sincronice la hora
    config.sync_cb = sntp_sync_time_cb;
    
    // Inicializar SNTP
    esp_netif_sntp_init(&config);
    sntp_initialized = true;
    
    ESP_LOGI(TAG, "SNTP inicializado, esperando sincronización...");
    
    // Esperar a que la hora se sincronice (máximo 30 segundos)
    int retry = 0;
    const int max_retries = 15;
    while (!time_synced && retry < max_retries) {
        if (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(2000)) == ESP_OK) {
            time_synced = true;
            break;
        }
        retry++;
        ESP_LOGI(TAG, "Esperando sincronización de hora... (%d/%d)", retry, max_retries);
    }
    
    if (time_synced) {
        time_t now;
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);
        ESP_LOGI(TAG, "Hora sincronizada (Colombia UTC-5): %04d-%02d-%02d %02d:%02d:%02d",
                 timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                 timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        
        // La hora ya fue guardada en el callback con el ajuste de zona horaria
    } else {
        ESP_LOGW(TAG, "⚠ No se pudo sincronizar la hora con NTP.");
        ESP_LOGW(TAG, "⚠ Intentando restaurar hora desde NVS...");
        
        // Intentar restaurar la hora guardada en NVS
        if (cargar_hora_desde_nvs()) {
            ESP_LOGI(TAG, "✓ Hora restaurada desde NVS");
            time_synced = true;
        } else {
            ESP_LOGW(TAG, "⚠ No hay hora guardada en NVS.");
            ESP_LOGW(TAG, "⚠ El ESP32 está en modo SoftAP (sin Internet), SNTP no puede sincronizar.");
            ESP_LOGW(TAG, "⚠ El sistema de horarios NO funcionará hasta que la hora esté sincronizada.");
            ESP_LOGW(TAG, "⚠ Solución: Conectar el ESP32 a una red WiFi con Internet o configurar hora manualmente.");
        }
    }
}

/**
 * Verifica si la hora del sistema está sincronizada
 */
bool hora_sincronizada(void) {
    if (!sntp_initialized) {
        return false;
    }
    
    // Verificar que la hora sea válida (no 1970)
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    
    // Si el año es 1970 o anterior, la hora no está sincronizada
    if (timeinfo.tm_year < (2016 - 1900)) {
        return false;
    }
    
    return time_synced;
}

/**
 * Obtiene el día de la semana actual en español
 */
bool obtener_dia_actual(char *dia_buffer, size_t buffer_size) {
    if (!hora_sincronizada()) {
        ESP_LOGW(TAG, "Hora no sincronizada, no se puede obtener el día");
        return false;
    }
    
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    
    // tm_wday: 0=domingo, 1=lunes, ..., 6=sábado
    int dia_semana = timeinfo.tm_wday;
    if (dia_semana >= 0 && dia_semana < 7) {
        strncpy(dia_buffer, dias_semana[dia_semana], buffer_size - 1);
        dia_buffer[buffer_size - 1] = '\0';
        return true;
    }
    
    return false;
}

/**
 * Obtiene la hora actual en formato HH:MM
 */
bool obtener_hora_actual(char *hora_buffer, size_t buffer_size) {
    if (!hora_sincronizada()) {
        ESP_LOGW(TAG, "Hora no sincronizada, no se puede obtener la hora");
        return false;
    }
    
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    
    // Formatear hora como HH:MM
    int len = snprintf(hora_buffer, buffer_size, "%02d:%02d", 
                       timeinfo.tm_hour, timeinfo.tm_min);
    
    return (len > 0 && len < (int)buffer_size);
}

/**
 * Establece la hora manualmente (útil cuando SNTP no puede sincronizar)
 * Útil cuando el ESP32 está en modo SoftAP sin conexión a Internet
 */
bool establecer_hora_manual(int year, int month, int day, int hour, int minute, int second) {
    struct tm timeinfo = {0};
    timeinfo.tm_year = year - 1900;  // tm_year es años desde 1900
    timeinfo.tm_mon = month - 1;      // tm_mon es 0-11
    timeinfo.tm_mday = day;
    timeinfo.tm_hour = hour;
    timeinfo.tm_min = minute;
    timeinfo.tm_sec = second;
    
    time_t t = mktime(&timeinfo);
    if (t == -1) {
        ESP_LOGE(TAG, "Error al convertir fecha/hora manual");
        return false;
    }
    
    struct timeval tv = {
        .tv_sec = t,
        .tv_usec = 0
    };
    
    if (settimeofday(&tv, NULL) != 0) {
        ESP_LOGE(TAG, "Error al establecer hora manual");
        return false;
    }
    
    time_synced = true;  // Marcar como sincronizado
    
    // Guardar la hora establecida manualmente en NVS
    guardar_hora_en_nvs(t);
    
    ESP_LOGI(TAG, "Hora establecida manualmente: %04d-%02d-%02d %02d:%02d:%02d",
             year, month, day, hour, minute, second);
    
    return true;
}

