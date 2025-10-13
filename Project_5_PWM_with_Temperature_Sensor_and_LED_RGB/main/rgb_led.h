#ifndef RGB_LED_H
#define RGB_LED_H

// Función para inicializar el controlador LEDC para los pines RGB
void rgb_led_init(void);

// Establece la intensidad del LED Rojo (0-100)
void set_red_intensity(float percentage);

// Establece la intensidad del LED Verde (0-100)
void set_green_intensity(float percentage);

#endif //RGB_LED_H