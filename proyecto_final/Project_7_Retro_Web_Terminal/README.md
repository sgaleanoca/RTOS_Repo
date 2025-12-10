# Project 7: Retro Web Terminal

Terminal web retro estilo Matrix para ESP32 con autenticación, control de hardware (LED RGB, ventilador PWM), sensores (temperatura NTC, presencia PIR), sincronización de tiempo y servidor web embebido.

## Descripción General

El proyecto implementa un servidor web retro estilo terminal en un ESP32 que permite controlar hardware mediante una interfaz web moderna. El sistema proporciona:

- **Access Point + Station WiFi (AP+STA)**: El ESP32 crea su propia red WiFi local y se conecta a Internet
- **Interfaz Web Retro**: Terminal estilo Matrix con estética retro y efectos visuales
- **Autenticación Segura**: Sistema de login con sesiones basadas en IP y timeout automático
- **Control de Hardware**: 
  - LED RGB verde (GPIO 27, PWM)
  - Ventilador con control PWM (GPIO 26, múltiples modos)
- **Sensores**:
  - Sensor de temperatura NTC 10k (GPIO 32, ADC1)
  - Sensor PIR de presencia (GPIO 12)
- **Sincronización de Tiempo**: SNTP para obtener hora actual y control por horarios
- **Sistema de Registros**: Gestión de horarios del ventilador con persistencia en SPIFFS
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
│  │  │  (app_main)  │  │    Task      │  │  Management  │       │    │
│  │  │              │  │              │  │     Task     │       │    │
│  │  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘       │    │
│  │         │                 │                  │              │    │
│  │  ┌──────▼───────┐  ┌──────▼───────┐  ┌──────▼───────┐       │    │
│  │  │ NTC Reader   │  │ Fan Auto Temp│  │ Fan Schedule │       │    │
│  │  │    Task      │  │     Task     │  │     Task     │       │    │
│  │  └──────────────┘  └──────────────┘  └──────────────┘       │    │
│  │                                                             │    │
│  │  ┌──────▼─────────────────▼──────────────────▼──────┐       │    │
│  │  │         Colas de Mensajes (FreeRTOS)             │       │    │
│  │  │  • gpio_command_queue                            │       │    │
│  │  │  • gpio_response_queue                           │       │    │
│  │  └──────────────────────────────────────────────────┘       │    │
│  │                                                             │    │
│  └─────────────────────────────────────────────────────────────┘    │
│                                                                     │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐               │
│  │  WiFi AP+STA │  │ HTTP Server  │  │  SPIFFS      │               │
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

1. **Módulo WiFi (wifi_app.c/h)**: Configura el ESP32 como Access Point + Station (AP+STA)
2. **Módulo Servidor Web (web_server.c/h)**: Implementa el servidor HTTP con manejo de sesiones y rutas
3. **Módulo Terminal (terminal_commands.c/h)**: Procesa comandos de la terminal web
4. **Módulo LED RGB (rgb_led.c/h)**: Control PWM del LED RGB verde
5. **Módulo Ventilador (fan_control.c/h)**: Control PWM del ventilador con múltiples modos
6. **Módulo Sensor NTC (ntc_sensor.c/h)**: Lee temperatura mediante termistor NTC 10k
7. **Módulo Sensor PIR (pir_driver.c/h)**: Detecta presencia mediante sensor PIR
8. **Módulo Sincronización de Tiempo (time_sync.c/h)**: Sincroniza hora mediante SNTP
9. **Módulo Registros (registros.c/h)**: Gestiona registros de horarios del ventilador
10. **Frontend Web (front/)**: Interfaz de usuario con terminal retro y dashboard

## Hardware

### Pines GPIO Configurados

| GPIO | Función | Descripción |
|------|---------|-------------|
| GPIO 27 | LED RGB Verde | Control PWM mediante LEDC Channel 1, Timer 0 |
| GPIO 26 | Ventilador | Control PWM mediante LEDC Channel 2, Timer 1 |
| GPIO 32 | Sensor NTC | Lectura ADC1 Channel 4 (divisor de voltaje) |
| GPIO 12 | Sensor PIR | Entrada digital con interrupciones |

### Especificaciones

