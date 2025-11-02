# Parcial_1

## Descripción

Este es el primer parcial de la materia de Sistemas Operativos en tiempo real. El sistema implementa un monitor de temperatura con control de LED RGB basado en sensores NTC y potenciómetro, utilizando FreeRTOS para gestión multitarea.

## Flujo General del Sistema

El sistema funciona mediante una arquitectura multitarea donde diferentes tareas se comunican a través de colas de FreeRTOS:

1. **Inicialización (`app_main`)**: Configura el hardware (ADC para sensor NTC y potenciómetro, GPIO para botón, PWM para LED RGB, UART para comandos) y crea todas las colas y tareas.

2. **Lectura de Sensores**: Dos tareas leen periódicamente los sensores:
   - `pot_reading_task`: Lee el potenciómetro cada 250ms y publica datos en `pot_queue`
   - `ntc_reading_task`: Lee el sensor NTC cada 2000ms y publica datos validados en `ntc_queue`

3. **Control del LED RGB**: La tarea `rgb_control_task` recibe datos de las colas y decide el color/intensidad del LED según:
   - **Modo automático**: Basado en rangos de temperatura configurables (rojo/verde/azul)
   - **Modo manual**: Control directo por potenciómetro asignado a un color específico
   - **Control por botón**: Puede forzar el apagado del LED

4. **Interfaz de Usuario**: 
   - `display_info_task`: Muestra información del sistema periódicamente
   - `uart_receiver_task`: Procesa comandos UART para configuración
   - `button_task`: Detecta pulsaciones del botón físico

## Implementaciones del Parcial_1

Durante el desarrollo del parcial se implementaron tres funcionalidades principales que están marcadas en el código con los comentarios `IMPLEMENTACIÓN PARCIAL_1`:

### 1. Control Alternado del LED mediante Botón

**Ubicación**: `button_control.c` (línea 101) y `rgb_control_task` en `main.c` (línea 132)

**Funcionalidad implementada**:
- Se añadió la capacidad del botón físico para forzar el apagado del LED RGB de forma alternada mediante pulsaciones cortas.
- Cuando el usuario presiona el botón brevemente, se alterna el estado `led_forced_off` del contexto del botón.
- La tarea `rgb_control_task` verifica continuamente si `led_forced_off` está activo. Si es así, mantiene el LED apagado independientemente de otros controles (temperatura o potenciómetro) y continúa el ciclo sin procesar otros comandos.

**Comportamiento**:
- **Pulsación corta**: Alterna entre LED forzado apagado y control normal
- **Pulsación larga**: Se mantiene la detección para posibles usos futuros
- El LED se mantiene apagado mientras `led_forced_off` esté activo, incluso si la temperatura o el potenciómetro indican que debería estar encendido

### 2. Comando `potmv` - Lectura Única del Voltaje del Potenciómetro

**Ubicación**: `uart_receiver_task` en `main.c` (líneas 384-390) y menú de ayuda (línea 350)

**Funcionalidad implementada**:
- Se implementó el comando UART `potmv` que permite leer el voltaje del potenciómetro en milivoltios una sola vez, a demanda.
- Este comando lee directamente del ADC usando `pot_get_voltage_mv()` y muestra el resultado inmediatamente.
- Se eliminó la impresión periódica del voltaje en milivoltios del monitor de sistema (`display_info_task`), dejando solo el porcentaje (0-100%) en el monitoreo continuo.

**Uso**:
```
> potmv
POT: 1650 mV
>
```

**Motivación**: Permite consultar el voltaje exacto cuando sea necesario sin saturar la salida del monitor periódico, donde solo se muestra el porcentaje más útil para el usuario.

### 3. Comando `rate` - Configuración del Periodo de Impresión del Monitor

**Ubicación**: `uart_receiver_task` en `main.c` (líneas 393-401) y `display_info_task` (líneas 258-262)

**Funcionalidad implementada**:
- Se implementó el comando UART `rate <ms>` que permite cambiar dinámicamente el periodo de impresión del monitor de sistema.
- El comando acepta un valor en milisegundos (rango válido: 100ms a 60000ms) y actualiza `ctx->monitor_period_ms`.
- La tarea `display_info_task` utiliza este periodo configurable mediante `vTaskDelay()`, permitiendo ajustar la frecuencia de visualización del estado del sistema.

