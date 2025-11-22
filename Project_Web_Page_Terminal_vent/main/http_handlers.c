#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_http_server.h"
#include "config.h"
#include "session.h"
#include "gpio_control.h"
#include "web_content.h"
#include "http_handlers.h"

static const char *TAG = "http_handlers";

// Decodificar URL (convierte %20 a espacio, %21 a !, etc.)
static void url_decode(char* str) {
    char *src = str;
    char *dst = str;
    
    while (*src) {
        if (*src == '%' && src[1] && src[2]) {
            // Convertir dos caracteres hexadecimales a un byte
            char hex[3] = {src[1], src[2], '\0'};
            char *endptr;
            long val = strtol(hex, &endptr, 16);
            if (*endptr == '\0' && val >= 0 && val <= 255) {
                *dst++ = (char)val;
                src += 3;
                continue;
            }
        } else if (*src == '+') {
            // + se convierte en espacio en URL encoding
            *dst++ = ' ';
            src++;
            continue;
        }
        *dst++ = *src++;
    }
    *dst = '\0';
}

esp_err_t handle_root_get(httpd_req_t *req) {
    char client_ip[16];
    get_client_ip(req, client_ip, sizeof(client_ip));
    
    if (!is_authenticated(client_ip)) {
        httpd_resp_set_type(req, "text/html");
        httpd_resp_send(req, get_login_html(), HTTPD_RESP_USE_STRLEN);
    } else {
        httpd_resp_set_type(req, "text/html");
        httpd_resp_send(req, get_terminal_html(), HTTPD_RESP_USE_STRLEN);
    }
    return ESP_OK;
}

esp_err_t handle_login_post(httpd_req_t *req) {
    char content[256];
    int ret = httpd_req_recv(req, content, sizeof(content) - 1);
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    content[ret] = '\0';
    
    // Parsear user y pass
    char user[64] = {0};
    char pass[64] = {0};
    
    char *user_start = strstr(content, "user=");
    char *pass_start = strstr(content, "pass=");
    
    if (user_start) {
        user_start += 5;
        char *user_end = strchr(user_start, '&');
        if (user_end) {
            int len = user_end - user_start;
            strncpy(user, user_start, len < sizeof(user) - 1 ? len : sizeof(user) - 1);
        } else {
            strncpy(user, user_start, sizeof(user) - 1);
        }
    }
    
    if (pass_start) {
        pass_start += 5;
        strncpy(pass, pass_start, sizeof(pass) - 1);
    }
    
    // URL decode básico
    // Simplificado: solo maneja espacios codificados como +
    for (int i = 0; user[i]; i++) {
        if (user[i] == '+') user[i] = ' ';
    }
    for (int i = 0; pass[i]; i++) {
        if (pass[i] == '+') pass[i] = ' ';
    }
    
    char client_ip[16];
    get_client_ip(req, client_ip, sizeof(client_ip));
    
    if (strcmp(user, VALID_USER) == 0 && strcmp(pass, VALID_PASS) == 0) {
        create_session(client_ip);
        httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
        ESP_LOGI(TAG, "Login exitoso desde IP: %s", client_ip);
    } else {
        httpd_resp_set_status(req, "401 Unauthorized");
        httpd_resp_send(req, "[ERROR] Usuario o contraseña incorrectos.", HTTPD_RESP_USE_STRLEN);
        ESP_LOGI(TAG, "Intento de login fallido.");
    }
    
    return ESP_OK;
}

esp_err_t handle_logout_get(httpd_req_t *req) {
    char client_ip[16];
    get_client_ip(req, client_ip, sizeof(client_ip));
    
    session_t* s = find_session(client_ip);
    if (s != NULL) {
        s->authenticated = false;
    }
    
    led_all_off();
    
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_send(req, NULL, 0);
    
    ESP_LOGI(TAG, "Logout desde IP: %s. LEDs apagados.", client_ip);
    return ESP_OK;
}

