// ===== INCLUDES Y CONFIGURACIÓN =====
#include "rgb_led.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"

static const char *TAG = "RGB_LED";

// ===== CONFIGURACIÓN DE HARDWARE =====
#define GPIO_GREEN      27
#define LEDC_TIMER      LEDC_TIMER_0
#define LEDC_MODE       LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL_G  LEDC_CHANNEL_1
#define LEDC_DUTY_RES   LEDC_TIMER_8_BIT
#define LEDC_FREQUENCY  5000

// ===== FUNCIONES DE INICIALIZACIÓN =====
void rgb_led_init(void)
{
    ESP_LOGI(TAG, "Inicializando PWM para LED verde...");
    
    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LEDC_DUTY_RES,
        .freq_hz = LEDC_FREQUENCY,
        .speed_mode = LEDC_MODE,
        .timer_num = LEDC_TIMER,
        .clk_cfg = LEDC_AUTO_CLK
    };
    esp_err_t err = ledc_timer_config(&ledc_timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error configurando timer PWM: %d", err);
        return;
    }

    ledc_channel_config_t ledc_channel = {
        .channel    = LEDC_CHANNEL_G,
        .duty       = 0,
        .gpio_num   = GPIO_GREEN,
        .speed_mode = LEDC_MODE,
        .hpoint     = 0,
        .timer_sel  = LEDC_TIMER
    };
    err = ledc_channel_config(&ledc_channel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error configurando canal PWM: %d", err);
        return;
    }

    ESP_LOGI(TAG, "LED verde inicializado en GPIO %d (8-bit, 5kHz)", GPIO_GREEN);
}

// ===== FUNCIONES DE CONTROL DEL LED =====
void rgb_set_green_percent(uint8_t percent)
{
    if (percent > 100) percent = 100;
    
    uint32_t duty = (percent * 255) / 100;
    
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_G, duty);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_G);
}
