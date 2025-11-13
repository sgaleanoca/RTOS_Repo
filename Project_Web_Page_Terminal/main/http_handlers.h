#ifndef HTTP_HANDLERS_H
#define HTTP_HANDLERS_H

#include "esp_http_server.h"

// Handlers del servidor HTTP
esp_err_t handle_root_get(httpd_req_t *req);
esp_err_t handle_login_post(httpd_req_t *req);
esp_err_t handle_logout_get(httpd_req_t *req);
esp_err_t handle_cmd_get(httpd_req_t *req);
esp_err_t handle_login_css_get(httpd_req_t *req);
esp_err_t handle_login_js_get(httpd_req_t *req);
esp_err_t handle_terminal_css_get(httpd_req_t *req);
esp_err_t handle_terminal_js_get(httpd_req_t *req);

#endif // HTTP_HANDLERS_H

