#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "config.h"
#include "gpio_control.h"
#include "wifi.h"
#include "web_server.h"

static const char *TAG = "main";

void app_main(void) {
    // Inicializar NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Configurar GPIO para LEDs
    gpio_leds_init();
    
    ESP_LOGI(TAG, "Punto de Acceso (AP) activo: %s", WIFI_SSID);
    ESP_LOGI(TAG, "Contraseña de Login: %s", VALID_PASS);
    
    // Inicializar WiFi
    wifi_init_softap();
    
    // Inicializar servidor web
    start_webserver();
    
    ESP_LOGI(TAG, "Servidor web iniciado.");
}