**Validación**:
- Valores menores a 100ms se ajustan automáticamente a 100ms (mínimo)
- Valores mayores a 60000ms se ajustan automáticamente a 60000ms (máximo)

**Uso**:
```
> rate 5000
OK: rate 5000 ms
>
```
Esto cambia el periodo de impresión del monitor de 2000ms (por defecto) a 5000ms.

**Beneficios**: Permite al usuario ajustar la frecuencia de actualización del monitor según sus necesidades, reduciendo la salida de datos cuando se requiere menos información o aumentándola para depuración.

## Resumen de Módulos

### `main.c`
Archivo principal que orquesta todo el sistema RTOS. Contiene:

#### Estructuras de Datos
- **`pot_data_t`**: Almacena porcentaje (0-100%) y voltaje en milivoltios del potenciómetro
- **`app_command_t`**: Comandos UART con tipo (`CMD_SET_RED`, `CMD_SET_GREEN`, `CMD_SET_BLUE`, `CMD_SET_POT`) y valores (rangos de temperatura o canal del potenciómetro)
- **`app_context_t`**: Contexto global del sistema que incluye:
  - Todas las colas del sistema (pot, NTC, LED, comandos)
  - Estado actual de sensores (`current_pot_data`, `current_ntc_data`, `data_ready`)
  - Periodo configurable de monitorización (`monitor_period_ms`)
  - Referencias a todos los contextos de módulos (pot, NTC, RGB, botón)

#### Funciones Principales
- **`app_main()`**: Punto de entrada del sistema que realiza:
  - Asignación de memoria para el contexto de aplicación
  - Inicialización de hardware (sensores, botón, LED RGB)
  - Configuración de UART0 (115200 baud, 8N1, GPIO1/GPIO3)
  - Creación de todas las colas del sistema (5 colas con capacidad de 5 elementos cada una)
  - Creación de todas las tareas del sistema con sus prioridades asignadas
  - Logging inicial del estado del sistema
  
- **`conditional_log_info()`**: Función auxiliar que permite logging condicional basado en el estado `print_enabled` del botón. Utiliza `va_list` para formateo de mensajes similares a `printf`.

#### Tareas Implementadas
- **`pot_reading_task()`**: Lee potenciómetro cada 250ms, publica en cola y actualiza estado global
- **`ntc_reading_task()`**: Lee sensor NTC cada 2000ms, valida datos (ADC, temperatura, resistencia) antes de publicar
- **`rgb_control_task()`**: Control central del LED RGB que:
  - Procesa comandos UART no bloqueantes de la `command_queue`
  - Lee datos de `pot_queue` y `ntc_queue` de forma no bloqueante
  - Implementa modo automático (temperatura) y manual (potenciómetro)
  - Resuelve solapamientos de rangos mezclando colores (blanco, amarillo, magenta, cian)
  - Respeta el estado de forzado de apagado del botón
- **`display_info_task()`**: Muestra información periódica del sistema solo si `print_enabled` está activo
- **`uart_receiver_task()`**: Procesa comandos UART de forma no bloqueante:
  - Parsea comandos de texto (`help`, `R/G/B`, `pot`, `potmv`, `rate`)
  - Valida y envía comandos a `command_queue`
  - Algunos comandos (`potmv`, `rate`) se procesan directamente sin cola

#### Configuración de Hardware
- **UART0**: Configurado para entrada/salida estándar con:
  - Buffer de recepción de 1024 bytes
  - Pines TX=GPIO1, RX=GPIO3
  - Integración con VFS para uso de `printf`/`scanf`

#### Gestión de Memoria
- Asignación dinámica del contexto principal con `calloc()`
- Buffer dinámico de 256 bytes para recepción UART en `uart_receiver_task`
- Los módulos de hardware también utilizan asignación dinámica para sus contextos

### `button_control.c/h`
Módulo encapsulado para control de botón físico con detección de pulsaciones y debounce.

#### Estructuras de Datos
- **`button_state_t`**: Estructura de estado del botón que incluye:
  - `is_pressed`: Estado actual de la pulsación
  - `print_enabled`: Flag para habilitar/deshabilitar impresión (no se alterna con botón)
  - `led_forced_off`: Flag para forzar apagado del LED (alternado con pulsación corta)
  - `press_start_time`: Timestamp del inicio de la pulsación
  - `last_event_time`: Timestamp del último evento para debounce
