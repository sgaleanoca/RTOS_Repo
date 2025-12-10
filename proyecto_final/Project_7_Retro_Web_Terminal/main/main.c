/**
 * ============================================================================
 * ARCHIVO: main.c
 * ============================================================================
 * 
 * RESUMEN:
 * Este es el punto de entrada principal de la aplicación ESP32. Se encarga de
 * inicializar todos los componentes del sistema en el orden correcto:
 * - Sistema de almacenamiento no volátil (NVS) para WiFi
 * - Controladores de hardware (LED RGB, sensor NTC de temperatura)
 * - Configuración de WiFi en modo SoftAP (Access Point)
 * - Servidor web HTTP para la interfaz de usuario
 * 
 * El sistema crea una red WiFi propia a la que los usuarios pueden conectarse
 * y acceder a una terminal web retro para controlar LEDs y monitorear temperatura.
 * 
 * ============================================================================
 * ÍNDICE DE SECCIONES:
 * ============================================================================
 * Sección 1: INCLUDES se encuentra en las líneas 19 a 27
 * Sección 2: FUNCIÓN PRINCIPAL (app_main) se encuentra en las líneas 29 a 72
 *   - Subsección 2.1: INICIALIZACIÓN DE NVS se encuentra en las líneas 43 a 51
 *   - Subsección 2.2: INICIALIZACIÓN DE HARDWARE se encuentra en las líneas 53 a 61
 *   - Subsección 2.3: INICIALIZACIÓN DE RED se encuentra en las líneas 63 a 66
 *   - Subsección 2.4: INICIO DEL SERVIDOR WEB se encuentra en las líneas 68 a 71
 * ============================================================================
 */

// ===== INCLUDES =====
// ESP-IDF
#include <nvs_flash.h>
#include <esp_log.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Headers locales (ordenados alfabéticamente)
#include "fan_control.h"
#include "ntc_sensor.h"
#include "pir_driver.h"
#include "rgb_led.h"
#include "time_sync.h"
#include "web_server.h"
#include "wifi_app.h"

// ===== DEFINICIONES =====
static const char *TAG = "MAIN";

// ===== FUNCIÓN PRINCIPAL =====
/**
 * Función principal de la aplicación ESP32
 * Se ejecuta automáticamente al iniciar el dispositivo
 * 
 * Orden de inicialización:
 * 1. NVS (almacenamiento no volátil para configuración WiFi)
 * 2. Hardware (LED RGB y sensor NTC de temperatura)
 * 3. WiFi (configuración como Access Point)
 * 4. Servidor Web (HTTP con SPIFFS y rutas)
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
    // Inicializar LED RGB verde (GPIO 27, PWM mediante LEDC)
    if (!rgb_led_init()) {
        ESP_LOGE(TAG, "Error crítico al inicializar LED RGB");
        // Continuar ejecución aunque falle (puede ser problema de hardware)
    }
    
    // Inicializar sensor NTC de temperatura (ADC1, GPIO32)
    ntc_sensor_init();
    
    // Iniciar tarea de FreeRTOS para leer temperatura periódicamente
    ntc_start_reading_task();
    
    // Inicializar sensor PIR (GPIO12) - El ventilador solo funcionará si detecta presencia
    // No necesitamos cola de eventos para este uso, pasamos NULL
    if (!pir_init(PIR_GPIO_PIN, NULL)) {
        ESP_LOGE(TAG, "Error al inicializar sensor PIR");
        // Continuar ejecución aunque falle (el sistema puede funcionar sin PIR)
    } else {
        ESP_LOGI(TAG, "Sensor PIR inicializado correctamente en GPIO %d", PIR_GPIO_PIN);
    }
    
    // Inicializar control del ventilador (PWM en GPIO26)
    // NOTA: El ventilador solo se activará si el PIR detecta presencia
    fan_init();
    
    // Establecer modo inicial: temperatura (control automático por temperatura)
    // El ventilador se controlará automáticamente según la temperatura del sensor
    fan_set_mode(FAN_MODE_AUTO_TEMP);
    ESP_LOGI(TAG, "Ventilador iniciado en modo temperatura (modo por defecto)");
    
    // Iniciar tarea de control automático por temperatura
    // Esta tarea monitorea la temperatura y actualiza el ventilador automáticamente
    // cuando está en modo FAN_MODE_AUTO_TEMP
    fan_start_auto_temp_task();

    // ===== SECCIÓN 3: INICIALIZACIÓN DE RED =====
    // Configurar ESP32 en modo AP+STA (Access Point + Station)
    // - Crea una red WiFi propia: SSID "ESP32_Server", contraseña "12345678"
    // - Se conecta a la red WiFi "Mndongo" para tener Internet
    wifi_init_softap(); 
    
    // Esperar conexión WiFi para sincronizar hora
    ESP_LOGI(TAG, "Esperando conexión a Internet...");
    for (int retry = 0; retry < 30 && !wifi_is_connected(); retry++) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        if (retry % 5 == 4) ESP_LOGI(TAG, "Esperando conexión WiFi... (%d/30)", retry + 1);
    }
    
    if (wifi_is_connected()) {
        ESP_LOGI(TAG, "✓ Conectado a Internet, sincronizando hora...");
    } else {
        ESP_LOGW(TAG, "⚠ No se pudo conectar, intentando sincronizar hora de todas formas...");
    }
    time_sync_init();
    
    if (!hora_sincronizada()) {
        ESP_LOGW(TAG, "SNTP no sincronizó. Estableciendo hora por defecto (lunes 12:00).");
        establecer_hora_manual(2024, 12, 9, 12, 0, 0);
    }

    // ===== SECCIÓN 4: INICIO DEL SERVIDOR WEB =====
    // Inicia el servidor HTTP que sirve las páginas web y maneja las peticiones
    // Monta SPIFFS, registra rutas y handlers para login, terminal, dashboard, etc.
    start_webserver();
    
    // Iniciar tarea de control por horarios (registros)
    // Esta tarea verifica periódicamente los registros y activa el ventilador
    // cuando coincide el día y hora actual con algún registro
    fan_start_schedule_task();
}