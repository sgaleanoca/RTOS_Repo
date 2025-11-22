#include "esp_log.h"
#include "esp_http_server.h"
#include "http_handlers.h"
#include "web_server.h"

static const char *TAG = "web_server";
static httpd_handle_t server = NULL;

httpd_handle_t start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;
    
    ESP_LOGI(TAG, "Iniciando servidor en puerto: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        ESP_LOGI(TAG, "Servidor iniciado");
        
        // Registrar handlers
        httpd_uri_t root = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = handle_root_get
        };
        httpd_register_uri_handler(server, &root);
        
        httpd_uri_t login = {
            .uri = "/login",
            .method = HTTP_POST,
            .handler = handle_login_post
        };
        httpd_register_uri_handler(server, &login);
        
        httpd_uri_t logout = {
            .uri = "/logout",
            .method = HTTP_GET,
            .handler = handle_logout_get
        };
        httpd_register_uri_handler(server, &logout);
        
        httpd_uri_t cmd = {
            .uri = "/cmd",
            .method = HTTP_GET,
            .handler = handle_cmd_get
        };
        httpd_register_uri_handler(server, &cmd);
        
        // Handlers para archivos estáticos
        httpd_uri_t login_css = {
            .uri = "/login.css",
            .method = HTTP_GET,
            .handler = handle_login_css_get
        };
        httpd_register_uri_handler(server, &login_css);
        
        httpd_uri_t login_js = {
            .uri = "/login.js",
            .method = HTTP_GET,
            .handler = handle_login_js_get
        };
        httpd_register_uri_handler(server, &login_js);
        
        httpd_uri_t terminal_css = {
            .uri = "/terminal.css",
            .method = HTTP_GET,
            .handler = handle_terminal_css_get
        };
        httpd_register_uri_handler(server, &terminal_css);
        
        httpd_uri_t terminal_js = {
            .uri = "/terminal.js",
            .method = HTTP_GET,
            .handler = handle_terminal_js_get
        };
        httpd_register_uri_handler(server, &terminal_js);
        
        return server;
    }
    
    ESP_LOGE(TAG, "Error al iniciar servidor");
    return NULL;
}