esp_err_t handle_cmd_get(httpd_req_t *req) {
    char client_ip[16];
    get_client_ip(req, client_ip, sizeof(client_ip));
    
    if (!is_authenticated(client_ip)) {
        httpd_resp_set_status(req, "401 Unauthorized");
        httpd_resp_send(req, "[ERROR] No estás autenticado o la sesión ha expirado.", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    char query[256];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK) {
        httpd_resp_send(req, "[?] No se recibió ningún comando.", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    char cmd[64] = {0};
    if (httpd_query_key_value(query, "c", cmd, sizeof(cmd)) != ESP_OK) {
        httpd_resp_send(req, "[?] No se recibió ningún comando.", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    // Decodificar URL (convierte %20 a espacio, etc.)
    url_decode(cmd);
    
    // Convertir a minúsculas
    for (int i = 0; cmd[i]; i++) {
        if (cmd[i] >= 'A' && cmd[i] <= 'Z') {
            cmd[i] = cmd[i] - 'A' + 'a';
        }
    }
    
    // Trim
    char *start = cmd;
    while (*start == ' ') start++;
    char *end = start + strlen(start) - 1;
    while (end > start && *end == ' ') end--;
    *(end + 1) = '\0';
    
    char response[512] = {0};
    snprintf(response, sizeof(response), "[?] Comando no reconocido: '%s'. Escribe 'help' para ver la lista.", cmd);
    
    if (strcmp(cmd, "led r on") == 0) {
        led_amarillo_on();
        snprintf(response, sizeof(response), "[OK] LED rojo encendido.");
    } else if (strcmp(cmd, "led r off") == 0) {
        led_amarillo_off();
        snprintf(response, sizeof(response), "[OK] LED rojo apagado.");
    } else if (strcmp(cmd, "led b on") == 0) {
        led_azul_on();
        snprintf(response, sizeof(response), "[OK] LED azul encendido.");
    } else if (strcmp(cmd, "led b off") == 0) {
        led_azul_off();
        snprintf(response, sizeof(response), "[OK] LED azul apagado.");
    } else if (strcmp(cmd, "led all on") == 0) {
        led_all_on();
        snprintf(response, sizeof(response), "[OK] Ambos LEDs encendidos.");
    } else if (strcmp(cmd, "led all off") == 0) {
        led_all_off();
        snprintf(response, sizeof(response), "[OK] Ambos LEDs apagados.");
    } else if (strcmp(cmd, "status") == 0) {
        int estado_rojo = led_amarillo_get_state();
        int estado_azul = led_azul_get_state();
        snprintf(response, sizeof(response), 
                 "Estado de los LEDs:\n  - Rojo: %s\n  - Azul:     %s",
                 estado_rojo ? "ON" : "OFF",
                 estado_azul ? "ON" : "OFF");
    } else if (strcmp(cmd, "help") == 0) {
        snprintf(response, sizeof(response),
                 "Comandos disponibles:\n\n"
                 "  --- Control Individual ---\n"
                 "  led r on          - Enciende el LED rojo.\n"
                 "  led r off         - Apaga el LED rojo.\n"
                 "  led b on          - Enciende el LED azul.\n"
                 "  led b off         - Apaga el LED azul.\n\n"
                 "  --- Control General ---\n"
                 "  led all on        - Enciende ambos LEDs.\n"
                 "  led all off       - Apaga ambos LEDs.\n\n"
                 "  --- Sistema ---\n"
                 "  status            - Muestra el estado de los LEDs.\n"
                 "  help              - Muestra esta lista.\n"
                 "  clear             - Limpia la pantalla.");
    }
    
    httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t handle_login_css_get(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/css");
    httpd_resp_send(req, get_login_css(), HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t handle_login_js_get(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/javascript");
    httpd_resp_send(req, get_login_js(), HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t handle_terminal_css_get(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/css");
    httpd_resp_send(req, get_terminal_css(), HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t handle_terminal_js_get(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/javascript");
    httpd_resp_send(req, get_terminal_js(), HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

