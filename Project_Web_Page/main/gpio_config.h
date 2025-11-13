#ifndef GPIO_CONFIG_H
#define GPIO_CONFIG_H

#include "driver/gpio.h"
#include <stdbool.h>

// Pines
#define LED_AMARILLO GPIO_NUM_2
#define LED_AZUL GPIO_NUM_5
#define BOTON_AMARILLO GPIO_NUM_12
#define BOTON_AZUL GPIO_NUM_13

/**
 * @brief Configura los pines GPIO (LEDs y botones)
 */
void configure_gpio(void);

/**
 * @brief Obtiene el estado del LED rojo
 * @return true si está encendido, false si está apagado
 */
bool get_estado_rojo(void);

/**
 * @brief Obtiene el estado del LED azul
 * @return true si está encendido, false si está apagado
 */
bool get_estado_azul(void);

/**
 * @brief Establece el estado del LED rojo
 * @param estado true para encender, false para apagar
 */
void set_estado_rojo(bool estado);

/**
 * @brief Establece el estado del LED azul
 * @param estado true para encender, false para apagar
 */
void set_estado_azul(bool estado);

/**
 * @brief Alterna el estado del LED rojo
 */
void toggle_led_rojo(void);

/**
 * @brief Alterna el estado del LED azul
 */
void toggle_led_azul(void);

/**
 * @brief Establece el mismo estado para ambos LEDs
 * @param estado true para encender, false para apagar
 */
void set_estado_ambos(bool estado);

/**
 * @brief Tarea para leer botones físicos
 * @param pvParameters Parámetros de la tarea (no usado)
 */
void button_task(void *pvParameters);

#endif // GPIO_CONFIG_H

