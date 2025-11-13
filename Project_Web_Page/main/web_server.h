#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "esp_http_server.h"

/**
 * @brief Inicia el servidor web HTTP
 * @return Handle del servidor o NULL en caso de error
 */
httpd_handle_t start_webserver(void);

/**
 * @brief Detiene el servidor web HTTP
 * @param server Handle del servidor
 */
void stop_webserver(httpd_handle_t server);

#endif // WEB_SERVER_H