- **`button_event_t`**: Enum con tipos de eventos (`BUTTON_EVENT_NONE`, `BUTTON_EVENT_PRESSED`, `BUTTON_EVENT_RELEASED`, `BUTTON_EVENT_LONG_PRESS`)
- **`button_ctx_t`**: Contexto opaco que contiene el estado y la cola de eventos interna

#### Funciones Principales
- **`button_control_create()`**: Inicializa el módulo:
  - Configura GPIO14 como entrada con pull-up interno (activo en bajo)
  - Crea cola de eventos interna (5 elementos, tipo `button_event_t`)
  - Inicializa estado por defecto (LED no forzado apagado, impresión habilitada)
- **`button_control_destroy()`**: Limpia recursos (cola y memoria)
- **`button_task()`**: Tarea FreeRTOS que implementa la máquina de estados:
  - Polling cada 10ms para detectar cambios de estado
  - Detección de flancos de bajada/subida con debounce de 50ms
  - Detección de pulsación larga (≥1000ms) mientras está presionado
  - En pulsación corta: alterna `led_forced_off`
  - En pulsación larga: genera evento `BUTTON_EVENT_LONG_PRESS`

#### Funciones Auxiliares Privadas
- **`read_button_state()`**: Lee estado físico del GPIO (retorna `true` si presionado)
- **`get_current_time_ms()`**: Obtiene tiempo actual en ms usando `esp_timer_get_time()`
- **`is_debounce_ready()`**: Verifica si ha pasado el tiempo de debounce desde el último evento
- **`is_long_press()`**: Determina si la pulsación actual ya supera el umbral de pulsación larga

#### Funciones Públicas de Acceso
- **`get_button_state()`**: Obtiene copia completa del estado del botón
- **`is_led_forced_off()`**: Verifica si el LED está forzado apagado
- **`is_print_enabled()`**: Verifica si la impresión está habilitada
- **`set_print_enabled()`**: Permite habilitar/deshabilitar impresión manualmente

#### Configuración
- **`BUTTON_PIN`**: GPIO14
- **`BUTTON_DEBOUNCE_TIME`**: 50ms
- **`BUTTON_LONG_PRESS_TIME`**: 1000ms
- **Cola interna**: 5 elementos, creada pero principalmente mantenida por compatibilidad

### `ntc_sensor.c/h`
Módulo para lectura de temperatura mediante termistor NTC (Negative Temperature Coefficient) de 10kΩ.

#### Estructuras de Datos
- **`ntc_data_t`**: Estructura de datos de lectura que contiene:
  - `temperature_c`: Temperatura calculada en grados Celsius
  - `resistance`: Resistencia del NTC calculada en Ohms
  - `raw_adc_value`: Valor ADC raw (0-4095)
- **`ntc_sensor_ctx_t`**: Contexto opaco que contiene:
  - `adc_handle`: Manejador del ADC oneshot para lectura
  - `cali_handle`: Manejador de calibración ADC (opcional)

#### Funciones Principales
- **`ntc_sensor_create()`**: Inicializa el módulo:
  - Configura ADC2 (unidad 2) en modo oneshot
  - Configura canal 9 (GPIO26) con atenuación de 12dB (rango 0-3.3V)
  - Intenta inicializar calibración ADC (curve fitting o line fitting según disponibilidad)
  - Retorna contexto inicializado o NULL en caso de error
- **`ntc_sensor_destroy()`**: Libera recursos del ADC y calibración
- **`ntc_read_temperature()`**: Función principal de lectura:
  - Lee valor ADC raw del canal configurado
  - Valida que el ADC esté en rango válido (0 < raw < 4096)
  - Calcula resistencia usando divisor de voltaje: `R = R_serie * (4095/ADC - 1)`
  - Valida que la resistencia esté en rango razonable (0 < R < 1MΩ)
  - Calcula temperatura usando ecuación de Steinhart-Hart simplificada
  - Retorna `-999.0°C` como valor de error si hay problemas

#### Calibración ADC
- **`adc_calibration_init()`**: Función privada que intenta inicializar calibración:
  - Prioriza curve fitting si está soportado (mayor precisión)
  - Si no, intenta line fitting (método alternativo)
  - Retorna `true` si la calibración fue exitosa, `false` en caso contrario
  - Sin calibración, el sistema funciona pero con menor precisión

