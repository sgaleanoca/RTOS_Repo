#ifndef GPIO_CONTROL_H
#define GPIO_CONTROL_H

// Inicializar GPIO para LEDs
void gpio_leds_init(void);

// Control de LEDs
void led_amarillo_on(void);
void led_amarillo_off(void);
void led_azul_on(void);
void led_azul_off(void);
void led_all_on(void);
void led_all_off(void);

// Obtener estado de LEDs
int led_amarillo_get_state(void);
int led_azul_get_state(void);

#endif // GPIO_CONTROL_H

