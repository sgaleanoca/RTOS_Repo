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
#include <esp_spiffs.h>
#include <esp_log.h>

// FreeRTOS
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Headers locales (ordenados alfabéticamente)
#include "api_server.h"
#include "gpio_driver.h"
#include "iot_client.h"
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

    // ===== SECCIÓN 1.5: INICIALIZACIÓN DE SPIFFS =====
    // Montar SPIFFS para almacenamiento persistente de registros
    static const char *TAG_SPIFFS = "SPIFFS";
    ESP_LOGI(TAG_SPIFFS, "Inicializando SPIFFS...");
    
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "storage",
        .max_files = 10,
        .format_if_mount_failed = true
    };
    
    ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG_SPIFFS, "Error montando o formateando SPIFFS");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG_SPIFFS, "Partición SPIFFS no encontrada");
        } else {
            ESP_LOGE(TAG_SPIFFS, "Error inicializando SPIFFS (%s)", esp_err_to_name(ret));
        }
        return;
    }
    
    size_t total = 0, used = 0;
    ret = esp_spiffs_info("storage", &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG_SPIFFS, "Error obteniendo información de SPIFFS (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG_SPIFFS, "SPIFFS montado. Total=%d bytes, Usado=%d bytes", total, used);
    }

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
    
    // Esperar un poco para que WiFi se estabilice
    vTaskDelay(pdMS_TO_TICKS(2000));

    // ===== SECCIÓN 3.5: INICIALIZACIÓN DEL CLIENTE IoT =====
    // Inicializar cliente IoT para enviar datos al servidor Flask
    // Nota: Ajusta la IP según tu configuración de red
    // Si el servidor Flask está en la Raspberry Pi conectada al ESP32, usa la IP del gateway
    // Por defecto, el ESP32 en modo AP tiene IP 192.168.4.1, el gateway suele ser 192.168.4.1
    // Si la Raspberry Pi está conectada al ESP32, su IP será algo como 192.168.4.2
    // Si el ESP32 está conectado a otra red WiFi, usa la IP de la Raspberry Pi en esa red
    iot_client_init("http://192.168.4.2:5000");  // Ajusta según tu configuración

    // ===== SECCIÓN 4: INICIO DEL SERVIDOR API =====
    // Inicia el servidor API REST que expone endpoints para controlar el hardware
    // No sirve HTML/CSS/JS (eso lo hace Flask en Raspberry Pi)
    // Solo expone API REST: /api/temperature, /api/time, /api/logs, /api/terminal
    start_api_server();
}