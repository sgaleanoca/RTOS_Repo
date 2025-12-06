# Project 7: Retro Web Terminal

Terminal web retro estilo Matrix para ESP32 con autenticación, control de LEDs, sensor de temperatura y servidor web embebido.

## Descripción General

El proyecto implementa un servidor web retro estilo terminal en un ESP32 que permite controlar hardware mediante una interfaz web moderna. El sistema proporciona:

- **Acceso Point WiFi (SoftAP)**: El ESP32 crea su propia red WiFi para acceso directo
- **Interfaz Web Retro**: Terminal estilo Matrix con estética retro y efectos visuales
- **Autenticación Segura**: Sistema de login con sesiones basadas en IP y timeout automático
- **Control de Hardware**: Comandos para controlar LEDs (amarillo y azul) mediante GPIO
- **Sensor de Temperatura**: Lectura en tiempo real mediante sensor NTC
- **Almacenamiento SPIFFS**: Archivos HTML/CSS/JS almacenados en partición SPIFFS del flash
- **Arquitectura RTOS**: Sistema basado en FreeRTOS con tareas concurrentes y colas de mensajes

## Arquitectura del Sistema

### Diagrama de Arquitectura General

```
┌─────────────────────────────────────────────────────────────────────┐
│                         ESP32 SYSTEM                                │
│                                                                     │
│  ┌─────────────────────────────────────────────────────────────┐    │
│  │                    FreeRTOS Kernel                          │    │
│  │                                                             |    │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐       │    │
│  │  │  Main Task   │  │ GPIO Command │  │  Session     │       │    │
│  │  │  (app_main)  │  │    Task      │  │  Management   │      │    │
│  │  │              │  │              │  │     Task      │      │    │
│  │  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘       │    │
│  │         │                 │                  │              │    │
│  │         │                 │                  │              │    │
│  │  ┌──────▼─────────────────▼──────────────────▼──────┐       │    │
│  │  │         Colas de Mensajes (FreeRTOS)             │       │    │
│  │  │  • gpio_command_queue                            │       │    │
│  │  │  • gpio_response_queue                           │       │    │
│  │  └──────────────────────────────────────────────────┘       │    │
│  │                                                             │    │
│  └─────────────────────────────────────────────────────────────┘    │
│                                                                     │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐               │
│  │  WiFi SoftAP │  │ HTTP Server  │  │  SPIFFS      │               │
│  │  (wifi_app)  │  │(web_server)  │  │  Filesystem  │               │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘               │
│         │                 │                 │                       │
│         └─────────────────┴─────────────────┘                       │
│                            │                                        │
│                            ▼                                        │
│                  ┌─────────────────┐                                │
│                  │  Port 80        │                                │
│                  │  HTTP/1.1       │                                │
│                  └────────┬────────┘                                │
└───────────────────────────┼─────────────────────────────────────────┘
                            │
                            │ WiFi Network (192.168.4.1)
                            │
                            ▼
                    ┌───────────────┐
                    │   Cliente     │
                    │   Web Browser │
                    └───────────────┘
```

### Componentes del Sistema

El sistema está compuesto por los siguientes módulos principales:

1. **Módulo WiFi (wifi_app.c/h)**: Configura el ESP32 como Access Point, creando una red WiFi independiente
2. **Módulo Servidor Web (web_server.c/h)**: Implementa el servidor HTTP con manejo de sesiones y rutas
3. **Módulo GPIO (gpio_driver.c/h)**: Controla los LEDs mediante pines GPIO configurados
4. **Módulo Sensor NTC (ntc_sensor.c/h)**: Lee temperatura mediante termistor NTC 10k
5. **Frontend Web (front/)**: Interfaz de usuario con terminal retro y dashboard

## Flujo de Inicialización del Sistema

