/**
 * ============================================================================
 * ARCHIVO: wifi_app.h
 * ============================================================================
 * 
 * RESUMEN:
 * Header file para la configuración y gestión de WiFi en modo SoftAP
 * (Access Point). Define las credenciales de la red WiFi que crea el ESP32
 * y la función para inicializar el punto de acceso.
 * 
 * Configuración:
 * - SSID: "ESP32_Server"
 * - Contraseña: "12345678"
 * - Canal: 1
 * - Máximo de estaciones conectadas: 4
 * 
 * ============================================================================
 * ÍNDICE DE SECCIONES:
 * ============================================================================
 * Sección 1: CONFIGURACIÓN DE WIFI SOFTAP se encuentra en las líneas 21 a 26
 * Sección 2: PROTOTIPOS DE FUNCIONES se encuentra en las líneas 28 a 40
 * ============================================================================
 */

#pragma once

#include <stdbool.h>

// ===== CONFIGURACIÓN DE WIFI SOFTAP =====
// Credenciales de la red WiFi que crea el ESP32
#define ESP_WIFI_SSID      "ESP32_Server"  // Nombre de la red WiFi (SSID)
#define ESP_WIFI_PASS      "12345678"      // Contraseña de la red (mínimo 8 caracteres para WPA2)
#define ESP_WIFI_CHANNEL   1               // Canal WiFi (rango: 1-13)
#define MAX_STA_CONN       4                // Máximo número de dispositivos que pueden conectarse simultáneamente

// ===== CONFIGURACIÓN DE WIFI STATION (STA) =====
// Credenciales de la red WiFi a la que se conectará el ESP32 para tener Internet
#define WIFI_STA_SSID      "Mondongo"       // Nombre de la red WiFi a la que conectarse
#define WIFI_STA_PASS      "huevos12"      // Contraseña de la red WiFi
#define WIFI_MAX_RETRY     5                // Número máximo de intentos de conexión

// ===== PROTOTIPOS DE FUNCIONES =====
/**
 * Inicializa el ESP32 en modo AP+STA (Access Point + Station)
 * - Crea su propia red WiFi (SoftAP) para acceso local
 * - Se conecta a una red WiFi externa (Station) para tener Internet
 * 
 * Proceso:
 * - Inicializa la pila TCP/IP
 * - Crea el bucle de eventos
 * - Configura el driver WiFi en modo AP+STA
 * - Inicia el Access Point con credenciales locales
 * - Se conecta a la red WiFi externa para Internet
 */
void wifi_init_softap(void);

/**
 * Verifica si el ESP32 está conectado a Internet (modo Station)
 * @return true si está conectado, false en caso contrario
 */
bool wifi_is_connected(void);