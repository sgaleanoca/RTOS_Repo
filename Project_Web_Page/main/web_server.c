#include "web_server.h"
#include "gpio_config.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include <string.h>

static const char *TAG = "web_server";

// Servidor HTTP
static httpd_handle_t server = NULL;

// Plantilla HTML (con CSS embebido)
static const char* HTML_TEMPLATE = 
"<!DOCTYPE html>"
"<html>"
"<head>"
"<meta charset='UTF-8'>"
"<title>Control de LEDs</title>"
"<style>"
"body { background-color: #000; color: white; font-family: sans-serif; text-align: center; padding-top: 40px; }"
"button { padding: 15px 30px; font-size: 18px; margin: 10px; background-color: #333; color: white; border: none; border-radius: 10px; cursor: pointer; }"
"button:hover { background-color: #555; }"
"button:active { background-color: #777; }"
"</style>"
"</head>"
"<body>"
"<h1>Controlar LEDs desde ESP32</h1>"
"<p>LED Rojo: %s</p>"
"<form action='/toggleRojo' method='GET'><button type='submit'>Encender/Apagar Rojo</button></form>"
"<p>LED Azul: %s</p>"
"<form action='/toggleAzul' method='GET'><button type='submit'>Encender/Apagar Azul</button></form>"
"<p>Ambos LEDs: %s</p>"
"<form action='/toggleAmbos' method='GET'><button type='submit'>Encender/Apagar Ambos</button></form>"
"</body>"
"</html>";

// Handler para la página principal
static esp_err_t handle_root_get(httpd_req_t *req)
{
    char html[2048];
    const char* estadoRojoStr = get_estado_rojo() ? "ENCENDIDO 🔴" : "APAGADO ⚫";
    const char* estadoAzulStr = get_estado_azul() ? "ENCENDIDO 🔵" : "APAGADO ⚫";
    const char* estadoAmbosStr;
    
    // Si ambos están encendidos, mostrar ROSA
    if (get_estado_rojo() && get_estado_azul()) {
        estadoAmbosStr = "ENCENDIDO (ROSA)";
    } else {
        estadoAmbosStr = "APAGADO ⚫";
    }
    
    snprintf(html, sizeof(html), HTML_TEMPLATE, estadoRojoStr, estadoAzulStr, estadoAmbosStr);
    
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// Handler para CSS (opcional, ya está embebido en HTML)
static esp_err_t handle_css_get(httpd_req_t *req)
{
    const char* css = 
        "body { background-color: #000; color: white; font-family: sans-serif; text-align: center; padding-top: 40px; }"
        "button { padding: 15px 30px; font-size: 18px; margin: 10px; background-color: #333; color: white; border: none; border-radius: 10px; cursor: pointer; }"
        "button:hover { background-color: #555; }"
        "button:active { background-color: #777; }";
    
    httpd_resp_set_type(req, "text/css");
    httpd_resp_send(req, css, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// Handler para JavaScript (opcional)
static esp_err_t handle_js_get(httpd_req_t *req)
{
    const char* js = "// JavaScript para actualización dinámica (opcional)";
    
    httpd_resp_set_type(req, "application/javascript");
    httpd_resp_send(req, js, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// Handler para toggle LED rojo
static esp_err_t toggle_rojo_handler(httpd_req_t *req)
{
    toggle_led_rojo();
    return handle_root_get(req);
}

// Handler para toggle LED azul
static esp_err_t toggle_azul_handler(httpd_req_t *req)
{
    toggle_led_azul();
    return handle_root_get(req);
}

// Handler para toggle ambos LEDs
static esp_err_t toggle_ambos_handler(httpd_req_t *req)
{
    bool nuevoEstado = !(get_estado_rojo() && get_estado_azul());
    set_estado_ambos(nuevoEstado);
    return handle_root_get(req);
}

// Inicializar servidor HTTP
httpd_handle_t start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    ESP_LOGI(TAG, "Iniciando servidor en puerto: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        ESP_LOGI(TAG, "Servidor iniciado");

        // Registrar handler para página principal
        httpd_uri_t root = {
            .uri       = "/",
            .method    = HTTP_GET,
            .handler   = handle_root_get
        };
        httpd_register_uri_handler(server, &root);

        // Registrar handler para CSS
        httpd_uri_t css = {
            .uri       = "/styles.css",
            .method    = HTTP_GET,
            .handler   = handle_css_get
        };
        httpd_register_uri_handler(server, &css);

        // Registrar handler para JavaScript
        httpd_uri_t js = {
            .uri       = "/script.js",
            .method    = HTTP_GET,
            .handler   = handle_js_get
        };
        httpd_register_uri_handler(server, &js);

        // Registrar handler para toggle rojo
        httpd_uri_t toggle_rojo = {
            .uri       = "/toggleRojo",
            .method    = HTTP_GET,
            .handler   = toggle_rojo_handler
        };
        httpd_register_uri_handler(server, &toggle_rojo);

        // Registrar handler para toggle azul
        httpd_uri_t toggle_azul = {
            .uri       = "/toggleAzul",
            .method    = HTTP_GET,
            .handler   = toggle_azul_handler
        };
        httpd_register_uri_handler(server, &toggle_azul);

        // Registrar handler para toggle ambos
        httpd_uri_t toggle_ambos = {
            .uri       = "/toggleAmbos",
            .method    = HTTP_GET,
            .handler   = toggle_ambos_handler
        };
        httpd_register_uri_handler(server, &toggle_ambos);

        return server;
    }

    ESP_LOGI(TAG, "Error al iniciar servidor");
    return NULL;
}

// Detener servidor HTTP
void stop_webserver(httpd_handle_t server)
{
    if (server != NULL) {
        httpd_stop(server);
        ESP_LOGI(TAG, "Servidor detenido");
    }
}