```
┌─────────────────────────────────────────────────────────────────┐
│                    INICIO DEL PROGRAMA                          │
│                      (app_main)                                 │
└────────────────────────────┬────────────────────────────────────┘
                             │
                             ▼
                    ┌─────────────────┐
                    │ 1. Inicializar  │
                    │      NVS        │
                    │  (nvs_flash)    │
                    │                 │
                    │ • Erase si      │
                    │   necesario     │
                    │ • Init flash    │
                    └────────┬────────┘
                             │
                             ▼
                    ┌─────────────────┐
                    │ 2. Inicializar  │
                    │    Hardware     │
                    │                 │
                    │ • GPIO LEDs     │
                    │ • Sensor NTC    │
                    │ • Tarea lectura │
                    │   temperatura   │
                    └────────┬────────┘
                             │
                             ▼
                    ┌─────────────────┐
                    │ 3. Inicializar  │
                    │   WiFi SoftAP   │
                    │  (wifi_app.c)   │
                    │                 │
                    │ • Configurar    │
                    │   SSID/Pass     │
                    │ • Iniciar AP    │
                    │ • Asignar IP    │
                    └────────┬────────┘
                             │
                    ┌────────┴────────┐
                    │ Red WiFi Creada │
                    │                 │
                    │ SSID: ESP32_    │
                    │      Server     │
                    │ Pass: 12345678  │
                    │ IP: 192.168.4.1 │
                    └────────┬────────┘
                             │
                             ▼
                    ┌─────────────────┐
                    │ 4. Inicializar  │
                    │  Servidor Web   │
                    │ (web_server.c)  │
                    └────────┬────────┘
                             │
                    ┌────────┴────────┐
                    │ 4.1. Montar     │
                    │     SPIFFS      │
                    │  (/spiffs)      │
                    │                 │
                    │ • Verificar     │
                    │   partición     │
                    │ • Montar FS     │
                    └────────┬────────┘
                             │
                    ┌────────┴────────┐
                    │ 4.2. Verificar  │
                    │   archivos web  │
                    │                 │
                    │ • index.html    │
                    │ • login.html    │
                    │ • style.css     │
                    │ • script.js     │
                    └────────┬────────┘
                             │
                    ┌────────┴────────┐
                    │ 4.3. Crear      │
                    │   Colas RTOS    │
                    │                 │
                    │ • command_queue │
                    │ • response_queue│
                    │ • mutex sesiones│
                    └────────┬────────┘
                             │
                    ┌────────┴────────┐
                    │ 4.4. Crear      │
                    │   Tareas RTOS   │
                    │                 │
                    │ • GPIO task     │
                    │ • Session task  │
                    └────────┬────────┘
                             │
                    ┌────────┴────────┐
                    │ 4.5. Registrar  │
                    │   Handlers HTTP │
                    │                 │
                    │ • GET /         │
                    │ • GET /login    │
                    │ • POST /login   │
                    │ • GET /cmd      │
                    │ • GET /temp     │
                    │ • GET /*.html   │
                    │ • GET /*.css    │
                    │ • GET /*.js     │
                    └────────┬────────┘
                             │
                    ┌────────┴────────┐
                    │ 4.6. Iniciar    │
                    │  HTTP Server    │
                    │  (Puerto 80)    │
                    └────────┬────────┘
                             │
                             ▼
        ┌───────────────────────────────────────────────┐
        │         SERVIDOR WEB ACTIVO                   │
        │  Esperando conexiones en 192.168.4.1:80       │
        │                                               │
        │  • Máximo 5 sesiones simultáneas              │
        │  • Timeout: 3 minutos de inactividad          │
        │  • Sistema de autenticación activo            │
        └───────────────────────────────────────────────┘
```

## Flujo de Autenticación

