# Guía: Control de Ventilador GPIO con Gestor de Registros NVS

Esta guía explica paso a paso cómo adaptar el gestor de registros NVS para controlar un ventilador mediante GPIO basado en el valor guardado en memoria.

## Concepto General

El sistema funcionará de la siguiente manera:
1. Se guarda un umbral de temperatura en NVS (ej: 25°C)
2. Se lee la temperatura actual (simulada o desde un sensor)
3. Si la temperatura > umbral → encender ventilador (GPIO HIGH)
4. Si la temperatura ≤ umbral → apagar ventilador (GPIO LOW)

## Paso 1: Configurar GPIO para el Ventilador

### 1.1 Agregar las dependencias necesarias

En `main/CMakeLists.txt`, asegúrate de incluir el driver de GPIO:

```cmake
idf_component_register(SRCS "hello_world_main.c" "nvs_manager.c"
                       PRIV_REQUIRES nvs_flash spi_flash driver
                       INCLUDE_DIRS "")
```

### 1.2 Crear un módulo de control de ventilador

Crea un nuevo archivo `main/fan_controller.h`:

```c
#ifndef FAN_CONTROLLER_H
#define FAN_CONTROLLER_H

#include <stdbool.h>
#include "esp_err.h"

// Pin GPIO para controlar el ventilador
#define FAN_GPIO_PIN 2  // Cambia este pin según tu hardware

/**
 * @brief Inicializa el GPIO del ventilador
 * 
 * Configura el pin GPIO como salida y lo deja en estado bajo (apagado).
 * 
 * @return esp_err_t 
 *         - ESP_OK si la inicialización fue exitosa
 */
esp_err_t fan_controller_init(void);

/**
 * @brief Enciende el ventilador
 * 
 * Establece el pin GPIO en nivel alto.
 * 
 * @return esp_err_t 
 *         - ESP_OK si la operación fue exitosa
 */
esp_err_t fan_controller_on(void);

/**
 * @brief Apaga el ventilador
 * 
 * Establece el pin GPIO en nivel bajo.
 * 
 * @return esp_err_t 
 *         - ESP_OK si la operación fue exitosa
 */
esp_err_t fan_controller_off(void);

/**
 * @brief Controla el ventilador basado en temperatura
 * 
 * Compara la temperatura actual con el umbral guardado en NVS.
 * Si temperatura > umbral → enciende el ventilador
 * Si temperatura ≤ umbral → apaga el ventilador
 * 
 * @param temperatura_actual Temperatura actual medida
 * @return esp_err_t 
 *         - ESP_OK si la operación fue exitosa
 */
esp_err_t fan_controller_update(int32_t temperatura_actual);

#endif // FAN_CONTROLLER_H
```

### 1.3 Implementar el controlador de ventilador

Crea `main/fan_controller.c`:

```c
#include "fan_controller.h"
#include "driver/gpio.h"
#include "nvs_manager.h"
#include "esp_log.h"

static const char* TAG = "FAN_CONTROLLER";
static bool fan_initialized = false;

esp_err_t fan_controller_init(void)
{
    // Configurar el pin GPIO como salida
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << FAN_GPIO_PIN),  // Máscara de bits para el pin
        .mode = GPIO_MODE_OUTPUT,                 // Modo salida
        .pull_up_en = GPIO_PULLUP_DISABLE,        // Sin pull-up
        .pull_down_en = GPIO_PULLDOWN_DISABLE,    // Sin pull-down
        .intr_type = GPIO_INTR_DISABLE            // Sin interrupciones
    };
    
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error configurando GPIO: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Inicializar el ventilador apagado
    gpio_set_level(FAN_GPIO_PIN, 0);
    fan_initialized = true;
    
    ESP_LOGI(TAG, "Controlador de ventilador inicializado en GPIO %d", FAN_GPIO_PIN);
    return ESP_OK;
}

esp_err_t fan_controller_on(void)
{
    if (!fan_initialized) {
        ESP_LOGE(TAG, "Controlador no inicializado");
        return ESP_ERR_INVALID_STATE;
    }
    
    gpio_set_level(FAN_GPIO_PIN, 1);
    ESP_LOGI(TAG, "Ventilador ENCENDIDO");
    return ESP_OK;
}

esp_err_t fan_controller_off(void)
{
    if (!fan_initialized) {
        ESP_LOGE(TAG, "Controlador no inicializado");
        return ESP_ERR_INVALID_STATE;
    }
    
    gpio_set_level(FAN_GPIO_PIN, 0);
    ESP_LOGI(TAG, "Ventilador APAGADO");
    return ESP_OK;
}

esp_err_t fan_controller_update(int32_t temperatura_actual)
{
    if (!fan_initialized) {
        ESP_LOGE(TAG, "Controlador no inicializado");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Leer el umbral desde NVS
    int32_t umbral;
    esp_err_t ret = nvs_read_registro(REGISTRO_FAN_KEY, &umbral);
    
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        // Si no existe el umbral, usar un valor por defecto
        ESP_LOGW(TAG, "Umbral no encontrado en NVS, usando valor por defecto: 25°C");
        umbral = 25;
        
        // Opcional: guardar el valor por defecto en NVS
        nvs_create_registro(REGISTRO_FAN_KEY, umbral);
    } else if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error leyendo umbral desde NVS: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Controlar el ventilador según la temperatura
    if (temperatura_actual > umbral) {
        fan_controller_on();
        ESP_LOGI(TAG, "Temperatura %ld°C > umbral %ld°C → Ventilador ON", 
                 (long)temperatura_actual, (long)umbral);
    } else {
        fan_controller_off();
        ESP_LOGI(TAG, "Temperatura %ld°C ≤ umbral %ld°C → Ventilador OFF", 
                 (long)temperatura_actual, (long)umbral);
    }
    
    return ESP_OK;
}
```

