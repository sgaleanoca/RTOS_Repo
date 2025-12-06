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
 * ============================================================================
 */

#pragma once

// ===== CONFIGURACIÓN DE WIFI SOFTAP =====
// Credenciales de la red WiFi que crea el ESP32
#define ESP_WIFI_SSID      "ESP32_Server"  // Nombre de la red WiFi
#define ESP_WIFI_PASS      "12345678"      // Contraseña de la red (mínimo 8 caracteres)
#define ESP_WIFI_CHANNEL   1               // Canal WiFi (1-13)
#define MAX_STA_CONN       4                // Máximo número de dispositivos que pueden conectarse

// ===== PROTOTIPOS DE FUNCIONES =====
/**
 * Inicializa el ESP32 como Access Point (SoftAP)
 * Crea una red WiFi a la que los usuarios pueden conectarse
 */
void wifi_init_softap(void);