#### Cálculo de Temperatura
Utiliza la ecuación de Steinhart-Hart simplificada (modelo Beta):
```
1/T = (1/B) * ln(R/R0) + 1/T0
```
Donde:
- `T`: Temperatura absoluta en Kelvin
- `R`: Resistencia actual del NTC
- `R0`: Resistencia nominal (10000Ω a 25°C)
- `T0`: Temperatura nominal (298.15K = 25°C)
- `B`: Coeficiente Beta del termistor (3380)

#### Configuración del Hardware
- **`NTC_PIN`**: ADC_CHANNEL_9 (GPIO26)
- **`ADC_UNIT`**: ADC_UNIT_2
- **`SERIES_RESISTOR`**: 10000Ω (resistencia en serie del divisor de voltaje)
- **`NOMINAL_RESISTANCE`**: 10000Ω
- **`NOMINAL_TEMPERATURE`**: 25.0°C
- **`B_COEFFICIENT`**: 3380.0

#### Validación de Datos
- Valida ADC raw: debe estar entre 0 y 4095 (excluidos los extremos)
- Valida resistencia: debe estar entre 0 y 1MΩ
- Valida temperatura calculada: debe estar entre -50°C y 150°C (validación externa en `ntc_reading_task`)
- Retorna valores de error (`-999.0`) si alguna validación falla

### `potentiometer.c/h`
Módulo para lectura de potenciómetro analógico usando ADC con promediado para reducir ruido.

#### Estructuras de Datos
- **`pot_ctx_t`**: Contexto interno que contiene:
  - `adc_handle`: Manejador del ADC oneshot para lectura
  - `cali_handle`: Manejador de calibración ADC (puede ser NULL)
  - `has_calibration`: Flag que indica si la calibración está disponible
  - `channel`: Canal ADC configurado (ADC_CHANNEL_6)

#### Funciones Principales
- **`pot_create()`**: Inicializa el módulo:
  - Configura ADC1 (unidad 1) en modo oneshot
  - Configura canal 6 (GPIO34) con atenuación de 12dB (rango 0-3.3V)
  - Intenta inicializar calibración ADC (curve fitting o line fitting)
  - Almacena el resultado de la calibración en `has_calibration`
- **`pot_destroy()`**: Libera recursos del ADC y calibración
- **`pot_get_voltage_mv()`**: Lee y convierte a milivoltios:
  - Utiliza `read_raw_avg()` para obtener promedio de 8 muestras
  - Si hay calibración: usa `adc_cali_raw_to_voltage()` para conversión precisa
  - Si no hay calibración: cálculo lineal aproximado `(raw * 3300) / 4095`
  - Retorna voltaje en milivoltios (0-3300mV)
- **`pot_get_percent()`**: Calcula porcentaje de posición:
  - Obtiene voltaje mediante `pot_get_voltage_mv()`
  - Calcula porcentaje: `(mV * 100) / 3300`
  - Si mV >= 3300, retorna 100% (saturación)
  - Retorna valor entre 0-100%

#### Funciones Auxiliares Privadas
- **`read_raw_avg()`**: Promedio de múltiples muestras:
  - Lee 8 muestras del ADC con delay de 10ms entre cada una
  - Suma todas las muestras y calcula promedio
  - Reduce ruido eléctrico y variaciones temporales
  - Retorna valor ADC promedio

- **`adc_calibration_init()`**: Función privada idéntica a la del módulo NTC:
  - Intenta inicializar calibración curve fitting primero
  - Si falla, intenta line fitting
  - Retorna `true` si la calibración fue exitosa

#### Configuración del Hardware
- **Canal**: ADC_CHANNEL_6 (GPIO34)
- **Unidad ADC**: ADC_UNIT_1
- **Atenuación**: ADC_ATTEN_DB_12 (rango de medida 0-3.3V)
- **Resolución**: ADC_BITWIDTH_DEFAULT (12 bits = 0-4095)
- **Número de muestras**: 8 (definido por `NO_OF_SAMPLES`)
- **Delay entre muestras**: 10ms

#### Uso en el Sistema
- La lectura del potenciómetro se usa para:
  - Controlar intensidad del LED RGB (0-100%)
  - Control manual de color específico cuando está asignado (rojo/verde/azul)
