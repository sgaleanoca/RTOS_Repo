#include "rgb_led.h"
#include "driver/ledc.h"
#include "esp_rom_gpio.h"
#include "esp_check.h"

#define LED_R_PIN 25
#define LED_G_PIN 26
#define LEDC_TIMER          LEDC_TIMER_0
#define LEDC_MODE          LEDC_LOW_SPEED_MODE
#define LEDC_DUTY_RES     LEDC_TIMER_10_BIT // 0-1023
#define LEDC_FREQUENCY    5000
#define LEDC_CHANNEL_R    LEDC_CHANNEL_0
#define LEDC_CHANNEL_G    LEDC_CHANNEL_1

void rgb_led_init(void) {
    // 1. Configurar el Timer del LEDC
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_MODE,
        .duty_resolution = LEDC_DUTY_RES,
        .timer_num = LEDC_TIMER,
        .freq_hz = LEDC_FREQUENCY
    };
    ledc_timer_config(&ledc_timer);

    // 2. Configurar el Canal para el LED Rojo
    ledc_channel_config_t ledc_channel_r = {
        .gpio_num = LED_R_PIN,
        .speed_mode = LEDC_MODE,
        .channel = LEDC_CHANNEL_R,
        .timer_sel = LEDC_TIMER,
        .duty = 0,
        .hpoint = 0
    };
    ledc_channel_config(&ledc_channel_r);

    // 3. Configurar el Canal para el LED Verde
    ledc_channel_config_t ledc_channel_g = {
        .gpio_num = LED_G_PIN,
        .speed_mode = LEDC_MODE,
        .channel = LEDC_CHANNEL_G,
        .timer_sel = LEDC_TIMER,
        .duty = 0,
        .hpoint = 0
    };
    ledc_channel_config(&ledc_channel_g);
}

// Función interna para mapear porcentaje (0-100) a duty cycle (0-1023)
static uint32_t map_percentage_to_duty(float percentage) {
    if (percentage < 0) percentage = 0;
    if (percentage > 100) percentage = 100;
    return (uint32_t)((percentage / 100.0) * 1023);
}

void set_red_intensity(float percentage) {
    uint32_t duty = map_percentage_to_duty(percentage);
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_R, duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_R));
}

void set_green_intensity(float percentage) {
    uint32_t duty = map_percentage_to_duty(percentage);
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_G, duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_G));
}