- **LED RGB**: PWM 8-bit, 5kHz, control por porcentaje (0-100%)
- **Ventilador**: PWM 8-bit, 25kHz, control por porcentaje (0-100%)
- **Sensor NTC**: Termistor 10k, resistencia en serie 10k, ecuación Steinhart-Hart
- **Sensor PIR**: Detección de movimiento mediante interrupciones GPIO

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
                    └────────┬────────┘
                             │
                             ▼
                    ┌─────────────────┐
                    │ 2. Inicializar  │
                    │    Hardware     │
                    │                 │
                    │ • LED RGB       │
                    │ • Sensor NTC    │
                    │ • Sensor PIR    │
                    │ • Ventilador    │
                    │ • Tareas RTOS   │
                    └────────┬────────┘
                             │
                             ▼
                    ┌─────────────────┐
                    │ 3. Inicializar  │
                    │   WiFi AP+STA   │
                    │  (wifi_app.c)   │
                    │                 │
                    │ • Access Point  │
                    │ • Station       │
                    └────────┬────────┘
                             │
                    ┌────────┴────────┐
                    │ Red WiFi Creada │
                    │                 │
                    │ AP: ESP32_      │
                    │      Server     │
                    │ STA: Mondongo   │
                    └────────┬────────┘
                             │
                             ▼
                    ┌─────────────────┐
                    │ 4. Sincronizar  │
                    │      Tiempo     │
                    │  (time_sync.c)  │
                    │                 │
                    │ • SNTP          │
                    │ • Zona horaria  │
                    └────────┬────────┘
                             │
                             ▼
                    ┌─────────────────┐
                    │ 5. Inicializar  │
                    │  Servidor Web   │
                    │ (web_server.c)  │
                    └────────┬────────┘
                             │
                    ┌────────┴────────┐
                    │ 5.1. Montar     │
                    │     SPIFFS      │
                    │  (/spiffs)      │
                    └────────┬────────┘
                             │
                    ┌────────┴────────┐
                    │ 5.2. Crear      │
                    │   Colas RTOS    │
                    │                 │
                    │ • command_queue │
                    │ • response_queue│
                    │ • mutex sesiones│
                    │ • mutex cmd_ids │
                    └────────┬────────┘
                             │
                    ┌────────┴────────┐
                    │ 5.3. Crear      │
                    │   Tareas RTOS   │
                    │                 │
                    │ • GPIO task     │
                    │ • Session task  │
                    └────────┬────────┘
                             │
                    ┌────────┴────────┐
                    │ 5.4. Registrar  │
                    │   Handlers HTTP │
                    │                 │
                    │ • Páginas web   │
                    │ • API REST      │
                    │ • Autenticación │
                    └────────┬────────┘
                             │
                    ┌────────┴────────┐
                    │ 5.5. Iniciar    │
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
        │  • Control de ventilador activo               │
        │  • Sensores funcionando                       │
        └───────────────────────────────────────────────┘
