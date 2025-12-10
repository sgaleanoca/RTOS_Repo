/**
 * @file wifi_app.c
 * @brief Implementación del módulo de WiFi en modo AP+STA
 * @author Proyecto Final RTOS
 * @date 2024
 * 
 * @details Implementación del módulo de WiFi en modo AP+STA (Access Point + Station).
 * Este módulo configura el ESP32 para funcionar simultáneamente como:
 * - Access Point (SoftAP): Crea su propia red WiFi para acceso local
 * - Station (STA): Se conecta a una red WiFi externa para tener Internet
 * 
 * @section functionality Funcionalidades
 * - Inicializa la pila de red TCP/IP
 * - Configura el ESP32 como punto de acceso WiFi (red local)
 * - Se conecta a una red WiFi externa para tener Internet (SNTP, etc.)
 * - Maneja eventos de conexión/desconexión de clientes y estaciones
 * - Reintentos automáticos de conexión a la red externa
 * 
 * @section usage Uso
 * Los usuarios pueden conectarse a la red "ESP32_Server" y acceder al
 * servidor web para controlar el sistema. El ESP32 también se conecta
 * a la red "Mondongo" para tener acceso a Internet (SNTP, etc.).
 * 
 * ============================================================================
 * ARCHIVO: wifi_app.c
 * ============================================================================
 * 
 * RESUMEN:
 * Implementación del módulo de WiFi en modo AP+STA (Access Point + Station).
 * Este módulo configura el ESP32 para funcionar simultáneamente como:
 * - Access Point (SoftAP): Crea su propia red WiFi para acceso local
 * - Station (STA): Se conecta a una red WiFi externa para tener Internet
 * 
 * Funcionalidades:
 * - Inicializa la pila de red TCP/IP
 * - Configura el ESP32 como punto de acceso WiFi (red local)
 * - Se conecta a una red WiFi externa para tener Internet (SNTP, etc.)
 * - Maneja eventos de conexión/desconexión de clientes y estaciones
 * - Reintentos automáticos de conexión a la red externa
 * 
 * Los usuarios pueden conectarse a la red "ESP32_Server" y acceder al
 * servidor web para controlar el sistema. El ESP32 también se conecta
 * a la red "Mondongo" para tener acceso a Internet (SNTP, etc.).
 * 
 * ============================================================================
 * ÍNDICE DE SECCIONES:
 * ============================================================================
 * Sección 1: INCLUDES se encuentra en las líneas 36 a 60
 * Sección 2: DEFINICIONES Y CONSTANTES se encuentra en las líneas 62 a 64
 * Sección 3: MANEJO DE EVENTOS WIFI se encuentra en las líneas 66 a 103
 * Sección 4: INICIALIZACIÓN DE SOFTAP se encuentra en las líneas 105 a 197
 * Sección 5: FUNCIONES DE UTILIDAD se encuentra en las líneas 199 a 205
 * ============================================================================
 * 
 * ============================================================================
 * RESUMEN DE TAREAS, COLAS Y SEMÁFOROS IMPLEMENTADOS:
 * ============================================================================
 * 
 * === TAREAS (TASKS) ===
 * 
 * Ninguna en este módulo. WiFi funciona mediante eventos y callbacks.
 * 
 * === COLAS (QUEUES) ===
 * 
 * Ninguna en este módulo.
 * 
 * === SEMÁFOROS (MUTEXES) ===
 * 
 * Ninguno en este módulo. El acceso a variables globales es thread-safe
 * porque solo se modifican desde el handler de eventos que se ejecuta
 * en el contexto del bucle de eventos.
 * 
 * ============================================================================
 */

// ===== INCLUDES =====
// Header local
#include "wifi_app.h"

// ESP-IDF
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_netif_ip_addr.h"

// FreeRTOS
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// LWIP (red)
#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "esp_netif.h"
#include "esp_netif_types.h"

// Estándar C
#include <string.h>

// ===== DEFINICIONES Y CONSTANTES =====
static const char *TAG = "WIFI_APP";
static bool wifi_connected = false;
static int s_retry_num = 0;

