#ifndef RGB_LED_H
#define RGB_LED_H

#include <stdint.h>
#include <stdbool.h>

// Pines del LED RGB
#define RGB_RED_PIN    13
#define RGB_GREEN_PIN  12
#define RGB_BLUE_PIN   25

// Configuración PWM
#define RGB_PWM_FREQ       5000    // 5 kHz
#define RGB_PWM_RESOLUTION 8       // 8 bits (0-255)

// Contexto opaco del LED RGB
typedef struct rgb_led_ctx rgb_led_ctx_t;

rgb_led_ctx_t* rgb_led_create(void);
void rgb_led_destroy(rgb_led_ctx_t* ctx);
void rgb_led_set_intensity(rgb_led_ctx_t* ctx, uint8_t intensity);  // 0-100%
void rgb_led_set_color(rgb_led_ctx_t* ctx, uint8_t red, uint8_t green, uint8_t blue);  // 0-255
void rgb_led_off(rgb_led_ctx_t* ctx);
uint8_t rgb_led_get_intensity(rgb_led_ctx_t* ctx);
uint8_t rgb_led_get_red(rgb_led_ctx_t* ctx);
uint8_t rgb_led_get_green(rgb_led_ctx_t* ctx);
uint8_t rgb_led_get_blue(rgb_led_ctx_t* ctx);
bool rgb_led_is_on(rgb_led_ctx_t* ctx);
const char* rgb_led_get_color_name(rgb_led_ctx_t* ctx);

#endif // RGB_LED_H