```
┌─────────────────────────────────────────────────────────────────┐
│                    Cliente se conecta                           │
│                    a 192.168.4.1                                │
└────────────────────────────┬────────────────────────────────────┘
                             │
                             ▼
                    ┌─────────────────┐
                    │  GET /          │
                    │  (HTTP Request) │
                    └────────┬────────┘
                             │
                             ▼
                    ┌─────────────────┐
                    │ Extraer IP del  │
                    │    cliente      │
                    │ (get_client_ip) │
                    └────────┬────────┘
                             │
                             ▼
                    ┌─────────────────┐
                    │ Buscar sesión   │
                    │ por IP          │
                    │ (find_or_create)│
                    └────────┬────────┘
                             │
                    ┌────────┴────────┐
                    │                 │
                    ▼                 ▼
            ┌──────────────┐  ┌──────────────┐
            │ Sesión       │  │ No existe    │
            │ encontrada   │  │ sesión       │
            └──────┬───────┘  └──────┬───────┘
                   │                 │
                   ▼                 ▼
            ┌──────────────┐ ┌───────────────┐
            │ ¿Autenticada?│ │ Crear nueva   │
            │              │ │ sesión        │
            └──────┬───────┘ │ (authenticated│
                   │         │  = false)     │
            ┌──────┴──────┐  └──────┬────────┘
            │             │         │
            ▼             ▼         │
    ┌───────────┐  ┌───────────┐    │
    │   SÍ      │  │    NO     │    │
    └─────┬─────┘  └─────┬─────┘    │
          │              │          │
          │              └─────┬────┘
          │                    │
          │                    ▼
          │          ┌─────────────────┐
          │          │ Servir          │
          │          │ login.html      │
          │          │ desde SPIFFS    │
          │          └────────┬────────┘
          │                   │
          │                   ▼
          │          ┌─────────────────┐
          │          │ Usuario ingresa │
          │          │ credenciales    │
          │          └────────┬────────┘
          │                   │
          │                   ▼
          │          ┌─────────────────┐
          │          │ POST /login     │
          │          │ user=root       │
          │          │ pass=matrix123  │
          │          └────────┬────────┘
          │                   │
          │                   ▼
          │          ┌─────────────────┐
          │          │ Validar         │
          │          │ credenciales    │
          │          └────────┬────────┘
          │                   │
          │          ┌────────┴──────────┐
          │          │                   │
          │          ▼                   ▼
          │  ┌───────────┐      ┌───────────┐
          │  │ Válidas   │      │ Inválidas │
          │  └─────┬─────┘      └─────┬─────┘
          │        │                  │
          │        │                  ▼
          │        │          ┌──────────────┐
          │        │          │ HTTP 401     │
          │        │          │ Unauthorized │
          │        │          └──────────────┘
          │        │
          │        ▼
          │  ┌─────────────────┐
          │  │ Marcar sesión   │
          │  │ como autenticada│
          │  │ (authenticated= │
          │  │  true)          │
          │  │ Actualizar      │
          │  │ last_activity   │
          │  └────────┬────────┘
          │           │
          │           ▼
          │  ┌─────────────────┐
          │  │ HTTP 200 OK     │
          │  └────────┬────────┘
          │           │
          └───────────┘
                     │
                     ▼
            ┌─────────────────┐
            │ Redirigir a     │
            │ /dashboard o /  │
            └────────┬────────┘
                     │
                     ▼
            ┌─────────────────┐
            │ Servir          │
            │ index.html      │
            │ (Terminal)      │
            └─────────────────┘
```

## Flujo de Procesamiento de Comandos