// ===== SECCIÓN: MANEJO DE EVENTOS WIFI =====
/**
 * Manejador de eventos WiFi para logging de conexiones/desconexiones
 * Registra cuando los clientes se conectan o desconectan del Access Point
 * y maneja la conexión del ESP32 como Station a la red WiFi externa
 * 
 * @param arg: Argumento del handler (no usado)
 * @param event_base: Base del evento (WIFI_EVENT o IP_EVENT)
 * @param event_id: ID del evento específico
 * @param event_data: Datos del evento
 */
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    // Eventos del Access Point (SoftAP)
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "Estación "MACSTR" se unió al AP, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "Estación "MACSTR" se desconectó del AP, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
    // Eventos del Station (conexión a red WiFi externa)
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "Modo Station iniciado, conectando a %s...", WIFI_STA_SSID);
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_connected = false;
        if (s_retry_num < WIFI_MAX_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Reintentando conexión a %s... (%d/%d)", WIFI_STA_SSID, s_retry_num, WIFI_MAX_RETRY);
        } else {
            ESP_LOGW(TAG, "No se pudo conectar a %s después de %d intentos", WIFI_STA_SSID, WIFI_MAX_RETRY);
        }
    }
    // Eventos de IP (cuando se obtiene dirección IP)
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "✓ Conectado a %s", WIFI_STA_SSID);
        ESP_LOGI(TAG, "✓ Dirección IP obtenida: " IPSTR, IP2STR(&event->ip_info.ip));
        wifi_connected = true;
        s_retry_num = 0;
    }
}

// ===== SECCIÓN: INICIALIZACIÓN DE SOFTAP =====
/**
 * Inicializa el ESP32 en modo AP+STA (Access Point + Station)
 * - Crea su propia red WiFi (SoftAP) para acceso local
 * - Se conecta a una red WiFi externa (Station) para tener Internet
 * 
 * Proceso de inicialización:
 * 1. Inicializa la pila TCP/IP
 * 2. Crea el bucle de eventos
 * 3. Crea las interfaces de red WiFi (AP y STA)
 * 4. Inicializa el driver WiFi
 * 5. Registra los manejadores de eventos (WiFi e IP)
 * 6. Configura credenciales del Access Point
 * 7. Configura credenciales del Station (red externa)
 * 8. Aplica configuración y inicia en modo AP+STA
 */
void wifi_init_softap(void)
{
    // Paso 1: Inicializar la pila TCP/IP subyacente (necesaria para WiFi)
    ESP_ERROR_CHECK(esp_netif_init());
    
    // Paso 2: Crear el bucle de eventos por defecto para manejar eventos WiFi
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // Paso 3: Crear las interfaces de red WiFi (AP y STA)
    esp_netif_create_default_wifi_ap();  // Access Point
    esp_netif_create_default_wifi_sta();  // Station

    // Paso 4: Configuración inicial del driver WiFi con valores por defecto
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Paso 5: Registrar los manejadores de eventos
    // Eventos WiFi (conexión/desconexión)
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    // Eventos IP (cuando se obtiene dirección IP)
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    // Paso 6: Configurar credenciales del Access Point (red local)
    wifi_config_t wifi_ap_config = {
        .ap = {
            .ssid = ESP_WIFI_SSID,
            .ssid_len = strlen(ESP_WIFI_SSID),
            .channel = ESP_WIFI_CHANNEL,
            .password = ESP_WIFI_PASS,
            .max_connection = MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK, // WPA2 Personal (seguridad)
            .pmf_cfg = {
                .required = false,
                .capable = true,
            },
        },
    };

    // Si no hay contraseña, usar modo abierto (sin seguridad)
    if (strlen(ESP_WIFI_PASS) == 0) {
        wifi_ap_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    // Paso 7: Configurar credenciales del Station (red externa para Internet)
    wifi_config_t wifi_sta_config = {
        .sta = {
            .ssid = WIFI_STA_SSID,
            .password = WIFI_STA_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK, // WPA2 mínimo
            .pmf_cfg = {
                .capable = true,
                .required = false,
            },
        },
    };

    // Paso 8: Aplicar configuración al driver WiFi en modo AP+STA
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_ap_config));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_sta_config));
    
    // Paso 9: Iniciar el driver WiFi (inicia AP y STA)
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi iniciado en modo AP+STA");
    ESP_LOGI(TAG, "  - Access Point: SSID=%s, Clave=%s, Canal=%d", 
             ESP_WIFI_SSID, ESP_WIFI_PASS, ESP_WIFI_CHANNEL);
    ESP_LOGI(TAG, "  - Station: Conectando a %s...", WIFI_STA_SSID);
}

/**
 * Verifica si el ESP32 está conectado a Internet (modo Station)
 * @return true si está conectado, false en caso contrario
 */
bool wifi_is_connected(void) {
    return wifi_connected;
}