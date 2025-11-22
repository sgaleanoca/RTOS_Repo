#include "driver/gpio.h"
#include "config.h"
#include "gpio_control.h"

void gpio_leds_init(void) {
    gpio_reset_pin(LED_AMARILLO_PIN);
    gpio_set_direction(LED_AMARILLO_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_AMARILLO_PIN, 0);
    
    gpio_reset_pin(LED_AZUL_PIN);
    gpio_set_direction(LED_AZUL_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_AZUL_PIN, 0);
}

void led_amarillo_on(void) {
    gpio_set_level(LED_AMARILLO_PIN, 1);
}

void led_amarillo_off(void) {
    gpio_set_level(LED_AMARILLO_PIN, 0);
}

void led_azul_on(void) {
    gpio_set_level(LED_AZUL_PIN, 1);
}

void led_azul_off(void) {
    gpio_set_level(LED_AZUL_PIN, 0);
}

void led_all_on(void) {
    gpio_set_level(LED_AMARILLO_PIN, 1);
    gpio_set_level(LED_AZUL_PIN, 1);
}

void led_all_off(void) {
    gpio_set_level(LED_AMARILLO_PIN, 0);
    gpio_set_level(LED_AZUL_PIN, 0);
}

int led_amarillo_get_state(void) {
    return gpio_get_level(LED_AMARILLO_PIN);
}

int led_azul_get_state(void) {
    return gpio_get_level(LED_AZUL_PIN);
}