```
┌─────────────────────────────────────────────────────────────────┐
│                    Usuario escribe comando                      │
│                    en terminal web                              │
└────────────────────────────┬────────────────────────────────────┘
                             │
                             ▼
                    ┌─────────────────┐
                    │ JavaScript      │
                    │ (script.js)     │
                    │                 │
                    │ • Captura input │
                    │ • Valida formato│
                    └────────┬────────┘
                             │
                             ▼
                    ┌─────────────────┐
                    │ GET /cmd?c=     │
                    │ comando         │
                    │                 │
                    │ Ejemplo:        │
                    │ /cmd?c=led%20y% │
                    │ 20on            │
                    └────────┬────────┘
                             │
                             ▼
                    ┌───────────────────┐
                    │ Verificar         │
                    │ autenticación     │
                    │ (is_authenticated)│
                    └────────┬──────────┘
                             │
                    ┌────────┴────────┐
                    │                 │
                    ▼                 ▼
            ┌──────────────┐  ┌───────────────┐
            │ Autenticado  │  │ No autenticado│
            └──────┬───────┘  └──────┬────────┘
                   │                 │
                   │                 ▼
                   │          ┌──────────────┐
                   │          │ HTTP 401     │
                   │          │ Redirigir    │
                   │          │ a /login     │
                   │          └──────────────┘
                   │
                   ▼
            ┌─────────────────┐
            │ Decodificar     │
            │ comando (URL)   │
            │                 │
            │ • URL decode    │
            │ • To lowercase  │
            └────────┬────────┘
                     │
                     ▼
            ┌─────────────────┐
            │ ¿Comando        │
            │ especial?       │
            │ (clear)         │
            └────────┬────────┘
                     │
            ┌────────┴────────┐
            │                 │
            ▼                 ▼
    ┌──────────────┐  ┌──────────────┐
    │   SÍ         │  │     NO       │
    │ (clear)      │  │              │
    └──────┬───────┘  └──────┬───────┘
           │                 │
           │                 ▼
           │          ┌─────────────────┐
           │          │ Generar ID      │
           │          │ único de comando│
           │          │ (command_id)    │
           │          └────────┬────────┘
           │                   │
           │                   ▼
           │          ┌─────────────────┐
           │          │ Crear estructura│
           │          │ gpio_command_t  │
           │          │                 │
           │          │ • command_id    │
           │          │ • command       │
           │          │ • response      │
           │          └────────┬────────┘
           │                   │
           │                   ▼
           │          ┌─────────────────┐
           │          │ Enviar a cola   │
           │          │ gpio_command_   │
           │          │ queue           │
           │          │ (xQueueSend)    │
           │          └────────┬────────┘
           │                   │
           │                   ▼
           │          ┌──────────────────┐
           │          │ Esperar respuesta│
           │          │ de gpio_response_│
           │          │ queue            │
           │          │ (xQueueReceive)  │
           │          │ con timeout      │
           │          └────────┬─────────┘
           │                   │
           │                   ▼
           │          ┌─────────────────┐
           │          │ Verificar       │
           │          │ command_id      │
           │          │ coincide        │
           │          └────────┬────────┘
           │                   │
           │                   ▼
           │          ┌─────────────────┐
           │          │ Enviar respuesta│
           │          │ HTTP al cliente │
           │          └─────────────────┘
           │
           ▼
    ┌─────────────────┐
    │ Respuesta       │
    │ inmediata       │
    │ "[OK] Pantalla  │
    │ limpiada."      │
    └─────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│              PROCESAMIENTO EN TAREA GPIO (RTOS)                 │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────┐
│ gpio_command_   │
│ task (loop)     │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ Esperar comando │
│ de cola         │
│ (xQueueReceive) │
│ (bloqueante)    │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ Parsear comando │
│                 │
│ • led y on      │
│ • led y off     │
│ • led b on      │
│ • led b off     │
│ • led all on    │
│ • led all off   │
│ • status        │
│ • help          │
└────────┬────────┘
         │
         ▼
┌──────────────────┐
│ Ejecutar acción  │
│                  │
│ • gpio_set_      │
│   yellow()       │
│ • gpio_set_blue()│
│ • gpio_get_      │
│   yellow()       │
│ • gpio_get_blue()│
└────────┬─────────┘
         │
         ▼
┌──────────────────┐
│ Generar respuesta│
│                  │
│ • "[OK] LED..."  │
│ • Estado LEDs    │
│ • Lista comandos │
└────────┬─────────┘
         │
         ▼
┌─────────────────┐
│ Enviar respuesta│
│ a cola          │
│ gpio_response_  │
│ queue           │
└─────────────────┘
```

## Gestión de Sesiones y Timeout