```

## Rutas API Disponibles

### Páginas Web
- `GET /` - Redirige a login o dashboard según autenticación
- `GET /dashboard` - Panel principal (requiere autenticación)
- `GET /terminal` - Terminal web retro (requiere autenticación)
- `GET /slider` - Panel de control con temperatura y ventilador (requiere autenticación)

### Archivos Estáticos
- `GET /style.css` - Archivo CSS
- `GET /script.js` - Archivo JavaScript
- `GET /favicon.ico` - Respuesta 204 No Content

### Autenticación
- `POST /login` - Validar credenciales y crear sesión
- `GET /logout` - Cerrar sesión y apagar LEDs

### API de Comandos
- `GET /cmd?c=comando` - Ejecutar comando de terminal

### API de Temperatura
- `GET /temperature` - Obtener temperatura actual (JSON)

### API de Ventilador
- `POST /fan/mode` - Establecer modo del ventilador (off, manual, temperature, schedule)
- `POST /fan/manual` - Establecer velocidad manual (0-100%)
- `GET /fan/status` - Obtener estado del ventilador (JSON)
- `GET /fan/diagnostic` - Diagnóstico completo del sistema (JSON)

### API de Registros
- `GET /registros` - Leer todos los registros (JSON)
- `POST /registros` - Guardar nuevo registro (JSON)

### API de Sensor PIR
- `GET /pir/status` - Obtener estado del sensor PIR (JSON)

### API de Tiempo
- `POST /time/set` - Establecer hora manualmente (JSON)

## Comandos Disponibles en la Terminal

Una vez autenticado, el usuario puede usar los siguientes comandos en el terminal web:

### Control de LED RGB
- `led on` - Enciende el LED RGB verde (100% brillo)
- `led off` - Apaga el LED RGB verde (0% brillo)
- `led <0-100>` - Establece el brillo del LED RGB verde (0-100%)

### Control de Ventilador
- `fan on` - Enciende el ventilador al 50% de velocidad (modo manual)
- `fan off` - Apaga el ventilador (modo OFF)
- `fan <0-100>` - Establece la velocidad del ventilador manualmente (0-100%, modo manual)

### Sistema
- `status` - Muestra el estado actual del sistema (LED RGB y ventilador)
- `help` - Muestra la lista de comandos disponibles
- `clear` - Limpia la pantalla del terminal

## Modos de Operación del Ventilador

El ventilador soporta 4 modos de operación:

1. **FAN_MODE_OFF**: Ventilador apagado (0% PWM)
2. **FAN_MODE_MANUAL**: Control manual por porcentaje (0-100%), ignora sensor PIR
3. **FAN_MODE_AUTO_TEMP**: Control automático basado en temperatura (15-25°C), requiere presencia PIR
4. **FAN_MODE_SCHEDULE**: Control por horarios usando registros guardados, requiere presencia PIR

### Control Automático por Temperatura

- **Temperatura mínima**: 15°C (0% PWM)
- **Temperatura máxima**: 25°C (100% PWM)
- **Mapeo**: Lineal entre 15°C y 25°C
- **Actualización**: Cada 1 segundo mediante tarea RTOS

### Control por Horarios (Registros)

- Los registros se guardan en `/spiffs/registros.json`
- Formato: `{"dia": "lunes", "hora": "14:30", "velocidad": 50}`
- Verificación: Cada 10 segundos mediante tarea RTOS
- Requiere: Hora sincronizada mediante SNTP

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
   - LED RGB conectado a GPIO 27 (opcional)
   - Ventilador con MOSFET en GPIO 26 (opcional)
   - Sensor NTC 10k con resistencia en serie de 10k en GPIO 32 (opcional)
   - Sensor PIR en GPIO 12 (opcional)

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
I (xxx) WIFI_APP: WiFi iniciado en modo AP+STA
I (xxx) WIFI_APP: Access Point: SSID=ESP32_Server, Clave=12345678, Canal=1
I (xxx) WIFI_APP: Station: Conectando a Mondongo...
I (xxx) TIME_SYNC: SNTP inicializado, esperando sincronización...
I (xxx) WEB_SERVER: SPIFFS montado correctamente en /spiffs
I (xxx) WEB_SERVER: SPIFFS: 1024 KB total, XX KB usado
I (xxx) WEB_SERVER: Archivo encontrado: /spiffs/index.html (XXX bytes)
I (xxx) WEB_SERVER: Archivo encontrado: /spiffs/login.html (XXX bytes)
I (xxx) WEB_SERVER: Archivo encontrado: /spiffs/dashboard.html (XXX bytes)
I (xxx) WEB_SERVER: Archivo encontrado: /spiffs/terminal.html (XXX bytes)
I (xxx) WEB_SERVER: Archivo encontrado: /spiffs/slider.html (XXX bytes)
I (xxx) WEB_SERVER: Archivo encontrado: /spiffs/style.css (XXX bytes)
I (xxx) WEB_SERVER: Archivo encontrado: /spiffs/script.js (XXX bytes)
I (xxx) WEB_SERVER: Todos los archivos encontrados correctamente (7/7)
I (xxx) WEB_SERVER: Servidor Web iniciado correctamente en puerto 80
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
├── FLUJO_DEL_CODIGO.md    # Documentación del flujo del código
│
├── front/                 # Archivos web (se copian a SPIFFS)
│   ├── index.html        # Terminal principal (requiere login)
│   ├── login.html        # Página de login
│   ├── dashboard.html    # Dashboard de control
│   ├── terminal.html     # Terminal web retro
│   ├── slider.html       # Control con slider
│   ├── style.css         # Estilos retro estilo Matrix
│   └── script.js         # Lógica del terminal web
│
└── main/                  # Código fuente del ESP32
    ├── CMakeLists.txt
    ├── main.c            # Punto de entrada (app_main)
    ├── wifi_app.c/h      # Configuración WiFi AP+STA
    ├── web_server.c/h    # Servidor HTTP y lógica web
    ├── terminal_commands.c/h # Procesamiento de comandos
    ├── rgb_led.c/h       # Control LED RGB verde
    ├── fan_control.c/h  # Control ventilador PWM
    ├── ntc_sensor.c/h   # Sensor de temperatura NTC
    ├── pir_driver.c/h   # Driver sensor PIR
    ├── time_sync.c/h    # Sincronización de tiempo SNTP
    └── registros.c/h    # Gestión de registros de horarios
```