- Las lecturas se publican periódicamente en `pot_queue` desde `pot_reading_task`
- También se puede leer directamente con el comando UART `potmv`

### `rgb_led.c/h`
Módulo de control de LED RGB mediante PWM (Pulse Width Modulation) usando el periférico LEDC de ESP32.

#### Estructuras de Datos
- **`rgb_led_ctx_t`**: Contexto interno que contiene:
  - `current_intensity`: Intensidad global actual (0-100%)
  - `current_red`: Componente rojo base (0-255)
  - `current_green`: Componente verde base (0-255)
  - `current_blue`: Componente azul base (0-255)

#### Funciones Principales
- **`rgb_led_create()`**: Inicializa el módulo:
  - Configura timer LEDC (LEDC_TIMER_0) con frecuencia 5kHz y resolución 8 bits
  - Configura tres canales LEDC (0, 1, 2) para rojo, verde y azul respectivamente
  - Todos los canales comparten el mismo timer para sincronización
  - Configura GPIO13 (rojo), GPIO12 (verde), GPIO25 (azul)
  - Inicializa LED apagado (intensidad 0, colores base en 255)
- **`rgb_led_destroy()`**: Libera memoria del contexto (no requiere limpieza de hardware)
- **`rgb_led_set_color()`**: Establece color base RGB (0-255 por canal):
  - Actualiza los valores base de cada componente
  - Llama a `rgb_led_update_pwm()` para aplicar cambios
- **`rgb_led_set_intensity()`**: Establece intensidad global (0-100%):
  - Limita el valor al rango 0-100
  - Aplica la intensidad multiplicando cada canal por el porcentaje
  - Llama a `rgb_led_update_pwm()` para aplicar cambios
- **`rgb_led_off()`**: Apaga el LED:
  - Establece intensidad a 0%
  - Actualiza PWM para reflejar el cambio

#### Funciones Auxiliares Privadas
- **`rgb_led_update_pwm()`**: Actualiza los tres canales PWM:
  - Calcula valor PWM para cada canal: `(color_base * intensity) / 100`
  - Establece duty cycle para cada canal con `ledc_set_duty()`
  - Actualiza duty cycle con `ledc_update_duty()`
  - Esta función se llama cada vez que cambia color o intensidad

#### Funciones de Consulta
- **`rgb_led_get_intensity()`**: Retorna intensidad actual (0-100%)
- **`rgb_led_get_red()`**, **`rgb_led_get_green()`**, **`rgb_led_get_blue()`**: Retornan componentes RGB base actuales
- **`rgb_led_is_on()`**: Retorna `true` si la intensidad > 0
- **`rgb_led_get_color_name()`**: Identifica el color actual por nombre:
  - Analiza los valores RGB actuales
  - Retorna: "ROJO", "VERDE", "AZUL", "AMARILLO", "MAGENTA", "CIAN", "BLANCO", "MIXTO", o "APAGADO"
  - Usa umbrales (>200 para color puro, <50 para ausencia)

#### Configuración PWM
- **Timer**: LEDC_TIMER_0
- **Modo**: LEDC_LOW_SPEED_MODE
- **Frecuencia**: 5000 Hz (5 kHz) - define en `RGB_PWM_FREQ`
- **Resolución**: 8 bits (256 niveles, 0-255) - define en `RGB_PWM_RESOLUTION`
- **Canales**:
  - Canal 0 (LEDC_CHANNEL_0): GPIO13 (rojo)
  - Canal 1 (LEDC_CHANNEL_1): GPIO12 (verde)
  - Canal 2 (LEDC_CHANNEL_2): GPIO25 (azul)
- **Características**: Los tres canales comparten el mismo timer para sincronización y evitar parpadeos

#### Modelo de Control
El módulo implementa un modelo de color base + intensidad:
- **Color base**: Define qué componentes RGB están activos (0-255 cada uno)
- **Intensidad**: Escala global aplicada a todos los componentes (0-100%)
- **Valor PWM final**: `(color_base * intensity) / 100`
- Esto permite mantener el color mientras se ajusta el brillo sin cambiar la proporción de colores

#### Uso en el Sistema
- Controlado por `rgb_control_task` que decide:
  - Color según temperatura (modo automático)
  - Color según asignación del potenciómetro (modo manual)
  - Apagado forzado por botón
