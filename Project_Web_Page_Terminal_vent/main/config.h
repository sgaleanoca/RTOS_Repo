#ifndef CONFIG_H
#define CONFIG_H

// Credenciales para el Punto de Acceso (AP)
#define WIFI_SSID "ESP32_Server"
#define WIFI_PASSWORD "12345678"

// Pines para los LEDs
#define LED_AMARILLO_PIN GPIO_NUM_2
#define LED_AZUL_PIN GPIO_NUM_5

// Credenciales de login
#define VALID_USER "root"
#define VALID_PASS "matrix123"

// Tiempo de expiración de la sesión en milisegundos (3 minutos)
#define SESSION_TIMEOUT_MS (3UL * 60UL * 1000UL)

// Máximo de sesiones simultáneas
#define MAX_SESSIONS 10

#endif // CONFIG_H

