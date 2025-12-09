/**
 * ============================================================================
 * ARCHIVO: wifi_app.c
 * ============================================================================
 * 
 * RESUMEN:
 * Implementación del módulo de WiFi en modo SoftAP (Access Point). Este módulo:
 * - Inicializa la pila de red TCP/IP
 * - Configura el ESP32 como punto de acceso WiFi
 * - Crea una red WiFi con las credenciales definidas
 * - Maneja eventos de conexión/desconexión de clientes
 * 
 * Los usuarios pueden conectarse a la red "ESP32_Server" y acceder al
 * servidor web para controlar el sistema.
 * 
 * ============================================================================
 * ÍNDICE DE SECCIONES:
 * ============================================================================
 * Sección 1: INCLUDES se encuentra en las líneas 18 a 38
 * Sección 2: DEFINICIONES Y CONSTANTES se encuentra en las líneas 40 a 41
 * Sección 3: MANEJO DE EVENTOS WIFI se encuentra en las líneas 43 a 66
 * Sección 4: INICIALIZACIÓN DE SOFTAP se encuentra en las líneas 68 a 134
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

// FreeRTOS
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// LWIP (red)
#include "lwip/err.h"
#include "lwip/sys.h"

// Estándar C
#include <string.h>

// ===== DEFINICIONES Y CONSTANTES =====
static const char *TAG = "WIFI_APP";

// ===== SECCIÓN: MANEJO DE EVENTOS WIFI =====
/**
 * Manejador de eventos WiFi para logging de conexiones/desconexiones
 * Registra cuando los clientes se conectan o desconectan del Access Point
 * Útil para debugging y monitoreo de clientes conectados
 * 
 * @param arg: Argumento del handler (no usado)
 * @param event_base: Base del evento (WIFI_EVENT)
 * @param event_id: ID del evento específico
 * @param event_data: Datos del evento
 */
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "Estación "MACSTR" se unió, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "Estación "MACSTR" se desconectó, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
}

// ===== SECCIÓN: INICIALIZACIÓN DE SOFTAP =====
/**
 * Inicializa el ESP32 como Access Point (SoftAP)
 * Configura la red WiFi y permite que los clientes se conecten
 * 
 * Proceso de inicialización:
 * 1. Inicializa la pila TCP/IP
 * 2. Crea el bucle de eventos
 * 3. Crea la interfaz de red WiFi en modo AP
 * 4. Inicializa el driver WiFi
 * 5. Registra el manejador de eventos
 * 6. Configura credenciales y seguridad
 * 7. Aplica configuración y inicia el AP
 */
void wifi_init_softap(void)
{
    // Paso 1: Inicializar la pila TCP/IP subyacente (necesaria para WiFi)
    ESP_ERROR_CHECK(esp_netif_init());
    
    // Paso 2: Crear el bucle de eventos por defecto para manejar eventos WiFi
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // Paso 3: Crear la interfaz de red WiFi en modo Access Point
    esp_netif_create_default_wifi_ap();

    // Paso 4: Configuración inicial del driver WiFi con valores por defecto
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Paso 5: Registrar el manejador de eventos para logging de conexiones
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    // Paso 6: Configurar credenciales y modo de seguridad de la red
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = ESP_WIFI_SSID,
            .ssid_len = strlen(ESP_WIFI_SSID),
            .channel = ESP_WIFI_CHANNEL,
            .password = ESP_WIFI_PASS,
            .max_connection = MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK, // WPA2 Personal (seguridad)
            .pmf_cfg = {
                .required = false, // PMF no puede ser requerido con WPA_WPA2_PSK
                .capable = true,   // Pero sí puede ser capaz
            },
        },
    };

    // Si no hay contraseña, usar modo abierto (sin seguridad)
    if (strlen(ESP_WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    // Paso 7: Aplicar configuración al driver WiFi
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    
    // Paso 8: Iniciar el driver WiFi y activar el Access Point
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "SoftAP iniciado. SSID: %s, Clave: %s, Canal: %d, Max conexiones: %d",
             ESP_WIFI_SSID, ESP_WIFI_PASS, ESP_WIFI_CHANNEL, MAX_STA_CONN);
}