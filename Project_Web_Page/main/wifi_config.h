#ifndef WIFI_CONFIG_H
#define WIFI_CONFIG_H

#include "esp_err.h"

// Configuración WiFi
#define WIFI_SSID "ESP32_Server"
#define WIFI_PASSWORD "12345678"

/**
 * @brief Inicializa WiFi en modo Access Point (AP)
 */
void wifi_init_softap(void);

#endif // WIFI_CONFIG_H

