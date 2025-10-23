#ifndef RGB_LED_H
#define RGB_LED_H

#include <stdint.h>
#include <stdbool.h>

// Pines del LED RGB
#define RGB_RED_PIN    13
#define RGB_GREEN_PIN  33
#define RGB_BLUE_PIN   25

// Configuración PWM
#define RGB_PWM_FREQ       5000    // 5 kHz
#define RGB_PWM_RESOLUTION 8       // 8 bits (0-255)

// Funciones públicas
void rgb_led_init(void);
void rgb_led_set_intensity(uint8_t intensity);  // 0-100%
void rgb_led_set_color(uint8_t red, uint8_t green, uint8_t blue);  // 0-255 cada color
void rgb_led_off(void);
uint8_t rgb_led_get_intensity(void);  // Obtener intensidad actual (0-100%)
uint8_t rgb_led_get_red(void);        // Obtener valor rojo actual (0-255)
uint8_t rgb_led_get_green(void);      // Obtener valor verde actual (0-255)
uint8_t rgb_led_get_blue(void);       // Obtener valor azul actual (0-255)
bool rgb_led_is_on(void);             // Verificar si el LED está encendido
const char* rgb_led_get_color_name(void);  // Obtener nombre del color actual

#endif // RGB_LED_H
