# Flujo del Código - Proyecto Retro Web Terminal ESP32

Este documento explica el flujo de ejecución del código desde el inicio hasta el funcionamiento completo del sistema.

---

## Índice

1. [Punto de Entrada](#1-punto-de-entrada)
2. [Inicialización del Sistema](#2-inicialización-del-sistema)
3. [Inicialización de Hardware](#3-inicialización-de-hardware)
4. [Configuración de Red WiFi](#4-configuración-de-red-wifi)
5. [Inicio del Servidor Web](#5-inicio-del-servidor-web)
6. [Flujo de Peticiones HTTP](#6-flujo-de-peticiones-http)
7. [Procesamiento de Comandos](#7-procesamiento-de-comandos)
8. [Tareas en Segundo Plano](#8-tareas-en-segundo-plano)

---

## 1. Punto de Entrada

**Archivo:** `main/main.c`  
**Función:** `app_main()`

Esta es la función principal que se ejecuta automáticamente cuando el ESP32 inicia. Es el punto de entrada de toda la aplicación.

```c
void app_main(void)
```

**Orden de ejecución:**
1. Inicialización de NVS (almacenamiento no volátil)
2. Inicialización de hardware (GPIO y sensor NTC)
3. Inicialización de WiFi (modo SoftAP)
4. Inicio del servidor web

---

## 2. Inicialización del Sistema

**Archivo:** `main/main.c`  
**Sección:** Líneas 53-61

### 2.1. Inicialización de NVS (Non-Volatile Storage)

```c
ret = nvs_flash_init();
if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
}
```

**¿Qué hace?**
- Inicializa el sistema de almacenamiento no volátil del ESP32
- NVS es necesario para almacenar configuración WiFi
- Si hay problemas con la partición, la borra y reinicializa

**¿Por qué es necesario?**
- El WiFi necesita NVS para guardar configuración persistente
- Aunque en este proyecto usamos SoftAP (no guarda configuración), NVS es requerido por el stack WiFi

---

## 3. Inicialización de Hardware

**Archivo:** `main/main.c`  
**Sección:** Líneas 63-71

### 3.1. Inicialización de GPIO (LEDs)

```c
gpio_init_leds();
```

**Archivo:** `main/gpio_driver.c`  
**Función:** `gpio_init_leds()`

**¿Qué hace?**
- Crea un mutex para proteger acceso concurrente a GPIO
- Configura GPIO 2 (LED amarillo) como entrada/salida
- Configura GPIO 5 (LED azul) como entrada/salida
- Inicializa ambos LEDs en estado apagado

**Hardware:**
- LED Amarillo: GPIO 2
- LED Azul: GPIO 5

### 3.2. Inicialización del Sensor NTC

```c
ntc_sensor_init();
ntc_start_reading_task();
```

**Archivo:** `main/ntc_sensor.c`

**¿Qué hace `ntc_sensor_init()`?**
- Crea un mutex para proteger acceso a datos del sensor
- Configura ADC1 (canal 4, GPIO32) para leer el sensor
- Inicializa la calibración del ADC para lecturas precisas
- **Nota:** Usa ADC1 porque ADC2 no funciona cuando WiFi está activo

**¿Qué hace `ntc_start_reading_task()`?**
- Crea una tarea de FreeRTOS llamada `"ntc_reader"`
- La tarea lee la temperatura cada 1 segundo
- Almacena los datos de forma thread-safe usando mutex
- Los datos quedan disponibles para el servidor web

**Tarea creada:**
- Nombre: `"ntc_reader"`
- Stack: 4096 bytes
- Prioridad: 5
- Función: `ntc_reading_task()` (loop infinito)

---

## 4. Configuración de Red WiFi

**Archivo:** `main/main.c`  
**Línea:** 76

```c
wifi_init_softap();
```

**Archivo:** `main/wifi_app.c`  
**Función:** `wifi_init_softap()`

**¿Qué hace?**
1. Inicializa la pila TCP/IP (`esp_netif_init()`)
2. Crea el bucle de eventos para manejar eventos WiFi
3. Crea la interfaz de red WiFi en modo Access Point
4. Inicializa el driver WiFi con configuración por defecto
5. Registra el manejador de eventos (para logging de conexiones)
6. Configura credenciales de la red:
   - SSID: `"ESP32_Server"`
   - Contraseña: `"12345678"`
   - Canal: 1
   - Máximo de conexiones: 4
   - Seguridad: WPA2 Personal
7. Inicia el Access Point

**Resultado:**
- El ESP32 crea una red WiFi propia
- Los usuarios pueden conectarse a `"ESP32_Server"` con contraseña `"12345678"`
- La IP del ESP32 será `192.168.4.1` (por defecto en SoftAP)

---

## 5. Inicio del Servidor Web

**Archivo:** `main/main.c`  
**Línea:** 81

```c
start_webserver();
```

**Archivo:** `main/web_server.c`  
**Función:** `start_webserver()`

### 5.1. Creación de Estructura de Contexto

Se crea una estructura `webserver_context_t` que encapsula todo el estado del servidor:
- Handles de colas (queues) para comandos
- Mutexes para protección de sesiones y IDs
- Array de sesiones de usuarios
- Handle del servidor HTTP

### 5.2. Creación de Mutexes

**Mutex de Sesiones:**
```c
ctx.session_mutex = xSemaphoreCreateMutex();
```
- Protege el array `ctx->sessions[]` de acceso concurrente
- Necesario porque múltiples requests HTTP pueden acceder simultáneamente

**Mutex de IDs de Comandos:**
```c
ctx.command_id_mutex = xSemaphoreCreateMutex();
```
- Protege `ctx->command_id_counter` de race conditions
- Garantiza IDs únicos para cada comando

### 5.3. Creación de Colas (Queues)

**Cola de Comandos:**
```c
ctx.gpio_command_queue = xQueueCreate(GPIO_QUEUE_SIZE, sizeof(gpio_command_t));
```
- Tamaño: 10 elementos
- Dirección: Handler HTTP → Tarea de procesamiento
- Uso: El handler `/cmd` envía comandos aquí

**Cola de Respuestas:**
```c
ctx.gpio_response_queue = xQueueCreate(GPIO_QUEUE_SIZE, sizeof(gpio_command_t));
```
- Tamaño: 10 elementos
- Dirección: Tarea de procesamiento → Handler HTTP
- Uso: La tarea envía respuestas aquí

### 5.4. Creación de Tareas

**Tarea de Procesamiento de Comandos:**
```c
xTaskCreate(gpio_command_task_wrapper, "gpio_cmd_task", 4096, &ctx, 5, NULL);
```
- Nombre: `"gpio_cmd_task"`
- Stack: 4096 bytes
- Prioridad: 5 (alta)
- Función: `gpio_command_task_wrapper()` → llama a `terminal_command_task()`
- Propósito: Procesar comandos de la terminal web de forma asíncrona

**Tarea de Gestión de Sesiones:**
```c
xTaskCreate(session_management_task, "session_mgmt", 2048, &ctx, 3, NULL);
```
- Nombre: `"session_mgmt"`
- Stack: 2048 bytes
- Prioridad: 3 (media)
- Función: `session_management_task()`
- Propósito: Verificar periódicamente las sesiones y expirar las inactivas (>3 minutos)

### 5.5. Inicialización de SPIFFS

```c
init_spiffs();
```

**¿Qué hace?**
- Monta el sistema de archivos SPIFFS desde la partición `"storage"`
- SPIFFS contiene los archivos HTML, CSS y JS del frontend
- Verifica que todos los archivos necesarios estén presentes:
  - `index.html`
  - `login.html`
  - `dashboard.html`
  - `terminal.html`
  - `slider.html`
  - `style.css`
  - `script.js`

**Inicialización de Registros:**
```c
crear_archivo_si_no_existe();
```
- Crea `/spiffs/registros.json` si no existe
- Inicializa con un array JSON vacío `[]`

### 5.6. Configuración e Inicio del Servidor HTTP

```c
httpd_config_t config = HTTPD_DEFAULT_CONFIG();
config.max_uri_handlers = 14;
config.max_open_sockets = 7;
config.lru_purge_enable = true;
httpd_start(&ctx.server, &config);
```

**Configuración:**
- Puerto: 80 (por defecto)
- Máximo de handlers de URI: 14
- Máximo de sockets abiertos: 7
- Purga LRU habilitada (cierra conexiones inactivas)

### 5.7. Registro de Rutas HTTP

```c
register_http_routes(&ctx);
```

**Rutas registradas:**

**Páginas Web:**
- `GET /` → Redirige a login o dashboard según autenticación
- `GET /dashboard` → Panel principal (requiere autenticación)
- `GET /terminal` → Terminal web retro (requiere autenticación)
- `GET /slider` → Panel de control con temperatura (requiere autenticación)

**Archivos Estáticos:**
- `GET /style.css` → Archivo CSS
- `GET /script.js` → Archivo JavaScript
- `GET /favicon.ico` → Respuesta 204 No Content

**Autenticación:**
- `POST /login` → Validar credenciales y crear sesión
- `GET /logout` → Cerrar sesión y apagar LEDs

**API REST:**
- `GET /cmd?c=comando` → Ejecutar comando de terminal
- `GET /temperature` → Obtener temperatura actual (JSON)
- `GET /registros` → Leer todos los registros (JSON)
- `POST /registros` → Guardar nuevo registro (JSON)

---

## 6. Flujo de Peticiones HTTP

### 6.1. Conexión del Cliente

1. **El usuario se conecta a la red WiFi:**
   - SSID: `"ESP32_Server"`
   - Contraseña: `"12345678"`

2. **El usuario abre el navegador:**
   - URL: `http://192.168.4.1/` (o cualquier IP del ESP32)

3. **El servidor recibe la petición:**
   - El servidor HTTP (creado en `start_webserver()`) recibe la petición
   - Busca el handler correspondiente según la URI

### 6.2. Flujo de Login

**Petición:** `GET /`

**Handler:** `root_get_handler()`

**Flujo:**
1. Se verifica si el usuario está autenticado mediante `is_authenticated()`
2. Si **NO está autenticado:**
   - Se sirve `login.html` desde SPIFFS
   - El usuario visualiza el formulario de login
3. Si **está autenticado:**
   - Se redirige a `/dashboard`

**Petición:** `POST /login`

**Handler:** `login_post_handler()`

**Flujo:**
1. Se reciben los parámetros del formulario: `user=root&pass=matrix123`
2. Se validan las credenciales:
   - Usuario válido: `"root"`
   - Contraseña válida: `"matrix123"`
3. Si las credenciales son correctas:
   - Se obtiene la IP del cliente
   - Se busca o crea una sesión para esa IP mediante `find_or_create_session()`
   - Se marca la sesión como autenticada
   - Se actualiza `last_activity` con el tiempo actual
   - Se responde `"OK"`
4. Si las credenciales son incorrectas:
   - Se responde `401 Unauthorized`

**Estructura de Sesión:**
```c
typedef struct {
    char ip[16];              // IP del cliente
    int64_t last_activity;     // Timestamp de última actividad
    bool authenticated;         // Estado de autenticación
} session_t;
```

### 6.3. Flujo de Peticiones Autenticadas

**Cualquier petición a rutas protegidas:**

1. Handler verifica autenticación:
   ```c
   if (!is_authenticated(ctx, req)) {
       // Redirige a login o responde 401
   }
   ```

2. `is_authenticated()` realiza lo siguiente:
   - Obtiene la IP del cliente
   - Busca la sesión correspondiente
   - Verifica que `authenticated == true`
   - Verifica que no haya expirado (timeout de 3 minutos)
   - Si está activa, actualiza `last_activity`
   - Retorna `true` o `false`

3. Si está autenticado:
   - El handler procesa la petición normalmente
   - Se sirve el archivo o se ejecuta la acción solicitada

### 6.4. Flujo de Terminal Web

**Petición:** `GET /terminal`

**Handler:** `terminal_get_handler()`

**Flujo:**
1. Se verifica la autenticación
2. Si está autenticado:
   - Se sirve `terminal.html` desde SPIFFS
3. El frontend carga y muestra la terminal

**Petición:** `GET /cmd?c=led y on`

**Handler:** `cmd_get_handler()`

**Flujo completo:**

1. **Verificación de autenticación:**
   ```c
   if (!is_authenticated(ctx, req)) {
       // Responde 401
   }
   ```

2. **Extracción del comando:**
   - Se lee el parámetro `c` de la query string
   - Se decodifica la URL (convierte `%20` a espacios)
   - Se convierte a minúsculas

3. **Generación de ID único:**
   ```c
   // Protegido por mutex
   xSemaphoreTake(ctx->command_id_mutex, portMAX_DELAY);
   cmd_id = ++ctx->command_id_counter;
   xSemaphoreGive(ctx->command_id_mutex);
   ```

4. **Preparación del comando:**
   ```c
   gpio_command_t command;
   command.command_id = cmd_id;
   strncpy(command.command, cmd, sizeof(command.command) - 1);
   ```

5. **Envío a la cola:**
   ```c
   xQueueSend(ctx->gpio_command_queue, &command, pdMS_TO_TICKS(1000));
   ```
   - Envía el comando a `gpio_command_queue`
   - La tarea `terminal_command_task` lo recibirá

6. **Espera de respuesta:**
   ```c
   // Se intenta recibir respuesta hasta 10 veces
   for (int attempts = 0; attempts < CMD_RESPONSE_MAX_ATTEMPTS; attempts++) {
       if (xQueueReceive(ctx->gpio_response_queue, &response, pdMS_TO_TICKS(500)) == pdTRUE) {
           if (response.command_id == cmd_id) {
               // Respuesta correcta
               httpd_resp_send(req, response.response, HTTPD_RESP_USE_STRLEN);
               return ESP_OK;
           }
       }
   }
   ```

7. **Envío de respuesta al cliente:**
   - Si se recibió la respuesta correcta, se envía al navegador
   - Si hay timeout, se envía un mensaje de error

### 6.5. Flujo de Lectura de Temperatura

**Petición:** `GET /temperature`

**Handler:** `temperature_get_handler()`

**Flujo:**
1. Se verifica la autenticación
2. Se obtiene la temperatura actual:
   ```c
   ntc_data_t temp_data = ntc_get_current_temperature();
   ```
   - Esta función lee los datos almacenados por la tarea `ntc_reader`
   - Está protegido por mutex para acceso thread-safe

3. Se formatea la respuesta JSON:
   ```json
   {"temperature": 25.5}
   ```
   o
   ```json
   {"error": "No data available"}
   ```

4. Se envía la respuesta al cliente

### 6.6. Flujo de Gestión de Registros

**Petición:** `GET /registros`

**Handler:** `registros_get_handler()`

**Flujo:**
1. Se verifica la autenticación
2. Se leen los registros desde SPIFFS:
   ```c
   char *json = leer_registros_json();
   ```
   - Se abre `/spiffs/registros.json`
   - Se lee el contenido completo
   - Se retorna un string JSON

3. Se envía la respuesta JSON al cliente:
   ```json
   [
     {"dia": "lunes", "hora": "14:30", "velocidad": 50, "id": "1234567890"},
     {"dia": "martes", "hora": "08:00", "velocidad": 75, "id": "1234567891"}
   ]
   ```

**Petición:** `POST /registros`

**Handler:** `registros_post_handler()`

**Flujo:**
1. Se verifica la autenticación
2. Se recibe el body JSON:
   ```json
   {"dia": "lunes", "hora": "14:30", "velocidad": 50}
   ```

3. Se parsea el JSON usando cJSON

4. Se validan los campos requeridos:
   - `dia` (string)
   - `hora` (string)
   - `velocidad` (number)

5. Se guarda el registro:
   ```c
   bool ok = agregar_registro(dia, hora, velocidad);
   ```
   - Se lee el archivo completo
   - Se agrega el nuevo registro al array JSON
   - Se guarda el archivo actualizado en SPIFFS

6. Se responde `"OK"` o un error

---

## 7. Procesamiento de Comandos

**Archivo:** `main/terminal_commands.c`  
**Tarea:** `terminal_command_task()`

### 7.1. Recepción de Comando

La tarea `terminal_command_task` se ejecuta en un loop infinito:

```c
while (1) {
    // Espera indefinidamente hasta recibir un comando
    if (xQueueReceive(ctx->gpio_command_queue, &cmd, portMAX_DELAY) == pdTRUE) {
        // Procesar comando
    }
}
```

**Flujo:**
1. La tarea se bloquea esperando en `gpio_command_queue`
2. Cuando el handler HTTP envía un comando, la tarea lo recibe
3. Se copia el comando a la variable local `cmd`

### 7.2. Procesamiento del Comando

```c
process_terminal_command(&cmd);
```

**Archivo:** `main/terminal_commands.c`  
**Función:** `process_terminal_command()`

**Comandos soportados:**

| Comando | Acción |
|---------|--------|
| `led y on` | Enciende LED amarillo |
| `led y off` | Apaga LED amarillo |
| `led b on` | Enciende LED azul |
| `led b off` | Apaga LED azul |
| `led all on` | Enciende ambos LEDs |
| `led all off` | Apaga ambos LEDs |
| `status` | Muestra estado de los LEDs |
| `help` | Lista de comandos disponibles |
| `clear` | Limpia pantalla (manejado en frontend) |

**Ejemplo de procesamiento:**
```c
if (strcmp(cmd->command, "led y on") == 0) {
    gpio_set_yellow(true);  // Llama a gpio_driver.c
    strcpy(cmd->response, "[OK] LED amarillo encendido.");
}
```

**Control de GPIO:**
- `gpio_set_yellow()` y `gpio_set_blue()` están en `gpio_driver.c`
- Estas funciones usan mutex para acceso thread-safe
- Modifican directamente el estado de los pines GPIO

### 7.3. Envío de Respuesta

```c
xQueueSend(ctx->gpio_response_queue, &cmd, pdMS_TO_TICKS(100));
```

**Flujo:**
1. La función `process_terminal_command()` modifica `cmd->response` in-place
2. La tarea envía el comando completo (con respuesta) a `gpio_response_queue`
3. El handler HTTP recibe la respuesta y la envía al cliente

**Arquitectura Producer-Consumer:**
- **Producer (HTTP Handler):** Envía comandos a `gpio_command_queue`
- **Consumer (Tarea):** Recibe comandos, los procesa
- **Producer (Tarea):** Envía respuestas a `gpio_response_queue`
- **Consumer (HTTP Handler):** Recibe respuestas, las envía al cliente

**Ventajas:**
- El servidor HTTP no se bloquea esperando procesamiento
- Múltiples comandos pueden estar en cola simultáneamente
- Thread-safe gracias a las colas de FreeRTOS
- Permite manejar picos de tráfico sin perder comandos

---

## 8. Tareas en Segundo Plano

### 8.1. Tarea de Lectura de Temperatura

**Archivo:** `main/ntc_sensor.c`  
**Tarea:** `ntc_reading_task()`

**Configuración:**
- Nombre: `"ntc_reader"`
- Stack: 4096 bytes
- Prioridad: 5

**Flujo:**
```c
while (1) {
    // Lee temperatura del sensor
    ntc_data = ntc_read_temperature();
    
    // Protege acceso con mutex
    xSemaphoreTake(data_mutex, portMAX_DELAY);
    current_ntc_data = ntc_data;  // Almacena datos
    data_ready = true;             // Marca como listo
    xSemaphoreGive(data_mutex);
    
    // Espera 1 segundo
    vTaskDelay(pdMS_TO_TICKS(1000));
}
```

**¿Qué hace `ntc_read_temperature()`?**
1. Lee valor crudo del ADC (0-4095)
2. Calcula resistencia del NTC usando divisor de voltaje:
   ```
   R_ntc = R_series * (4095 / ADC_value - 1)
   ```
3. Convierte resistencia a temperatura usando Steinhart-Hart:
   ```
   1/T = 1/T0 + (1/B) * ln(R/R0)
   ```

**Datos disponibles:**
- Los datos se almacenan en `current_ntc_data` (protegido por mutex)
- El servidor web puede leerlos con `ntc_get_current_temperature()`

### 8.2. Tarea de Gestión de Sesiones

**Archivo:** `main/web_server.c`  
**Tarea:** `session_management_task()`

**Configuración:**
- Nombre: `"session_mgmt"`
- Stack: 2048 bytes
- Prioridad: 3

**Flujo:**
```c
while (1) {
    vTaskDelay(pdMS_TO_TICKS(2000));  // Espera 2 segundos
    
    int64_t now = get_time_ms();
    
    // Protege acceso con mutex
    xSemaphoreTake(ctx->session_mutex, pdMS_TO_TICKS(100));
    
    // Verifica cada sesión
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (ctx->sessions[i].authenticated && 
            (now - ctx->sessions[i].last_activity > SESSION_TIMEOUT_MS)) {
            // Sesión expirada (>3 minutos de inactividad)
            ctx->sessions[i].authenticated = false;
            gpio_set_yellow(false);  // Apaga LEDs por seguridad
            gpio_set_blue(false);
        }
    }
    
    xSemaphoreGive(ctx->session_mutex);
}
```

**Propósito:**
- Expira sesiones inactivas automáticamente
- Apaga LEDs cuando una sesión expira (seguridad)
- Libera slots de sesión para nuevos usuarios
- Previene sesiones huérfanas que consuman recursos

**Timeout:**
- `SESSION_TIMEOUT_MS = 3 * 60 * 1000` (3 minutos)

---

## Resumen del Flujo Completo

### Al Iniciar el ESP32:

1. **`app_main()` se ejecuta**
   - Inicializa NVS
   - Inicializa GPIO (LEDs)
   - Inicializa sensor NTC y crea tarea de lectura
   - Inicializa WiFi (SoftAP)
   - Inicia servidor web

2. **`start_webserver()` se ejecuta**
   - Crea mutexes y colas
   - Crea tareas (comandos y sesiones)
   - Monta SPIFFS
   - Inicia servidor HTTP
   - Registra todas las rutas

3. **Tareas en segundo plano comienzan:**
   - `ntc_reader`: Lee temperatura cada 1 segundo
   - `gpio_cmd_task`: Espera comandos en la cola
   - `session_mgmt`: Verifica sesiones cada 2 segundos

### Cuando un Usuario se Conecta:

1. **El usuario se conecta a WiFi:**
   - SSID: `"ESP32_Server"`
   - Contraseña: `"12345678"`

2. **El usuario abre el navegador:**
   - URL: `http://192.168.4.1/`

3. **El servidor sirve el login:**
   - `GET /` → `root_get_handler()` → `login.html`

4. **El usuario realiza el login:**
   - `POST /login` → Se validan las credenciales → Se crea la sesión

5. **El usuario accede a la terminal:**
   - `GET /terminal` → `terminal.html`

6. **El usuario ejecuta un comando:**
   - `GET /cmd?c=led y on`
   - El handler envía el comando a la cola
   - La tarea procesa el comando
   - La tarea envía la respuesta a la cola
   - El handler recibe la respuesta y la envía al cliente

### Flujo de Datos de Temperatura:

1. **Tarea `ntc_reader` lee sensor:**
   - Cada 1 segundo
   - Calcula temperatura
   - Almacena en `current_ntc_data` (protegido por mutex)

2. **El cliente solicita la temperatura:**
   - `GET /temperature`
   - El handler lee `current_ntc_data`
   - Se responde con JSON: `{"temperature": 25.5}`

---

## Diagrama de Arquitectura

```
┌─────────────────────────────────────────────────────────────┐
│                        ESP32                                │
│                                                             │
│  ┌──────────────┐                                           │
│  │   app_main() │                                           │
│  └──────┬───────┘                                           │
│         │                                                   │
│         ├─→ NVS Init                                        │
│         ├─→ GPIO Init (LEDs)                                │
│         ├─→ NTC Init + Task                                 │
│         ├─→ WiFi SoftAP                                     │
│         └─→ Web Server Start                                │
│                                                             │
│  ┌──────────────────────────────────────────────────────┐   │
│  │              start_webserver()                       │   |
│  │  ┌──────────────┐  ┌──────────────┐                  |   │
│  │  │   Mutexes    │  │    Colas      │                 │   │
│  │  │ - sessions   │  │ - commands    │                 │   │
│  │  │ - cmd_ids    │  │ - responses   │                 │   │
│  │  └──────────────┘  └──────────────┘                  |   │
│  │                                                      │   │
│  │  ┌──────────────────────────────────────────────┐    |   │
│  │  │         Tareas FreeRTOS                      │    │   │
│  │  │  - gpio_cmd_task (prioridad 5)               │    │   │
│  │  │  - session_mgmt (prioridad 3)                │    │   │
│  │  └──────────────────────────────────────────────┘    │   │
│  │                                                      │   │
│  │  ┌──────────────────────────────────────────────┐    │   │
│  │  │         Servidor HTTP                        │    │   │
│  │  │  - Rutas registradas                         │    │   │
│  │  │  - Handlers HTTP                             │    │   │
│  │  └──────────────────────────────────────────────┘    │   │
│  │                                                      │   │
│  └──────────────────────────────────────────────────────┘   │
│                                                             │
│  ┌──────────────────────────────────────────────────────┐   │
│  │         Tarea: ntc_reader (prioridad 5)              │   │
│  │  - Lee temperatura cada 1 segundo                    │   │
│  │  - Almacena en current_ntc_data (mutex)              │   │
│  └──────────────────────────────────────────────────────┘   │
│                                                             │
└─────────────────────────────────────────────────────────────┘
         │                    │
         │ WiFi               │ GPIO
         ▼                    ▼
    ┌─────────┐          ┌─────────┐
    │ Cliente │          │  LEDs   │
    │  Web    │          │  NTC    │
    └─────────┘          └─────────┘
```

---

## Sistema de Autenticación

### Estructura de Sesión

```c
typedef struct {
    char ip[16];              // IP del cliente
    int64_t last_activity;     // Timestamp (ms desde inicio)
    bool authenticated;         // true si está autenticado
} session_t;
```

### Credenciales

- **Usuario:** `"root"`
- **Contraseña:** `"matrix123"`

### Timeout

- **Inactividad:** 3 minutos
- **Verificación:** Cada 2 segundos (tarea `session_mgmt`)
- **Acción al expirar:** Apaga LEDs automáticamente

### Protección

- **Mutex:** `session_mutex` protege acceso a `sessions[]`
- **Máximo de sesiones:** 5 simultáneas
- **Verificación:** En cada petición a rutas protegidas

---

## Estructura de Archivos

```
main/
├── main.c                 # Punto de entrada (app_main)
├── wifi_app.c/h          # Configuración WiFi SoftAP
├── web_server.c/h        # Servidor HTTP y rutas
├── terminal_commands.c/h # Procesamiento de comandos
├── gpio_driver.c/h       # Control de LEDs (GPIO)
├── ntc_sensor.c/h        # Sensor de temperatura
└── registros.c/h         # Gestión de registros (SPIFFS)

front/                     # Archivos del frontend (SPIFFS)
├── index.html
├── login.html
├── dashboard.html
├── terminal.html
├── slider.html
├── style.css
└── script.js
```

---

## Puntos Clave

1. **Arquitectura Multi-tarea:**
   - Múltiples tareas de FreeRTOS ejecutándose simultáneamente
   - Comunicación mediante colas (queues) thread-safe
   - Protección de recursos compartidos con mutexes

2. **Servidor HTTP Asíncrono:**
   - Los handlers no bloquean esperando procesamiento
   - Comandos se procesan en tarea separada
   - Permite múltiples conexiones simultáneas

3. **Persistencia:**
   - SPIFFS para archivos estáticos y registros
   - NVS para configuración del sistema

4. **Seguridad:**
   - Autenticación basada en sesiones
   - Timeout automático de sesiones inactivas
   - LEDs se apagan automáticamente al expirar sesión

5. **Hardware:**
   - GPIO 2: LED Amarillo
   - GPIO 5: LED Azul
   - GPIO 32 (ADC1_CH4): Sensor NTC

---

## Notas Finales

- El código está diseñado para ser modular y fácil de mantener
- Cada módulo tiene responsabilidades claras y bien definidas
- La comunicación entre módulos se hace a través de interfaces bien definidas
- El uso de mutexes y colas garantiza thread-safety en un entorno multi-tarea
- El sistema es robusto y maneja errores de forma apropiada
