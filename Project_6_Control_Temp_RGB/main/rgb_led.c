#include "rgb_led.h"
#include "temp_sensor.h"
#include "esp_log.h"
#include "esp_adc_cal.h"
#include <math.h>

static const char *TAG = "RGB_LED";

// Current LED state
uint8_t current_red = 0;
uint8_t current_green = 0;
uint8_t current_blue = 0;
uint8_t current_brightness = 255;

static esp_adc_cal_characteristics_t *adc_chars;

void rgb_led_init(void) {
    // Configure LEDC timer
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_MODE,
        .timer_num = LEDC_TIMER,
        .duty_resolution = LEDC_DUTY_RES,
        .freq_hz = LEDC_FREQUENCY,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    // Configure LEDC channels
    ledc_channel_config_t ledc_channel_red = {
        .speed_mode = LEDC_MODE,
        .channel = LEDC_CHANNEL_RED,
        .timer_sel = LEDC_TIMER,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = LED_RED_GPIO,
        .duty = 0,
        .hpoint = 0
    };
    ledc_channel_config(&ledc_channel_red);

    ledc_channel_config_t ledc_channel_green = {
        .speed_mode = LEDC_MODE,
        .channel = LEDC_CHANNEL_GREEN,
        .timer_sel = LEDC_TIMER,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = LED_GREEN_GPIO,
        .duty = 0,
        .hpoint = 0
    };
    ledc_channel_config(&ledc_channel_green);

    ledc_channel_config_t ledc_channel_blue = {
        .speed_mode = LEDC_MODE,
        .channel = LEDC_CHANNEL_BLUE,
        .timer_sel = LEDC_TIMER,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = LED_BLUE_GPIO,
        .duty = 0,
        .hpoint = 0
    };
    ledc_channel_config(&ledc_channel_blue);

    // Configure potentiometer ADC
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(POT_ADC_CHANNEL, ADC_ATTEN_DB_11);
    
    // Characterize ADC for potentiometer
    adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, adc_chars);

    ESP_LOGI(TAG, "RGB LED initialized");
}

void rgb_led_set_color(uint8_t red, uint8_t green, uint8_t blue) {
    current_red = red;
    current_green = green;
    current_blue = blue;
    
    // Apply brightness
    uint8_t adjusted_red = (red * current_brightness) / 255;
    uint8_t adjusted_green = (green * current_brightness) / 255;
    uint8_t adjusted_blue = (blue * current_brightness) / 255;
    
    // Set PWM duty cycles
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_RED, adjusted_red);
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_GREEN, adjusted_green);
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_BLUE, adjusted_blue);
    
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_RED);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_GREEN);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_BLUE);
    
    ESP_LOGI(TAG, "RGB set to R=%d G=%d B=%d", red, green, blue);
}

void rgb_led_set_brightness(uint8_t brightness) {
    current_brightness = brightness;
    rgb_led_set_color(current_red, current_green, current_blue);
}

void rgb_led_auto_control(float temperature, float pot_value) {
    uint8_t red = 0, green = 0, blue = 0;
    
    // Temperature-based color selection
    if (temperature < TEMP_COLD_MAX) {
        // Cold: Blue
        blue = 255;
        red = (uint8_t)((temperature - TEMP_COLD_MIN) / (TEMP_COLD_MAX - TEMP_COLD_MIN) * 100);
    }
    else if (temperature < TEMP_NORMAL_MAX) {
        // Normal: Green to Yellow
        green = 255;
        red = (uint8_t)((temperature - TEMP_NORMAL_MIN) / (TEMP_NORMAL_MAX - TEMP_NORMAL_MIN) * 255);
    }
    else if (temperature < TEMP_WARM_MAX) {
        // Warm: Yellow to Orange
        red = 255;
        green = (uint8_t)(255 - (temperature - TEMP_WARM_MIN) / (TEMP_WARM_MAX - TEMP_WARM_MIN) * 100);
    }
    else {
        // Hot: Red
        red = 255;
    }
    
    // Apply potentiometer brightness control
    uint8_t brightness = (uint8_t)(pot_value * 255);
    rgb_led_set_brightness(brightness);
    rgb_led_set_color(red, green, blue);
}

void rgb_led_get_status(void) {
    ESP_LOGI(TAG, "RGB Status - R:%d G:%d B:%d Brightness:%d", 
             current_red, current_green, current_blue, current_brightness);
}

void rgb_led_task(void *pvParameters) {
    while (1) {
        float temperature = temp_sensor_read();
        float pot_value = rgb_led_read_potentiometer();
        
        rgb_led_auto_control(temperature, pot_value);
        
        vTaskDelay(pdMS_TO_TICKS(100)); // Update every 100ms
    }
}

float rgb_led_read_potentiometer(void) {
    uint32_t adc_reading = adc1_get_raw(POT_ADC_CHANNEL);
    return (float)adc_reading / 4095.0f; // Normalize to 0.0-1.0
}
