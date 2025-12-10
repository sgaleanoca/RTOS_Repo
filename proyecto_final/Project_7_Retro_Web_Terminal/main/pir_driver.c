/**
 * @file pir_driver.c
 * @brief Implementación del driver para sensor PIR
 * @author Proyecto Final RTOS
 * @date 2024
 * 
 * @details Implementación del driver del sensor PIR (Passive Infrared Sensor).
 * Este módulo gestiona la detección de movimiento mediante interrupciones GPIO.
 * 
 * @section hardware Hardware
 * - Sensor PIR: GPIO 12 (configurable mediante pir_init())
 * - El sensor PIR detecta movimiento mediante cambios en radiación infrarroja
 * 
 * @section features Características
 * - Detección de movimiento mediante interrupciones GPIO
 * - Soporte para cola de eventos opcional para notificaciones asíncronas
 * - Lectura síncrona del estado actual del sensor
 * - ISR (Interrupt Service Routine) thread-safe usando colas desde ISR
 * - Configuración automática de pull-up/pull-down (deshabilitados, el módulo PIR ya los tiene)
 * 
 * @section architecture Arquitectura
 * - Interrupciones GPIO en ambos flancos (subida y bajada) para detectar cambios
 * - ISR envía eventos a cola de FreeRTOS (si está configurada) usando xQueueSendFromISR()
 * - Función síncrona para leer estado actual sin esperar eventos
 * - El ventilador utiliza este sensor para verificar presencia antes de activarse
 * 
 * @section usage Uso en el sistema
 * - El ventilador verifica presencia mediante pir_is_motion_active()
 * - En modo MANUAL, el ventilador ignora el PIR
 * - En otros modos, el ventilador solo funciona si hay presencia detectada
 * 
 * ============================================================================
 * ARCHIVO: pir_driver.c
 * ============================================================================
 * 
 * RESUMEN:
 * Implementación del driver del sensor PIR (Passive Infrared Sensor).
 * Este módulo gestiona la detección de movimiento mediante interrupciones GPIO.
 * 
 * Hardware:
 * - Sensor PIR: GPIO 12 (configurable mediante pir_init())
 * - El sensor PIR detecta movimiento mediante cambios en radiación infrarroja
 * 
 * Características:
 * - Detección de movimiento mediante interrupciones GPIO
 * - Soporte para cola de eventos opcional para notificaciones asíncronas
 * - Lectura síncrona del estado actual del sensor
 * - ISR (Interrupt Service Routine) thread-safe usando colas desde ISR
 * - Configuración automática de pull-up/pull-down (deshabilitados, el módulo PIR ya los tiene)
 * 
 * Arquitectura:
 * - Interrupciones GPIO en ambos flancos (subida y bajada) para detectar cambios
 * - ISR envía eventos a cola de FreeRTOS (si está configurada) usando xQueueSendFromISR()
 * - Función síncrona para leer estado actual sin esperar eventos
 * - El ventilador utiliza este sensor para verificar presencia antes de activarse
 * 
 * Uso en el sistema:
 * - El ventilador verifica presencia mediante pir_is_motion_active()
 * - En modo MANUAL, el ventilador ignora el PIR
 * - En otros modos, el ventilador solo funciona si hay presencia detectada
 * 
 * ============================================================================
 * ÍNDICE DE SECCIONES:
 * ============================================================================
 * Sección 1: INCLUDES se encuentra en las líneas 37 a 47
 * Sección 2: DEFINICIONES Y CONSTANTES se encuentra en las líneas 49 a 50
 * Sección 3: VARIABLES ESTÁTICAS se encuentra en las líneas 52 a 55
 * Sección 4: FUNCIONES INTERNAS se encuentra en las líneas 57 a 94
 * Sección 5: FUNCIONES PÚBLICAS se encuentra en las líneas 96 a 219
 * ============================================================================
 * 
 * ============================================================================
 * RESUMEN DE TAREAS, COLAS Y SEMÁFOROS IMPLEMENTADOS:
 * ============================================================================
 * 
 * === TAREAS (TASKS) ===
 * 
 * Ninguna en este módulo. Las funciones se llaman desde otras tareas.
 * 
 * === COLAS (QUEUES) ===
 * 
 * 1. s_pir_evt_queue (opcional, configurada externamente)
 *    - Tipo: QueueHandle_t (FreeRTOS queue)
 *    - Tamaño: Configurado externamente
 *    - Elemento: pir_event_t (estructura con campo motion)
 *    - Dirección: ISR → Tarea externa
 *    - Operación: xQueueSendFromISR() desde ISR
 *    - Uso: Notificaciones asíncronas de cambios de movimiento (opcional)
 * 
 * === SEMÁFOROS (MUTEXES) ===
 * 
 * Ninguno en este módulo. gpio_get_level() es thread-safe.
 * 
 * ============================================================================
 */

