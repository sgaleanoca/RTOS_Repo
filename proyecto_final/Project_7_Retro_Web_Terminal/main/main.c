/**
 * @file main.c
 * @brief Punto de entrada principal de la aplicación ESP32 - Terminal Web Retro
 * @author Proyecto Final RTOS
 * @date 2025
 * 
 * @details Este es el punto de entrada principal de la aplicación ESP32. Se encarga de
 * inicializar todos los componentes del sistema en el orden correcto:
 * - Sistema de almacenamiento no volátil (NVS) para WiFi
 * - Controladores de hardware (LED RGB, sensor NTC de temperatura, sensor PIR, ventilador)
 * - Configuración de WiFi en modo AP+STA (Access Point + Station)
 * - Sincronización de tiempo mediante SNTP
 * - Servidor web HTTP para la interfaz de usuario
 * 
 * El sistema crea una red WiFi propia a la que los usuarios pueden conectarse
 * y acceder a una terminal web retro para controlar LEDs, ventilador y monitorear
 * temperatura. También se conecta a una red WiFi externa para tener Internet
 * y sincronizar la hora mediante SNTP.
 * 
 * @section architecture Arquitectura del sistema
 * - Hardware: LED RGB (GPIO 27), Sensor NTC (GPIO 32), Sensor PIR (GPIO 12), Ventilador (GPIO 26)
 * - Red: WiFi AP+STA (red local + conexión a Internet)
 * - Servidor: HTTP con SPIFFS para archivos estáticos
 * - Control: Terminal web, API REST, control automático por temperatura y horarios
 * 
 * @section tasks Tareas FreeRTOS
 * Las tareas se crean en los módulos correspondientes:
 * - ntc_start_reading_task(): Tarea de lectura periódica del sensor NTC
 * - fan_start_auto_temp_task(): Tarea de control automático por temperatura
 * - fan_start_schedule_task(): Tarea de control por horarios (registros)
 * - start_webserver(): Crea tareas de procesamiento de comandos y gestión de sesiones
 * 
 * @section queues Colas FreeRTOS
 * Las colas se crean en web_server.c:
 * - gpio_command_queue: Para comandos desde handler HTTP a tarea de procesamiento
 * - gpio_response_queue: Para respuestas desde tarea de procesamiento a handler HTTP
 * 
 * @section mutexes Semáforos FreeRTOS
 * Los semáforos se crean en web_server.c:
 * - session_mutex: Protege acceso a sesiones de usuarios
 * - command_id_mutex: Protege contador de IDs de comandos
 * 
 * ============================================================================
 * ARCHIVO: main.c
 * ============================================================================
 * 
 * RESUMEN:
 * Este es el punto de entrada principal de la aplicación ESP32. Se encarga de
 * inicializar todos los componentes del sistema en el orden correcto:
 * - Sistema de almacenamiento no volátil (NVS) para WiFi
 * - Controladores de hardware (LED RGB, sensor NTC de temperatura, sensor PIR, ventilador)
 * - Configuración de WiFi en modo AP+STA (Access Point + Station)
 * - Sincronización de tiempo mediante SNTP
 * - Servidor web HTTP para la interfaz de usuario
 * 
 * El sistema crea una red WiFi propia a la que los usuarios pueden conectarse
 * y acceder a una terminal web retro para controlar LEDs, ventilador y monitorear
 * temperatura. También se conecta a una red WiFi externa para tener Internet
 * y sincronizar la hora mediante SNTP.
 * 
 * Arquitectura del sistema:
 * - Hardware: LED RGB (GPIO 27), Sensor NTC (GPIO 32), Sensor PIR (GPIO 12), Ventilador (GPIO 26)
 * - Red: WiFi AP+STA (red local + conexión a Internet)
 * - Servidor: HTTP con SPIFFS para archivos estáticos
 * - Control: Terminal web, API REST, control automático por temperatura y horarios
 * 
 * ============================================================================
 * ÍNDICE DE SECCIONES:
 * ============================================================================
 * Sección 1: INCLUDES se encuentra en las líneas 39 a 53
 * Sección 2: DEFINICIONES se encuentra en las líneas 55 a 56
 * Sección 3: FUNCIÓN PRINCIPAL (app_main) se encuentra en las líneas 58 a 142
 *   - Subsección 3.1: INICIALIZACIÓN DE NVS se encuentra en las líneas 72 a 80
 *   - Subsección 3.2: INICIALIZACIÓN DE HARDWARE se encuentra en las líneas 82 a 106
 *   - Subsección 3.3: INICIALIZACIÓN DE RED se encuentra en las líneas 108 a 131
 *   - Subsección 3.4: INICIO DEL SERVIDOR WEB se encuentra en las líneas 133 a 141
 * ============================================================================
 * 
 * ============================================================================
 * RESUMEN DE TAREAS, COLAS Y SEMÁFOROS IMPLEMENTADOS:
 * ============================================================================
 * 
 * === TAREAS (TASKS) ===
 * 
 * Las tareas se crean en los módulos correspondientes:
 * - ntc_start_reading_task(): Tarea de lectura periódica del sensor NTC
 * - fan_start_auto_temp_task(): Tarea de control automático por temperatura
 * - fan_start_schedule_task(): Tarea de control por horarios (registros)
 * - start_webserver(): Crea tareas de procesamiento de comandos y gestión de sesiones
 * 
 * === COLAS (QUEUES) ===
 * 
 * Las colas se crean en web_server.c:
 * - gpio_command_queue: Para comandos desde handler HTTP a tarea de procesamiento
 * - gpio_response_queue: Para respuestas desde tarea de procesamiento a handler HTTP
 * 
 * === SEMÁFOROS (MUTEXES) ===
 * 
 * Los semáforos se crean en web_server.c:
 * - session_mutex: Protege acceso a sesiones de usuarios
 * - command_id_mutex: Protege contador de IDs de comandos
 * 
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
 * @brief Función principal de la aplicación ESP32
 * @details Se ejecuta automáticamente al iniciar el dispositivo. Inicializa todos
 * los componentes del sistema en el orden correcto.
 * 
 * @note Orden de inicialización:
 * 1. NVS (almacenamiento no volátil para configuración WiFi)
 * 2. Hardware (LED RGB y sensor NTC de temperatura)
 * 3. WiFi (configuración como Access Point)
 * 4. Servidor Web (HTTP con SPIFFS y rutas)
 * 
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
        
        // Iniciar control automático del LED basado en el sensor PIR
        // El LED se encenderá al 100% cuando detecte movimiento y se apagará cuando no haya movimiento
        rgb_led_start_pir_control();
        ESP_LOGI(TAG, "Control automático del LED por PIR iniciado");
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