```
┌─────────────────────────────────────────────────────────────────┐
│              Tarea de Gestión de Sesiones (RTOS)                │
│              (session_management_task)                          │
└─────────────────────────────────────────────────────────────────┘

                    ┌─────────────────┐
                    │ Loop cada 2 seg │
                    │ (vTaskDelay)    │
                    └────────┬────────┘
                             │
                             ▼
                    ┌─────────────────┐
                    │ Obtener tiempo  │
                    │ actual (ms)     │
                    │ (get_time_ms)   │
                    └────────┬────────┘
                             │
                             ▼
                    ┌─────────────────┐
                    │ Tomar mutex     │
                    │ (session_mutex) │
                    └────────┬────────┘
                             │
                             ▼
                    ┌─────────────────┐
                    │ Iterar sobre    │
                    │ todas las       │
                    │ sesiones        │
                    │ (MAX_SESSIONS)  │
                    └────────┬────────┘
                             │
                    ┌────────┴────────┐
                    │                 │
                    ▼                 ▼
            ┌──────────────┐  ┌───────────────┐
            │ Sesión       │  │ Sesión        │
            │ autenticada  │  │ no autenticada│
            └──────┬───────┘  └───────────────┘
                   │
                   ▼
            ┌─────────────────┐
            │ Calcular tiempo │
            │ de inactividad  │
            │                 │
            │ timeout = now - │
            │ last_activity   │
            └────────┬────────┘
                     │
            ┌────────┴────────┐
            │                 │
            ▼                 ▼
    ┌──────────────┐  ┌──────────────┐
    │ timeout >    │  │ timeout <=   │
    │ 3 minutos    │  │ 3 minutos    │
    └──────┬───────┘  └──────────────┘
           │
           ▼
    ┌─────────────────┐
    │ Expirar sesión  │
    │                 │
    │ • authenticated │
    │   = false       │
    │ • Apagar LEDs   │
    │ • Log evento    │
    └────────┬────────┘
             │
             ▼
    ┌─────────────────┐
    │ Liberar mutex   │
    │ (xSemaphoreGive)│
    └─────────────────┘
```

## Diagrama de Interacción de Componentes