// ===== INCLUDES =====
#include "pir_driver.h"

// ESP-IDF
#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_log.h"

// FreeRTOS
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// ===== DEFINICIONES Y CONSTANTES =====
static const char *TAG = "PIR_DRIVER";

// ===== VARIABLES ESTÁTICAS =====
static gpio_num_t s_pir_gpio = GPIO_NUM_NC;      // Pin GPIO configurado (NC = No Configurado)
static QueueHandle_t s_pir_evt_queue = NULL;    // Cola de eventos opcional
static bool s_pir_initialized = false;          // Flag de inicialización

// ===== FUNCIONES INTERNAS =====
/**
 * @brief ISR (Interrupt Service Routine) del sensor PIR
 * 
 * Esta función se ejecuta cuando se detecta un cambio en el pin GPIO del sensor PIR
 * (flanco de subida o bajada). Lee el nivel actual del GPIO y envía un evento
 * a la cola de FreeRTOS si está configurada.
 * 
 * Características importantes:
 * - Debe estar marcada con IRAM_ATTR para ejecutarse desde IRAM (memoria rápida)
 * - Usa xQueueSendFromISR() para enviar eventos de forma thread-safe desde ISR
 * - Hace yield si una tarea de mayor prioridad fue despertada
 * 
 * @param arg Argumento pasado al registrar el ISR (no usado)
 */