## Paso 2: Actualizar el main.c para usar el controlador

Modifica `main/hello_world_main.c` para integrar el control del ventilador:

```c
#include "fan_controller.h"
// ... otros includes ...

void app_main(void)
{
    // Inicializar NVS
    nvs_manager_init();
    
    // Inicializar controlador de ventilador
    fan_controller_init();
    
    // Configurar umbral inicial (ejemplo: 25°C)
    nvs_create_registro(REGISTRO_FAN_KEY, 25);
    
    // Simular lectura de temperatura y control del ventilador
    while (1) {
        // Simular temperatura (en un caso real, leerías de un sensor)
        int32_t temperatura_simulada = 28; // 28°C
        
        // Actualizar el ventilador según la temperatura
        fan_controller_update(temperatura_simulada);
        
        // Esperar antes de la siguiente lectura
        vTaskDelay(5000 / portTICK_PERIOD_MS); // 5 segundos
    }
}
```

## Paso 3: Actualizar CMakeLists.txt

Asegúrate de incluir el nuevo archivo en `main/CMakeLists.txt`:

```cmake
idf_component_register(SRCS "hello_world_main.c" "nvs_manager.c" "fan_controller.c"
                       PRIV_REQUIRES nvs_flash spi_flash driver
                       INCLUDE_DIRS "")
```

## Paso 4: Conexión Hardware

### Esquema de conexión básico:

```
ESP32 GPIO2 ──┬── Resistor 220Ω ──┬── Base del Transistor NPN
              │                    │
              │                    └── Emisor del Transistor ── GND
              │
              └── (Opcional: LED indicador)

Colector del Transistor ── Ventilador DC ── Fuente de alimentación externa (+)
```

**Nota importante**: 
- El GPIO del ESP32 solo puede proporcionar ~40mA máximo
- Para controlar un ventilador DC, necesitas usar un transistor (como 2N2222) o un relé
- El ventilador debe tener su propia fuente de alimentación externa

### Alternativa con relé:

```
ESP32 GPIO2 ── Módulo Relé ── Ventilador AC/DC
```

## Paso 5: Funcionalidades Avanzadas (Opcional)

### 5.1 Cambiar el umbral dinámicamente

Puedes agregar una función para cambiar el umbral en tiempo de ejecución:

```c
void cambiar_umbral_ventilador(int32_t nuevo_umbral)
{
    nvs_update_registro(REGISTRO_FAN_KEY, nuevo_umbral);
    ESP_LOGI("FAN", "Umbral actualizado a %ld°C", (long)nuevo_umbral);
}
```

### 5.2 Leer temperatura desde sensor (ejemplo con DS18B20)

```c
#include "ds18b20.h"  // Biblioteca para sensor de temperatura

int32_t leer_temperatura_sensor(void)
{
    float temp = ds18b20_get_temp();
    return (int32_t)temp;  // Convertir a entero
}

// En el loop principal:
int32_t temp_real = leer_temperatura_sensor();
fan_controller_update(temp_real);
```

### 5.3 Control con histéresis (evitar encendido/apagado constante)

```c
esp_err_t fan_controller_update_hysteresis(int32_t temperatura_actual)
{
    int32_t umbral;
    nvs_read_registro(REGISTRO_FAN_KEY, &umbral);
    
    static bool fan_on = false;
    
    // Histéresis: encender a umbral+2, apagar a umbral-2
    if (temperatura_actual > (umbral + 2) && !fan_on) {
        fan_controller_on();
        fan_on = true;
    } else if (temperatura_actual < (umbral - 2) && fan_on) {
        fan_controller_off();
        fan_on = false;
    }
    
    return ESP_OK;
}
```

## Resumen

1. **Inicializar NVS** → Guardar/leer umbral de temperatura
2. **Configurar GPIO** → Pin de salida para controlar ventilador
3. **Leer temperatura** → Desde sensor o simulación
4. **Comparar con umbral** → Leer umbral desde NVS
5. **Controlar GPIO** → Encender/apagar ventilador según comparación

El valor del umbral persiste en flash gracias a NVS, por lo que se mantiene incluso después de reiniciar el ESP32.