- La intensidad siempre se controla con el potenciómetro (0-100%)

## Tareas del Sistema

El sistema utiliza **6 tareas** de FreeRTOS con diferentes prioridades:

### 1. `uart_receiver_task` (Prioridad: 7 - MÁS ALTA)
- **Función**: Procesa comandos recibidos por UART de forma no bloqueante
- **Periodo**: Tiempo real (polling cada 50ms en `uart_read_bytes`)
- **Comunicación**: 
  - Lee de UART
  - Envía a `command_queue` (no bloqueante)
- **Comandos procesados**: `help`, `R/G/B <min> <max>`, `pot <r|g|b|none>`, `potmv`, `rate <ms>`

### 2. `rgb_control_task` (Prioridad: 6)
- **Función**: Controla el LED RGB según temperatura (automático) o potenciómetro (manual)
- **Periodo**: 50ms (20Hz)
- **Comunicación**:
  - Recibe de `pot_queue` (timeout 1ms, no bloqueante)
  - Recibe de `ntc_queue` (timeout 10ms, no bloqueante)
  - Recibe de `command_queue` (timeout 0ms, no bloqueante)
  - Lee estado del botón directamente (función `is_led_forced_off`)
- **Lógica**:
  - Si botón fuerza apagado: mantiene LED apagado
  - Si `pot_control_target != 0`: modo manual con potenciómetro
  - Si no: modo automático según rangos de temperatura (resuelve solapamientos mezclando colores)

### 3. `pot_reading_task` (Prioridad: 5)
- **Función**: Lee el potenciómetro periódicamente y publica datos
- **Periodo**: 250ms (4Hz)
- **Comunicación**: 
  - Envía a `pot_queue` (timeout 10ms)
  - Actualiza `ctx->current_pot_data` directamente
- **Datos publicados**: Porcentaje (0-100%) y voltaje (mV)

### 4. `ntc_reading_task` (Prioridad: 5)
- **Función**: Lee temperatura del sensor NTC y valida rangos antes de publicar
- **Periodo**: 2000ms (0.5Hz)
- **Comunicación**: 
  - Envía a `ntc_queue` (timeout 10ms) solo si datos son válidos
  - Actualiza `ctx->current_ntc_data` y `ctx->data_ready` directamente
- **Validación**: ADC entre 0-4096, temperatura entre -50°C y 150°C, resistencia razonable

### 5. `button_task` (Prioridad: 4)
- **Función**: Detecta pulsaciones del botón físico con debounce
- **Periodo**: 10ms (100Hz)
- **Comunicación**: 
  - Envía eventos a su cola interna (`ctx->queue` en button_control)
  - Actualiza estado interno del botón
- **Eventos**: Pulsación corta (alterna `led_forced_off`), pulsación larga (≥1000ms)

### 6. `display_info_task` (Prioridad: 3 - MÁS BAJA)
- **Función**: Muestra información del sistema por consola
- **Periodo**: Variable según `ctx->monitor_period_ms` (por defecto 2000ms, configurable con `rate`)
- **Comunicación**: 
  - Lee `ctx->current_pot_data`, `ctx->current_ntc_data`, estado del botón y LED
  - Solo imprime si `print_enabled` está habilitado
- **Información mostrada**: Porcentaje potenciómetro, estado LED RGB, temperatura NTC, resistencia, valor ADC

## Colas del Sistema

El sistema utiliza **5 colas** de FreeRTOS para comunicación entre tareas:

### 1. `pot_queue` (Tipo: `pot_data_t`, Tamaño: 5 elementos)
- **Productores**: `pot_reading_task` (cada 250ms)
- **Consumidores**: `rgb_control_task` (lectura no bloqueante)
- **Datos**: Porcentaje del potenciómetro (0-100%) y voltaje en milivoltios
- **Uso**: Proporciona datos actualizados del potenciómetro para control de intensidad del LED

### 2. `ntc_queue` (Tipo: `ntc_data_t`, Tamaño: 5 elementos)
- **Productores**: `ntc_reading_task` (cada 2000ms, solo datos válidos)
- **Consumidores**: `rgb_control_task` (lectura no bloqueante con timeout 10ms)
- **Datos**: Temperatura en °C, resistencia en Ω, valor ADC raw
- **Uso**: Proporciona lecturas de temperatura para control automático del color del LED RGB

