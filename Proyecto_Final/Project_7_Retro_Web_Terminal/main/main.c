/**
 * ============================================================================
 * ARCHIVO: main.c
 * ============================================================================
 * 
 * RESUMEN:
 * Este es el punto de entrada principal de la aplicación ESP32. Se encarga de
 * inicializar todos los componentes del sistema en el orden correcto:
 * - Sistema de almacenamiento no volátil (NVS) para WiFi
 * - Controladores de hardware (GPIO para LEDs, sensor NTC de temperatura)
 * - Configuración de WiFi en modo SoftAP (Access Point)
 * - Servidor API REST para exponer endpoints de control de hardware
 * 
 * Arquitectura de dos capas:
 * - Frontend + servidor Flask en Raspberry Pi (sirve HTML/CSS/JS)
 * - ESP32 manejando solo lógica de hardware + API liviana (endpoints REST)
 * 
 * ============================================================================
 * ÍNDICE DE SECCIONES:
 * ============================================================================
 * Sección 1: INCLUDES se encuentra en las líneas 19 a 27
 * Sección 2: FUNCIÓN PRINCIPAL (app_main) se encuentra en las líneas 29 a 72
 *   - Subsección 2.1: INICIALIZACIÓN DE NVS se encuentra en las líneas 43 a 51
 *   - Subsección 2.2: INICIALIZACIÓN DE HARDWARE se encuentra en las líneas 53 a 61
 *   - Subsección 2.3: INICIALIZACIÓN DE RED se encuentra en las líneas 63 a 66
 *   - Subsección 2.4: INICIO DEL SERVIDOR API se encuentra en las líneas 80 a 84
 * ============================================================================
 */

// ===== INCLUDES =====
// ESP-IDF
#include <nvs_flash.h>

// Headers locales (ordenados alfabéticamente)
#include "api_server.h"
#include "gpio_driver.h"
#include "ntc_sensor.h"
#include "wifi_app.h"

// ===== FUNCIÓN PRINCIPAL =====
/**
 * Función principal de la aplicación ESP32
 * Se ejecuta automáticamente al iniciar el dispositivo
 * 
 * Orden de inicialización:
 * 1. NVS (almacenamiento no volátil para configuración WiFi)
 * 2. Hardware (GPIO para LEDs y sensor NTC de temperatura)
 * 3. WiFi (configuración como Access Point)
 * 4. Servidor API REST (endpoints REST para controlar hardware)
 */
void app_main(void) {
    esp_err_t ret;
    
    // ===== SECCIÓN 1: INICIALIZACIÓN DE NVS =====
    // NVS (Non-Volatile Storage) es necesario para almacenar configuración WiFi
    // Si hay problemas con la partición NVS, se borra y se reinicializa
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // ===== SECCIÓN 2: INICIALIZACIÓN DE HARDWARE =====
    // Configurar GPIO para control de LEDs (amarillo y azul)
    gpio_init_leds();
    
    // Inicializar sensor NTC de temperatura (ADC1, GPIO32)
    ntc_sensor_init();
    
    // Iniciar tarea de FreeRTOS para leer temperatura periódicamente
    ntc_start_reading_task();

    // ===== SECCIÓN 3: INICIALIZACIÓN DE RED =====
    // Configurar ESP32 como Access Point (SoftAP)
    // Crea una red WiFi propia: SSID "ESP32_Server", contraseña "12345678"
    wifi_init_softap(); 

    // ===== SECCIÓN 4: INICIO DEL SERVIDOR API =====
    // Inicia el servidor API REST que expone endpoints para controlar el hardware
    // No sirve HTML/CSS/JS (eso lo hace Flask en Raspberry Pi)
    // Solo expone API REST: /api/temperature, /api/time, /api/logs, /api/terminal
    start_api_server();
}