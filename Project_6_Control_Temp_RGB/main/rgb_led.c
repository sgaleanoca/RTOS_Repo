// ===== INCLUDES Y CONFIGURACIÓN =====
#include "rgb_led.h"
#include "esp_log.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include <stdlib.h>

static const char *TAG = "RGB_LED";

// ===== CONFIGURACIÓN PWM =====
#define RGB_LEDC_TIMER      LEDC_TIMER_0
#define RGB_LEDC_MODE       LEDC_LOW_SPEED_MODE
#define RGB_LEDC_CHANNEL_R  LEDC_CHANNEL_0
#define RGB_LEDC_CHANNEL_G  LEDC_CHANNEL_1
#define RGB_LEDC_CHANNEL_B  LEDC_CHANNEL_2

// Variables globales para control de intensidad
static uint8_t current_intensity = 0;  // 0-100%
static uint8_t current_red = 255;      // Color base (blanco)
static uint8_t current_green = 255;
static uint8_t current_blue = 255;

// ===== FUNCIONES PRIVADAS =====
// Actualiza los tres canales PWM según color e intensidad actuales
static void rgb_led_update_pwm(void)
{
    uint8_t values[3] = {(current_red * current_intensity) / 100, 
                        (current_green * current_intensity) / 100, 
                        (current_blue * current_intensity) / 100};
    ledc_channel_t channels[3] = {RGB_LEDC_CHANNEL_R, RGB_LEDC_CHANNEL_G, RGB_LEDC_CHANNEL_B};
    
    for (int i = 0; i < 3; i++) {
        ESP_ERROR_CHECK(ledc_set_duty(RGB_LEDC_MODE, channels[i], values[i]));
        ESP_ERROR_CHECK(ledc_update_duty(RGB_LEDC_MODE, channels[i]));
    }
}

// ===== FUNCIONES PÚBLICAS =====
// Inicializa timer y canales PWM y deja el LED apagado
void rgb_led_init(void)
{
    ESP_LOGI(TAG, "Inicializando LED RGB...");
    
    // Configurar timer
    ledc_timer_config_t ledc_timer = {.speed_mode = RGB_LEDC_MODE, .timer_num = RGB_LEDC_TIMER, 
                                     .duty_resolution = RGB_PWM_RESOLUTION, .freq_hz = RGB_PWM_FREQ, .clk_cfg = LEDC_AUTO_CLK};
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));
    
    // Configurar canales RGB
    ledc_channel_config_t channels[3] = {
        {.speed_mode = RGB_LEDC_MODE, .channel = RGB_LEDC_CHANNEL_R, .timer_sel = RGB_LEDC_TIMER, 
         .intr_type = LEDC_INTR_DISABLE, .gpio_num = RGB_RED_PIN, .duty = 0, .hpoint = 0},
        {.speed_mode = RGB_LEDC_MODE, .channel = RGB_LEDC_CHANNEL_G, .timer_sel = RGB_LEDC_TIMER, 
         .intr_type = LEDC_INTR_DISABLE, .gpio_num = RGB_GREEN_PIN, .duty = 0, .hpoint = 0},
        {.speed_mode = RGB_LEDC_MODE, .channel = RGB_LEDC_CHANNEL_B, .timer_sel = RGB_LEDC_TIMER, 
         .intr_type = LEDC_INTR_DISABLE, .gpio_num = RGB_BLUE_PIN, .duty = 0, .hpoint = 0}
    };
    
    for (int i = 0; i < 3; i++) {
        ESP_ERROR_CHECK(ledc_channel_config(&channels[i]));
    }
    
    rgb_led_off();
    ESP_LOGI(TAG, "LED RGB inicializado correctamente - R:GPIO%d, G:GPIO%d, B:GPIO%d, F:%dHz", 
             RGB_RED_PIN, RGB_GREEN_PIN, RGB_BLUE_PIN, RGB_PWM_FREQ);
}

// Ajusta la intensidad global (0-100%) y actualiza PWM
void rgb_led_set_intensity(uint8_t intensity)
{
    current_intensity = intensity > 100 ? 100 : intensity;
    rgb_led_update_pwm();
}

// Define el color base (0-255 por canal) y actualiza PWM
void rgb_led_set_color(uint8_t red, uint8_t green, uint8_t blue)
{
    current_red = red;
    current_green = green;
    current_blue = blue;
    rgb_led_update_pwm();
}

// Apaga el LED estableciendo intensidad a 0 y actualizando PWM
void rgb_led_off(void)
{
    current_intensity = 0;
    rgb_led_update_pwm();
    ESP_LOGI(TAG, "LED RGB apagado");
}

uint8_t rgb_led_get_intensity(void) { return current_intensity; }
uint8_t rgb_led_get_red(void) { return current_red; }
uint8_t rgb_led_get_green(void) { return current_green; }
uint8_t rgb_led_get_blue(void) { return current_blue; }
bool rgb_led_is_on(void) { return current_intensity > 0; }

// Devuelve un nombre aproximado del color actual según componentes RGB
const char* rgb_led_get_color_name(void)
{
    if (!rgb_led_is_on()) return "APAGADO";
    
    // Determinar color basado en los valores RGB
    if (current_red > 200 && current_green < 50 && current_blue < 50) return "ROJO";
    if (current_red < 50 && current_green > 200 && current_blue < 50) return "VERDE";
    if (current_red < 50 && current_green < 50 && current_blue > 200) return "AZUL";
    if (current_red > 200 && current_green > 200 && current_blue < 50) return "AMARILLO";
    if (current_red > 200 && current_green < 50 && current_blue > 200) return "MAGENTA";
    if (current_red < 50 && current_green > 200 && current_blue > 200) return "CIAN";
    if (current_red > 200 && current_green > 200 && current_blue > 200) return "BLANCO";
    return "MIXTO";
}