```
┌──────────────┐
│   Cliente    │
│   Web        │
└──────┬───────┘
       │ HTTP Requests
       │
       ▼
┌─────────────────────────────────────────────────────────────┐
│                    HTTP Server (web_server.c)               │
│                                                             │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐       │
│  │   Handler    │  │   Handler    │  │   Handler    │       │
│  │   GET /      │  │  POST /login │  │  GET /cmd    │       │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘       │
│         │                 │                  │              │
│         │                 │                  │              │
│         ▼                 ▼                  ▼              │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐       │
│  │ Verificar    │  │ Validar      │  │ Verificar    │       │
│  │ sesión       │  │ credenciales │  │ sesión       │       │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘       │
│         │                 │                  │              │
│         │                 │                  │              │
│         ▼                 ▼                  ▼              │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐       │
│  │ Servir desde │  │ Crear/       │  │ Enviar a     │       │
│  │ SPIFFS       │  │ Actualizar   |  │ cola comandos│       │
│  └──────────────┘  └──────────────┘  └──────┬───────┘       │
│                                                │            │
└────────────────────────────────────────────────┼────────────┘
                                                 │
                                                 ▼
                                    ┌─────────────────────┐
                                    │  gpio_command_queue │
                                    │  (FreeRTOS Queue)   │
                                    └──────────┬──────────┘
                                               │
                                               ▼
                                    ┌─────────────────────┐
                                    │  GPIO Command Task  │
                                    │  (FreeRTOS Task)    │
                                    └──────────┬──────────┘
                                               │
                                               ▼
                                    ┌─────────────────────┐
                                    │  gpio_driver.c      │
                                    │                     │
                                    │  • gpio_set_yellow()│
                                    │  • gpio_set_blue()  │
                                    │  • gpio_get_yellow()│
                                    │  • gpio_get_blue()  │
                                    └──────────┬──────────┘
                                               │
                                               ▼
                                    ┌─────────────────────┐
                                    │  Hardware GPIO      │
                                    │                     │
                                    │  • LED Amarillo     │
                                    │  • LED Azul         │
                                    └─────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│                    Session Management                       │
│                                                             │
│  ┌──────────────┐                                           │
│  │ Session Task │                                           │
│  │ (RTOS Task)  │                                           │
│  └──────┬───────┘                                           │
│         │                                                   │
│         ▼                                                   │
│  ┌──────────────┐                                           │
│  │ Verificar    │                                           │
│  │ timeouts     │                                           │
│  │ cada 2 seg   │                                           │
│  └──────┬───────┘                                           │
│         │                                                   │
│         ▼                                                   │
│  ┌──────────────┐                                           │
│  │ sessions[]   │                                           │
│  │ array        │                                           │
│  │ (MAX_SESSIONS│                                           │
│  │  = 5)        │                                           │
│  └──────────────┘                                           │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│                    NTC Sensor Module                        │
│                                                             │
│  ┌──────────────┐                                           │
│  │ NTC Reading  │                                           │
│  │ Task (RTOS)  │                                           │
│  └──────┬───────┘                                           │
│         │                                                   |
│         ▼                                                   │
│  ┌──────────────┐                                           │
│  │ ntc_sensor.c │                                           │
│  │              │                                           │
│  │ • ADC read   │                                           │
│  │ • Calculate  │                                           │
│  │   temp       │                                           │
│  └──────┬───────┘                                           │
│         │                                                   │
│         ▼                                                   │
│  ┌──────────────┐                                           │
│  │ GET /temp    │                                           │
│  │ Handler      │                                           │
│  └──────┬───────┘                                           │
│         │                                                   │
│         ▼                                                   │
│  ┌──────────────┐                                           │
│  │ JSON Response│                                           │
│  │ {"temp": XX} │                                           │
│  └──────────────┘                                           │
└─────────────────────────────────────────────────────────────┘
```

## Instrucciones de Flasheo

### Requisitos Previos

1. **ESP-IDF instalado y configurado**
   ```bash
   # Si no se tiene ESP-IDF instalado, se debe seguir la guía oficial:
   # https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/
   ```

2. **Hardware necesario**
   - ESP32 (cualquier variante compatible)
   - Cable USB para programación
   - LEDs conectados a los GPIOs configurados (opcional)
   - Sensor NTC 10k con resistencia en serie de 10k (opcional)

### Método 1: Script Automático (Recomendado)

El script `flash_all.sh` automatiza todo el proceso de flasheo:

```bash
# 1. Dar permisos de ejecución (solo la primera vez)
chmod +x flash_all.sh

# 2. Flashear el proyecto completo (incluyendo SPIFFS)
./flash_all.sh /dev/ttyUSB0

# Si el puerto es diferente, se debe especificar:
./flash_all.sh /dev/ttyACM0
```

**¿Qué hace el script?**
- Activa automáticamente el entorno ESP-IDF
- Construye el proyecto (`idf.py build`)
- Verifica que `storage.bin` se haya generado correctamente
- Flashea bootloader, particiones y aplicación
- Flashea la partición SPIFFS con los archivos web

### Método 2: Manual

Para realizar el flasheo paso a paso:

```bash
# 1. Activar entorno ESP-IDF
. $HOME/esp/esp-idf/export.sh

# 2. Construir el proyecto
idf.py build

# 3. Flashear firmware (bootloader, particiones, aplicación)
idf.py -p /dev/ttyUSB0 flash

# 4. Flashear SPIFFS (archivos web)
idf.py -p /dev/ttyUSB0 storage-flash

# O usando esptool.py directamente:
esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 921600 \
  --before default_reset --after hard_reset \
  write_flash --flash_mode dio --flash_freq 40m --flash_size 4MB \
  0x110000 build/storage.bin
```

### Verificación Post-Flasheo

Después de flashear, se debe ejecutar el monitor para verificar:

