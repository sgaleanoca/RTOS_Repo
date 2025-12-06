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
 * - Servidor web HTTP para la interfaz de usuario
 * 
 * El sistema crea una red WiFi propia a la que los usuarios pueden conectarse
 * y acceder a una terminal web retro para controlar LEDs y monitorear temperatura.
 * ============================================================================
 */

// ===== INCLUDES =====
#include <nvs_flash.h>
#include "wifi_app.h"
#include "web_server.h"
#include "gpio_driver.h"
#include "ntc_sensor.h"

// ===== FUNCIÓN PRINCIPAL =====
/**
 * Función principal de la aplicación ESP32
 * Se ejecuta automáticamente al iniciar el dispositivo
 */
void app_main(void) {
    // ===== SECCIÓN 1: INICIALIZACIÓN DE NVS =====
    // NVS (Non-Volatile Storage) es necesario para almacenar configuración WiFi
    // Si hay problemas con la partición NVS, se borra y se reinicializa
    esp_err_t ret = nvs_flash_init();
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

    // ===== SECCIÓN 3: INICIALIZACIÓN DE WIFI =====
    // Configurar ESP32 como Access Point (SoftAP)
    // Crea una red WiFi propia: SSID "ESP32_Server", contraseña "12345678"
    wifi_init_softap(); 

    // ===== SECCIÓN 4: INICIO DEL SERVIDOR WEB =====
    // Inicia el servidor HTTP que sirve las páginas web y maneja las peticiones
    // Monta SPIFFS, registra rutas y handlers para login, terminal, dashboard, etc.
    start_webserver();
}