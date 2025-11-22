#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "lwip/inet.h"
#include "lwip/ip4_addr.h"
#include "esp_timer.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "config.h"
#include "session.h"

static const char *TAG = "session";

static session_t sessions[MAX_SESSIONS];
static int session_count = 0;

session_t* find_session(const char* ip) {
    for (int i = 0; i < session_count; i++) {
        if (strcmp(sessions[i].ip, ip) == 0) {
            return &sessions[i];
        }
    }
    return NULL;
}

session_t* create_session(const char* ip) {
    if (session_count >= MAX_SESSIONS) {
        // Eliminar la sesión más antigua
        for (int i = 0; i < session_count - 1; i++) {
            sessions[i] = sessions[i + 1];
        }
        session_count--;
    }
    
    session_t* s = &sessions[session_count++];
    strncpy(s->ip, ip, sizeof(s->ip) - 1);
    s->ip[sizeof(s->ip) - 1] = '\0';
    s->last_activity = esp_timer_get_time() / 1000; // Convertir a milisegundos
    s->authenticated = true;
    return s;
}

bool is_authenticated(const char* ip) {
    session_t* s = find_session(ip);
    if (s == NULL || !s->authenticated) {
        return false;
    }
    
    unsigned long now = esp_timer_get_time() / 1000; // Convertir a milisegundos
    if (now - s->last_activity > SESSION_TIMEOUT_MS) {
        s->authenticated = false;
        gpio_set_level(LED_AMARILLO_PIN, 0);
        gpio_set_level(LED_AZUL_PIN, 0);
        ESP_LOGI(TAG, "Sesión expirada para IP %s. LEDs apagados.", ip);
        return false;
    }
    
    s->last_activity = now;
    return true;
}

void get_client_ip(httpd_req_t *req, char* ip_str, size_t len) {
    int sockfd = httpd_req_to_sockfd(req);
    if (sockfd >= 0) {
        struct sockaddr_in addr;
        socklen_t addr_len = sizeof(addr);
        if (getpeername(sockfd, (struct sockaddr *)&addr, &addr_len) == 0) {
            ip4_addr_t ip4_addr;
            ip4_addr.addr = addr.sin_addr.s_addr;
            const char* ip = ip4addr_ntoa_r(&ip4_addr, ip_str, len);
            if (ip != NULL) {
                return;
            }
        }
    }
    strncpy(ip_str, "unknown", len - 1);
    ip_str[len - 1] = '\0';
}

