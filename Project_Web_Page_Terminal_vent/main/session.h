#ifndef SESSION_H
#define SESSION_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "esp_http_server.h"

// Estructura de sesión
typedef struct {
    char ip[16];
    unsigned long last_activity;
    bool authenticated;
} session_t;

// Funciones de sesión
session_t* find_session(const char* ip);
session_t* create_session(const char* ip);
bool is_authenticated(const char* ip);
void get_client_ip(httpd_req_t *req, char* ip_str, size_t len);

#endif // SESSION_H