### 3. `led_queue` (Tipo: `uint8_t`, Tamaño: 5 elementos)
- **Estado**: Creada pero **NO UTILIZADA** actualmente en el código
- **Propósito reservado**: Potencial uso futuro para comandos directos al LED

### 4. `command_queue` (Tipo: `app_command_t`, Tamaño: 5 elementos)
- **Productores**: `uart_receiver_task` (cuando se procesa un comando válido)
- **Consumidores**: `rgb_control_task` (lectura no bloqueante sin timeout)
- **Datos**: Tipo de comando (`CMD_SET_RED`, `CMD_SET_GREEN`, `CMD_SET_BLUE`, `CMD_SET_POT`) y valores asociados
- **Uso**: Transmite comandos UART para configurar rangos de temperatura y modo de control del potenciómetro

### 5. `button_ctx->queue` (Tipo: `button_event_t`, Tamaño: 5 elementos)
- **Productores**: `button_task` (cuando detecta eventos de botón)
- **Consumidores**: Ninguno actualmente (la cola se mantiene por compatibilidad)
- **Datos**: Eventos del botón (`BUTTON_EVENT_PRESSED`, `BUTTON_EVENT_LONG_PRESS`)
- **Uso**: Encapsulado dentro del módulo `button_control`, acceso al estado mediante funciones públicas

## Otros Elementos FreeRTOS

### Delays y Timeouts
- **`vTaskDelay()`**: Utilizado en todas las tareas para control de período
  - `pot_reading_task`: 250ms
  - `ntc_reading_task`: 2000ms
  - `rgb_control_task`: 50ms
  - `display_info_task`: Variable (`monitor_period_ms`)
  - `button_task`: 10ms
  - `uart_receiver_task`: 10ms

- **`pdMS_TO_TICKS()`**: Conversión de milisegundos a ticks de FreeRTOS usada en:
  - Timeouts de colas (`xQueueSend`, `xQueueReceive`)
  - Delays de tareas

- **Timeouts en colas**:
  - `xQueueSend`: Timeout de 10ms para evitar bloqueos prolongados
  - `xQueueReceive`: Timeouts variables (0ms, 1ms, 10ms) según necesidad de bloqueo

### Gestión de Memoria
- **`calloc()`**: Asignación de memoria para contextos de módulos y estructura principal
- **`free()`**: Liberación de memoria en funciones de destrucción (aunque no se llaman en el código actual)

### Sincronización y Estado Compartido
- **Acceso directo a estructuras**: `ctx->current_pot_data` y `ctx->current_ntc_data` se actualizan directamente desde las tareas de lectura para acceso rápido sin colas
- **Funciones de estado**: `get_button_state()`, `is_led_forced_off()` proporcionan acceso thread-safe al estado del botón

## Comandos UART Disponibles

El sistema acepta los siguientes comandos por UART (115200 baud, 8N1):

```
help                    -> Muestra menú de ayuda

R <min> <max>          -> Define rango de temperatura para color ROJO (ej: R 0 15.5)
G <min> <max>          -> Define rango de temperatura para color VERDE (ej: G 10 30)
B <min> <max>          -> Define rango de temperatura para color AZUL (ej: B 25 40)

pot <r|g|b|none>       -> Asigna potenciómetro a controlar un color (r=rojo, g=verde, b=azul, none=automático)
potmv                   -> Lee y muestra voltaje del potenciómetro (mV) una sola vez

rate <ms>              -> Cambia periodo de impresión del monitor (100-60000 ms)
```

## Hardware Utilizado

- **Potenciómetro**: GPIO34 (ADC1_CH6)
- **Sensor NTC**: GPIO26 (ADC2_CH9)
- **Botón**: GPIO14 (con pull-up interno)
- **LED RGB**: GPIO13 (rojo), GPIO12 (verde), GPIO25 (azul)
- **UART**: GPIO1 (TX), GPIO3 (RX) - 115200 baud

## Frecuencias de Operación

- **Potenciómetro**: 4 Hz (250ms)
- **Sensor NTC**: 0.5 Hz (2000ms)
- **Control LED**: 20 Hz (50ms)
- **Monitor**: 0.5 Hz por defecto (2000ms, configurable)
- **Botón**: 100 Hz (10ms polling)
- **UART**: Tiempo real (lectura no bloqueante)

