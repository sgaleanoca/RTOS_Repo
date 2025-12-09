/**
 * ============================================================================
 * ARCHIVO: api_server.h
 * ============================================================================
 * 
 * RESUMEN:
 * Header file para el servidor API REST del ESP32. Este módulo proporciona
 * una API liviana que expone endpoints REST para controlar el hardware.
 * 
 * Endpoints REST disponibles:
 * - GET /api/temperature → Retorna la temperatura del sensor NTC
 * - GET /api/time → Retorna la hora actual del sistema
 * - GET /api/logs → Retorna los registros de horarios
 * - POST /api/terminal → Recibe un comando y retorna la respuesta
 * 
 * Arquitectura:
 * - Solo API REST, sin servir HTML/CSS/JS
 * - Respuestas en formato JSON
 * - Sin sistema de archivos SPIFFS para frontend
 * - Sin autenticación (se maneja en Flask/Raspberry Pi)
 * 
 * ============================================================================
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// ===== PROTOTIPOS DE FUNCIONES =====
/**
 * Inicia el servidor API REST
 * - Configura e inicia el servidor HTTP
 * - Registra todos los endpoints REST
 * - No monta SPIFFS ni sirve archivos estáticos
 * - Solo expone endpoints JSON para control de hardware
 */
void start_api_server(void);

#ifdef __cplusplus
}
#endif