```bash
idf.py -p /dev/ttyUSB0 monitor
```

**Logs esperados:**
```
I (xxx) WIFI_APP: SoftAP iniciado. SSID: ESP32_Server Clave: 12345678 canal: 1
I (xxx) WEB_SERVER: SPIFFS montado correctamente en /spiffs
I (xxx) WEB_SERVER: SPIFFS: 1024 KB total, XX KB usado
I (xxx) WEB_SERVER: Archivo encontrado: /spiffs/index.html (XXX bytes)
I (xxx) WEB_SERVER: Archivo encontrado: /spiffs/login.html (XXX bytes)
I (xxx) WEB_SERVER: Archivo encontrado: /spiffs/style.css (XXX bytes)
I (xxx) WEB_SERVER: Archivo encontrado: /spiffs/script.js (XXX bytes)
I (xxx) WEB_SERVER: Todos los archivos encontrados correctamente (4/4)
I (xxx) WEB_SERVER: Servidor Web Iniciado correctamente en puerto 80
```

### Solución de Problemas

**Error: "Puerto ocupado"**
```bash
# Se debe cerrar cualquier monitor o programa usando el puerto
# Luego se debe intentar de nuevo
./flash_all.sh /dev/ttyUSB0
```

**Error: "Archivos NO encontrados en SPIFFS"**
```bash
# Se debe asegurar de flashear SPIFFS explícitamente:
idf.py -p /dev/ttyUSB0 storage-flash
```

**Error: "idf.py: command not found"**
```bash
# Se debe activar el entorno ESP-IDF:
. $HOME/esp/esp-idf/export.sh
# O el script lo hará automáticamente
```

## Estructura del Proyecto

```
Project_7_Retro_Web_Terminal/
├── CMakeLists.txt          # Configuración del proyecto y SPIFFS
├── partitions.csv          # Tabla de particiones (incluye SPIFFS)
├── sdkconfig              # Configuración de ESP-IDF
├── flash_all.sh           # Script automático de flasheo
├── README.md              # Archivo de documentación
│
├── front/                 # Archivos web (se copian a SPIFFS)
│   ├── index.html        # Terminal principal (requiere login)
│   ├── login.html        # Página de login
│   ├── dashboard.html    # Dashboard de control
│   ├── slider.html       # Control con slider
│   ├── style.css         # Estilos retro estilo Matrix
│   └── script.js         # Lógica del terminal web
│
└── main/                  # Código fuente del ESP32
    ├── CMakeLists.txt
    ├── main.c            # Punto de entrada (app_main)
    ├── wifi_app.c        # Configuración WiFi SoftAP
    ├── wifi_app.h        # Definiciones WiFi
    ├── web_server.c      # Servidor HTTP y lógica web
    ├── web_server.h      # Headers del servidor
    ├── gpio_driver.c     # Control de LEDs
    ├── gpio_driver.h     # Headers GPIO
    ├── ntc_sensor.c      # Sensor de temperatura NTC
    └── ntc_sensor.h      # Headers sensor NTC
```

## Credenciales por Defecto

- **SSID WiFi**: `ESP32_Server`
- **Contraseña WiFi**: `12345678`
- **IP del ESP32**: `192.168.4.1`
- **Usuario Web**: `root`
- **Contraseña Web**: `matrix123`

> **Importante**: Se deben cambiar estas credenciales en producción.

## Comandos Disponibles

Una vez autenticado, el usuario puede usar los siguientes comandos en el terminal web:

### Control Individual de LEDs
- `led y on` - Enciende el LED amarillo
- `led y off` - Apaga el LED amarillo
- `led b on` - Enciende el LED azul
- `led b off` - Apaga el LED azul

### Control General
- `led all on` - Enciende ambos LEDs
- `led all off` - Apaga ambos LEDs

### Sistema
- `status` - Muestra el estado actual de los LEDs
- `help` - Muestra la lista de comandos disponibles
- `clear` - Limpia la pantalla del terminal

