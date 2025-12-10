/**
 * @file web_server.h
 * @brief Servidor web HTTP del ESP32
 * @author Proyecto Final RTOS
 * @date 2024
 * 
 * @details Header file para el servidor web HTTP del ESP32. Este módulo gestiona:
 * - Montaje del sistema de archivos SPIFFS (donde se almacenan los archivos HTML/CSS/JS)
 * - Inicialización del servidor HTTP con múltiples rutas
 * - Manejo de autenticación de usuarios (login/logout)
 * - Procesamiento de comandos desde la terminal web mediante colas
 * - Servicio de datos de temperatura en tiempo real
 * - Gestión de sesiones con timeout automático
 * - Control del ventilador mediante API REST
 * - Gestión de registros de horarios del ventilador
 * - Control del sensor PIR
 * - Configuración de hora manual
 * 
 * @section routes Rutas disponibles
 * 
 * @subsection pages Páginas web
 * - / : Página de login o redirección al dashboard
 * - /dashboard : Panel principal con opciones
 * - /terminal : Terminal web retro para comandos
 * - /slider : Panel de control con temperatura y ventilador
 * 
 * @subsection auth Autenticación
 * - /login : Autenticación de usuarios (POST)
 * - /logout : Cerrar sesión (GET)
 * 
 * @subsection api API REST
 * - /cmd : Endpoint para ejecutar comandos de terminal (GET ?c=comando)
 * - /temperature : API JSON para obtener temperatura actual (GET)
 * - /fan/mode : Establecer modo del ventilador (POST)
 * - /fan/manual : Establecer velocidad manual (POST)
 * - /fan/status : Obtener estado del ventilador (GET)
 * - /fan/diagnostic : Diagnóstico completo del sistema (GET)
 * - /registros : Obtener todos los registros (GET) / Agregar nuevo registro (POST)
 * - /pir/status : Obtener estado del sensor PIR (GET)
 * - /time/set : Establecer hora manualmente (POST)
 * 
 * ============================================================================
 * ARCHIVO: web_server.h
 * ============================================================================
 * 
 * RESUMEN:
 * Header file para el servidor web HTTP del ESP32. Este módulo gestiona:
 * - Montaje del sistema de archivos SPIFFS (donde se almacenan los archivos HTML/CSS/JS)
 * - Inicialización del servidor HTTP con múltiples rutas
 * - Manejo de autenticación de usuarios (login/logout)
 * - Procesamiento de comandos desde la terminal web mediante colas
 * - Servicio de datos de temperatura en tiempo real
 * - Gestión de sesiones con timeout automático
 * - Control del ventilador mediante API REST
 * - Gestión de registros de horarios del ventilador
 * - Control del sensor PIR
 * - Configuración de hora manual
 * 
 * Rutas disponibles:
 * 
 * Páginas web:
 * - / : Página de login o redirección al dashboard
 * - /dashboard : Panel principal con opciones
 * - /terminal : Terminal web retro para comandos
 * - /slider : Panel de control con temperatura y ventilador
 * 
 * Autenticación:
 * - /login : Autenticación de usuarios (POST)
 * - /logout : Cerrar sesión (GET)
 * 
 * API de comandos:
 * - /cmd : Endpoint para ejecutar comandos de terminal (GET ?c=comando)
 * 
 * API de temperatura:
 * - /temperature : API JSON para obtener temperatura actual (GET)
 * 
 * API de ventilador:
 * - /fan/mode : Establecer modo del ventilador (POST)
 * - /fan/manual : Establecer velocidad manual (POST)
 * - /fan/status : Obtener estado del ventilador (GET)
 * - /fan/diagnostic : Diagnóstico completo del sistema (GET)
 * 
 * API de registros:
 * - /registros : Obtener todos los registros (GET)
 * - /registros : Agregar nuevo registro (POST)
 * 
 * API de sensor PIR:
 * - /pir/status : Obtener estado del sensor PIR (GET)
 * 
 * API de tiempo:
 * - /time/set : Establecer hora manualmente (POST)
 * 
 * ============================================================================
 * ÍNDICE DE SECCIONES:
 * ============================================================================
 * Sección 1: PROTOTIPOS DE FUNCIONES se encuentra en las líneas 60 a 70
 * ============================================================================
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// ===== PROTOTIPOS DE FUNCIONES =====
/**
 * Inicia el servidor web HTTP
 * - Monta el sistema de archivos SPIFFS
 * - Registra todas las rutas y handlers
 * - Crea tareas para procesamiento de comandos y gestión de sesiones
 */
void start_webserver(void);

// Si necesitaras detenerlo desde otro lado, podrías añadir:
// void stop_webserver(void);

#ifdef __cplusplus
}
#endif