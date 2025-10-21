#ifndef RGB_LED_H
#define RGB_LED_H

#include "driver/ledc.h"
#include "driver/adc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// RGB LED GPIO pins
#define LED_RED_GPIO    2
#define LED_GREEN_GPIO  4
#define LED_BLUE_GPIO   5

// Potentiometer ADC channel
#define POT_ADC_CHANNEL ADC1_CHANNEL_3  // GPIO39

// PWM Configuration
#define LEDC_TIMER              LEDC_TIMER_0
#define LEDC_MODE               LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL_RED         LEDC_CHANNEL_0
#define LEDC_CHANNEL_GREEN       LEDC_CHANNEL_1
#define LEDC_CHANNEL_BLUE        LEDC_CHANNEL_2
#define LEDC_DUTY_RES            LEDC_TIMER_8_BIT
#define LEDC_FREQUENCY           5000

// Temperature ranges for automatic color control
#define TEMP_COLD_MIN    0.0f
#define TEMP_COLD_MAX    15.0f
#define TEMP_NORMAL_MIN  15.0f
#define TEMP_NORMAL_MAX  30.0f
#define TEMP_WARM_MIN    30.0f
#define TEMP_WARM_MAX    50.0f
#define TEMP_HOT_MIN     50.0f
#define TEMP_HOT_MAX     100.0f

// Function prototypes
void rgb_led_init(void);
void rgb_led_set_color(uint8_t red, uint8_t green, uint8_t blue);
void rgb_led_set_brightness(uint8_t brightness);
void rgb_led_auto_control(float temperature, float pot_value);
void rgb_led_get_status(void);
void rgb_led_task(void *pvParameters);
float rgb_led_read_potentiometer(void);

// External variables
extern uint8_t current_red, current_green, current_blue;
extern uint8_t current_brightness;

#endif // RGB_LED_H