## Configuración

### Cambiar Credenciales WiFi

Se debe editar el archivo `main/wifi_app.h`:
```c
#define ESP_WIFI_SSID      "Tu_SSID"
#define ESP_WIFI_PASS      "Tu_Contraseña"
#define ESP_WIFI_CHANNEL   1
```

### Cambiar Credenciales Web

Se debe editar el archivo `main/web_server.c`:
```c
#define VALID_USER "tu_usuario"
#define VALID_PASS "tu_contraseña"
```

### Modificar Archivos Web

1. Se deben editar los archivos en `front/`
2. Se debe reconstruir y flashear:
   ```bash
   ./flash_all.sh /dev/ttyUSB0
   ```

## Tabla de Particiones

```
# Name,   Type, SubType, Offset,  Size
nvs,      data, nvs,     ,        0x6000
phy_init, data, phy,     ,        0x1000
factory,  app,  factory, ,        1M
storage,  data, spiffs,  ,        1M    ← Archivos web aquí
```

## Troubleshooting

### Los archivos web no se cargan
- Se debe verificar que SPIFFS se haya flasheado: `idf.py -p PORT storage-flash`
- Se deben revisar los logs del monitor para ver errores de SPIFFS

### No se puede conectar al WiFi
- Se debe verificar que el ESP32 esté encendido
- Se debe revisar que el SSID sea "ESP32_Server"
- Se debe asegurar de usar la contraseña correcta: "12345678"

### El servidor web no responde
- Se debe verificar en los logs que el servidor se haya iniciado
- Se debe asegurar de estar conectado a la red WiFi del ESP32
- Se debe acceder a `http://192.168.4.1`

### Sesión expirada constantemente
- Las sesiones expiran después de 3 minutos de inactividad
- Se debe volver a hacer login si la sesión expira

## Notas Técnicas

### Sistema de Sesiones
- **Método**: Basado en dirección IP del cliente
- **Capacidad**: Máximo 5 sesiones simultáneas
- **Timeout**: 3 minutos de inactividad
- **Protección**: Mutex para acceso thread-safe a estructura de sesiones
- **Gestión**: Tarea RTOS dedicada que verifica timeouts cada 2 segundos

### Procesamiento de Comandos
- **Arquitectura**: Sistema de colas FreeRTOS para comunicación entre tareas
- **Colas**: 
  - `gpio_command_queue`: Comandos del servidor HTTP a la tarea GPIO
  - `gpio_response_queue`: Respuestas de la tarea GPIO al servidor HTTP
- **Identificación**: Cada comando tiene un ID único para emparejar comando-respuesta
- **Tarea Dedicada**: Tarea RTOS separada procesa comandos GPIO de forma asíncrona

### Sistema de Archivos
- **SPIFFS**: Sistema de archivos en flash, 1MB de capacidad
- **Montaje**: Se monta en `/spiffs` durante la inicialización
- **Verificación**: El sistema verifica que todos los archivos web estén presentes al iniciar

### Servidor HTTP
- **Puerto**: 80 (HTTP estándar)
- **Conexiones**: Soporta hasta 7 conexiones simultáneas
- **Protocolo**: HTTP/1.1
- **Handlers**: Múltiples handlers para diferentes rutas (GET /, POST /login, GET /cmd, GET /temp, etc.)

### FreeRTOS
- **Kernel**: El programa corre sobre FreeRTOS
- **Tareas**:
  - Tarea principal (app_main)
  - Tarea de procesamiento de comandos GPIO
  - Tarea de gestión de sesiones
  - Tarea de lectura de temperatura NTC
- **Sincronización**: Mutex y colas para comunicación entre tareas

### Sensor de Temperatura
- **Tipo**: Sensor NTC 10k (termistor)
- **ADC**: Utiliza ADC1, canal 4 (GPIO32)
- **Lectura**: Tarea RTOS lee temperatura periódicamente
- **API**: Endpoint `/temp` devuelve temperatura en formato JSON

---

