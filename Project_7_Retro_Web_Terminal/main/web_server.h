/**
 * ============================================================================
 * ARCHIVO: web_server.h
 * ============================================================================
 * 
 * RESUMEN:
 * Header file para el servidor web HTTP del ESP32. Este módulo gestiona:
 * - Montaje del sistema de archivos SPIFFS (donde se almacenan los archivos HTML/CSS/JS)
 * - Inicialización del servidor HTTP con múltiples rutas
 * - Manejo de autenticación de usuarios (login/logout)
 * - Procesamiento de comandos desde la terminal web
 * - Servicio de datos de temperatura en tiempo real
 * - Gestión de sesiones con timeout automático
 * 
 * Rutas disponibles:
 * - / : Página de login o redirección al dashboard
 * - /login : Autenticación de usuarios
 * - /logout : Cerrar sesión
 * - /dashboard : Panel principal con opciones
 * - /terminal : Terminal web retro para comandos
 * - /slider : Panel de control con temperatura y ventilador
 * - /cmd : Endpoint para ejecutar comandos
 * - /temperature : API JSON para obtener temperatura actual
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