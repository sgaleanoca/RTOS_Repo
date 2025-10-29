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

// Contexto del LED RGB
struct rgb_led_ctx {
    uint8_t current_intensity;  // 0-100%
    uint8_t current_red;
    uint8_t current_green;
    uint8_t current_blue;
};

// ===== FUNCIONES PRIVADAS =====
// Actualiza los tres canales PWM según color e intensidad actuales
static void rgb_led_update_pwm(struct rgb_led_ctx* ctx)
{
    uint8_t values[3] = {(ctx->current_red * ctx->current_intensity) / 100, 
                        (ctx->current_green * ctx->current_intensity) / 100, 
                        (ctx->current_blue * ctx->current_intensity) / 100};
    ledc_channel_t channels[3] = {RGB_LEDC_CHANNEL_R, RGB_LEDC_CHANNEL_G, RGB_LEDC_CHANNEL_B};
    
    for (int i = 0; i < 3; i++) {
        ESP_ERROR_CHECK(ledc_set_duty(RGB_LEDC_MODE, channels[i], values[i]));
        ESP_ERROR_CHECK(ledc_update_duty(RGB_LEDC_MODE, channels[i]));
    }
}

// ===== FUNCIONES PÚBLICAS =====
// Inicializa timer y canales PWM y deja el LED apagado
rgb_led_ctx_t* rgb_led_create(void)
{
    rgb_led_ctx_t* ctx = (rgb_led_ctx_t*)calloc(1, sizeof(rgb_led_ctx_t));
    if (!ctx) return NULL;
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
    
    ctx->current_intensity = 0;
    ctx->current_red = 255;
    ctx->current_green = 255;
    ctx->current_blue = 255;
    rgb_led_off(ctx);
    ESP_LOGI(TAG, "LED RGB inicializado correctamente - R:GPIO%d, G:GPIO%d, B:GPIO%d, F:%dHz", 
             RGB_RED_PIN, RGB_GREEN_PIN, RGB_BLUE_PIN, RGB_PWM_FREQ);
    return ctx;
}

void rgb_led_destroy(rgb_led_ctx_t* ctx)
{
    if (!ctx) return;
    free(ctx);
}

// Ajusta la intensidad global (0-100%) y actualiza PWM
void rgb_led_set_intensity(rgb_led_ctx_t* ctx, uint8_t intensity)
{
    if (!ctx) return;
    ctx->current_intensity = intensity > 100 ? 100 : intensity;
    rgb_led_update_pwm(ctx);
}

// Define el color base (0-255 por canal) y actualiza PWM
void rgb_led_set_color(rgb_led_ctx_t* ctx, uint8_t red, uint8_t green, uint8_t blue)
{
    if (!ctx) return;
    ctx->current_red = red;
    ctx->current_green = green;
    ctx->current_blue = blue;
    rgb_led_update_pwm(ctx);
}

// Apaga el LED estableciendo intensidad a 0 y actualizando PWM
void rgb_led_off(rgb_led_ctx_t* ctx)
{
    if (!ctx) return;
    ctx->current_intensity = 0;
    rgb_led_update_pwm(ctx);
    ESP_LOGI(TAG, "LED RGB apagado");
}

uint8_t rgb_led_get_intensity(rgb_led_ctx_t* ctx) { return ctx ? ctx->current_intensity : 0; }
uint8_t rgb_led_get_red(rgb_led_ctx_t* ctx) { return ctx ? ctx->current_red : 0; }
uint8_t rgb_led_get_green(rgb_led_ctx_t* ctx) { return ctx ? ctx->current_green : 0; }
uint8_t rgb_led_get_blue(rgb_led_ctx_t* ctx) { return ctx ? ctx->current_blue : 0; }
bool rgb_led_is_on(rgb_led_ctx_t* ctx) { return ctx && ctx->current_intensity > 0; }

// Devuelve un nombre aproximado del color actual según componentes RGB
const char* rgb_led_get_color_name(rgb_led_ctx_t* ctx)
{
    if (!rgb_led_is_on(ctx)) return "APAGADO";
    
    // Determinar color basado en los valores RGB
    if (ctx->current_red > 200 && ctx->current_green < 50 && ctx->current_blue < 50) return "ROJO";
    if (ctx->current_red < 50 && ctx->current_green > 200 && ctx->current_blue < 50) return "VERDE";
    if (ctx->current_red < 50 && ctx->current_green < 50 && ctx->current_blue > 200) return "AZUL";
    if (ctx->current_red > 200 && ctx->current_green > 200 && ctx->current_blue < 50) return "AMARILLO";
    if (ctx->current_red > 200 && ctx->current_green < 50 && ctx->current_blue > 200) return "MAGENTA";
    if (ctx->current_red < 50 && ctx->current_green > 200 && ctx->current_blue > 200) return "CIAN";
    if (ctx->current_red > 200 && ctx->current_green > 200 && ctx->current_blue > 200) return "BLANCO";
    return "MIXTO";
}