static void IRAM_ATTR pir_isr_handler(void *arg)
{
    // Si no hay cola configurada, no hacer nada
    if (s_pir_evt_queue == NULL) {
        return;
    }

    // Leer nivel actual del GPIO
    // Nota: gpio_get_level() es seguro desde ISR si el pin está configurado correctamente
    int level = gpio_get_level(s_pir_gpio);

    // Crear evento con el estado detectado
    pir_event_t evt = {
        .motion = (level == 1)  // true si nivel alto (movimiento detectado)
    };

    // Enviar evento a la cola desde ISR
    // xQueueSendFromISR() es thread-safe y puede ser llamado desde ISR
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    BaseType_t result = xQueueSendFromISR(s_pir_evt_queue, &evt, &xHigherPriorityTaskWoken);

    if (result != pdTRUE) {
        // La cola está llena, el evento se perdió
        // En producción, podrías querer incrementar el tamaño de la cola
        // o usar xQueueSendFromISR() con overwrite
    }

    // Si una tarea de mayor prioridad fue despertada, hacer yield
    // Esto permite que la tarea procese el evento inmediatamente
    if (xHigherPriorityTaskWoken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

// ===== FUNCIONES PÚBLICAS =====
/**
 * @brief Inicializa el driver del sensor PIR
 * 
 * Configura el pin GPIO como entrada con interrupciones en ambos flancos
 * (subida y bajada) para detectar cambios en el estado del sensor.
 * 
 * Si se proporciona una cola de eventos, los eventos de movimiento se enviarán
 * automáticamente a través de la cola cuando se detecten cambios. Esto permite
 * procesamiento asíncrono de eventos de movimiento en una tarea separada.
 * 
 * Proceso de inicialización:
 * 1. Valida que el pin GPIO sea válido
 * 2. Configura el pin GPIO como entrada sin pull-up/pull-down
 * 3. Configura interrupciones en ambos flancos (GPIO_INTR_ANYEDGE)
 * 4. Instala el servicio de ISR de GPIO (si no está instalado)
 * 5. Registra el handler de interrupción para el pin
 * 
 * @param pir_gpio Número del pin GPIO donde está conectado el sensor PIR
 * @param pir_queue Cola de eventos donde se enviarán las notificaciones de movimiento.
 *                  Puede ser NULL si no se necesita notificación asíncrona.
 *                  La cola debe ser creada previamente con xQueueCreate().
 * 
 * @return true si la inicialización fue exitosa, false en caso contrario
 * 
 * @note La cola de eventos debe ser creada antes de llamar a esta función.
 *       Ejemplo: QueueHandle_t pir_queue = xQueueCreate(10, sizeof(pir_event_t));
 * 
 * @note Muchos módulos PIR ya incluyen electrónica interna (pull-up, filtrado),
 *       por lo que no se configuran pull-up/pull-down en el GPIO.
 */
bool pir_init(gpio_num_t pir_gpio, QueueHandle_t pir_queue)
{
    if (s_pir_initialized) {
        ESP_LOGW(TAG, "PIR driver ya está inicializado");
        return true;
    }

    // Validar pin GPIO
    if (pir_gpio < GPIO_NUM_0 || pir_gpio >= GPIO_NUM_MAX) {
        ESP_LOGE(TAG, "Pin GPIO inválido: %d", pir_gpio);
        return false;
    }

    s_pir_gpio = pir_gpio;
    s_pir_evt_queue = pir_queue;

    ESP_LOGI(TAG, "Inicializando sensor PIR en GPIO %d...", pir_gpio);

    // Configuración del pin como entrada
    gpio_config_t io_conf = {
        .pin_bit_mask  = 1ULL << s_pir_gpio,      // Máscara de bits para el pin
        .mode          = GPIO_MODE_INPUT,         // Modo entrada
        .pull_up_en    = GPIO_PULLUP_DISABLE,    // Sin pull-up (módulo PIR ya lo tiene)
        .pull_down_en  = GPIO_PULLDOWN_DISABLE,  // Sin pull-down (módulo PIR ya lo tiene)
        .intr_type     = GPIO_INTR_ANYEDGE       // Interrupción en ambos flancos
        // Nota: Si solo quieres notificar cuando hay movimiento:
        // .intr_type  = GPIO_INTR_POSEDGE;
    };

    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error configurando GPIO %d: %s (0x%x)", pir_gpio, esp_err_to_name(err), err);
        return false;
    }

    // Instalar servicio de ISR (si aún no se ha instalado)
    // gpio_install_isr_service(0) instala el servicio con flags=0 (por defecto)
    // Si ya está instalado, esta función retorna ESP_OK sin hacer nada
    err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        // ESP_ERR_INVALID_STATE significa que ya está instalado, lo cual está bien
        ESP_LOGE(TAG, "Error instalando servicio ISR: %s (0x%x)", esp_err_to_name(err), err);
        return false;
    }

    // Registrar ISR para este pin específico
    err = gpio_isr_handler_add(s_pir_gpio, pir_isr_handler, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error registrando ISR para GPIO %d: %s (0x%x)", 
                 pir_gpio, esp_err_to_name(err), err);
        return false;
    }

    s_pir_initialized = true;
    ESP_LOGI(TAG, "Sensor PIR inicializado correctamente en GPIO %d", pir_gpio);
    if (pir_queue != NULL) {
        ESP_LOGI(TAG, "Cola de eventos configurada para notificaciones asíncronas");
    } else {
        ESP_LOGI(TAG, "Cola de eventos no configurada (solo lectura síncrona disponible)");
    }

    return true;
}

/**
 * @brief Lee el estado actual del sensor PIR
 * 
 * Lee directamente el nivel del pin GPIO para determinar si hay movimiento
 * detectado. Esta función es síncrona y puede ser llamada desde cualquier tarea.
 * 
 * @return true si hay movimiento detectado (GPIO en nivel alto), 
 *         false si no hay movimiento (GPIO en nivel bajo) o si el driver no está inicializado
 * 
 * @note Esta función lee el estado actual del GPIO, no el último evento.
 *       Para recibir notificaciones de cambios, usar la cola de eventos en pir_init().
 * 
 * @note Esta función es thread-safe porque gpio_get_level() es thread-safe.
 */
bool pir_is_motion_active(void)
{
    // Verificar que el driver esté inicializado
    if (!s_pir_initialized || s_pir_gpio == GPIO_NUM_NC) {
        ESP_LOGW(TAG, "PIR driver no inicializado");
        return false;
    }

    // Leer nivel del GPIO
    // gpio_get_level() es thread-safe y puede ser llamado desde cualquier tarea
    int level = gpio_get_level(s_pir_gpio);
    
    // Retornar true si el nivel es alto (movimiento detectado)
    return (level == 1);
}