## Credenciales por Defecto

- **SSID WiFi AP**: `ESP32_Server`
- **Contraseña WiFi AP**: `12345678`
- **SSID WiFi STA**: `Mondongo` (configurable en wifi_app.h)
- **Contraseña WiFi STA**: `huevos12` (configurable en wifi_app.h)
- **IP del ESP32 (AP)**: `192.168.4.1`
- **Usuario Web**: `root`
- **Contraseña Web**: `matrix123`

> **Importante**: Se deben cambiar estas credenciales en producción.

## Configuración

### Cambiar Credenciales WiFi AP

Se debe editar el archivo `main/wifi_app.h`:
```c
#define ESP_WIFI_SSID      "Tu_SSID"
#define ESP_WIFI_PASS      "Tu_Contraseña"
#define ESP_WIFI_CHANNEL   1
```

### Cambiar Credenciales WiFi STA

Se debe editar el archivo `main/wifi_app.h`:
```c
#define WIFI_STA_SSID      "Tu_Red_WiFi"
#define WIFI_STA_PASS      "Tu_Contraseña"
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
storage,  data, spiffs,  ,        1M    ← Archivos web y registros aquí
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

### El ventilador no funciona
- Verificar que el sensor PIR detecte presencia (excepto modo MANUAL)
- Verificar que la hora esté sincronizada (para modo SCHEDULE)
- Verificar que haya registros guardados (para modo SCHEDULE)

### La hora no se sincroniza
- Verificar que el ESP32 esté conectado a Internet (modo Station)
- Verificar que el servidor NTP sea accesible
- Se puede establecer la hora manualmente mediante `POST /time/set`

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
- **Tarea Dedicada**: Tarea RTOS separada procesa comandos de forma asíncrona

### Sistema de Archivos
- **SPIFFS**: Sistema de archivos en flash, 1MB de capacidad
- **Montaje**: Se monta en `/spiffs` durante la inicialización
- **Verificación**: El sistema verifica que todos los archivos web estén presentes al iniciar
- **Registros**: Los registros de horarios se guardan en `/spiffs/registros.json`

### Servidor HTTP
- **Puerto**: 80 (HTTP estándar)
- **Conexiones**: Soporta hasta 7 conexiones simultáneas
- **Protocolo**: HTTP/1.1
- **Handlers**: Múltiples handlers para diferentes rutas (17 rutas registradas)

### FreeRTOS
- **Kernel**: El programa corre sobre FreeRTOS
- **Tareas**:
  - Tarea principal (app_main)
  - Tarea de procesamiento de comandos GPIO
  - Tarea de gestión de sesiones
  - Tarea de lectura de temperatura NTC
  - Tarea de control automático por temperatura del ventilador
  - Tarea de control por horarios del ventilador
- **Sincronización**: Mutex y colas para comunicación entre tareas

### Sensor de Temperatura
- **Tipo**: Sensor NTC 10k (termistor)
- **ADC**: Utiliza ADC1, canal 4 (GPIO32)
- **Lectura**: Tarea RTOS lee temperatura periódicamente cada 1 segundo
- **API**: Endpoint `/temperature` devuelve temperatura en formato JSON
- **Cálculo**: Ecuación de Steinhart-Hart para conversión resistencia-temperatura

### Control del Ventilador
- **PWM**: Control mediante LEDC, 8 bits, 25kHz
- **Modos**: OFF, MANUAL, AUTO_TEMP, SCHEDULE
- **Sensor PIR**: El ventilador solo funciona si detecta presencia (excepto modo MANUAL)
- **Control Automático**: Basado en temperatura (15-25°C)
- **Control por Horarios**: Basado en registros guardados en SPIFFS

### Sincronización de Tiempo
- **Protocolo**: SNTP (Simple Network Time Protocol)
- **Servidor**: co.pool.ntp.org (Colombia)
- **Zona Horaria**: Colombia (UTC-5)
- **Persistencia**: Hora guardada en NVS para restaurar después de reinicio
- **Fallback**: Establecimiento manual de hora si SNTP no está disponible

---
