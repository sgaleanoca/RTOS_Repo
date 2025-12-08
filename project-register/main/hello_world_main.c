/*
 * Ejemplo de Gestor de Registros NVS para ESP32
 * 
 * Este ejemplo demuestra el uso completo del gestor de registros NVS:
 * - Inicialización de NVS
 * - Crear un registro
 * - Leer un registro
 * - Actualizar un registro
 * - Leer el registro actualizado
 * - Borrar un registro
 * - Verificar que el registro ya no existe
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_manager.h"

static const char* TAG = "MAIN";

void app_main(void)
{
    int32_t valor_leido;
    bool existe;
    esp_err_t ret;
    
    printf("\n=== Gestor de Registros NVS - Ejemplo Completo ===\n\n");
    
    // ============================================================
    // PASO 1: Inicializar NVS
    // ============================================================
    printf("[1] Inicializando NVS...\n");
    ret = nvs_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error inicializando NVS: %s", esp_err_to_name(ret));
        return;
    }
    printf("    ✓ NVS inicializado correctamente\n\n");
    
    // ============================================================
    // PASO 2: Guardar un valor inicial en NVS
    // ============================================================
    printf("[2] Guardando valor inicial en NVS...\n");
    int32_t valor_inicial = 25; // Ejemplo: temperatura de 25°C
    ret = nvs_create_registro(REGISTRO_FAN_KEY, valor_inicial);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error guardando registro: %s", esp_err_to_name(ret));
        nvs_manager_deinit();
        return;
    }
    printf("    ✓ Valor guardado: %ld\n\n", (long)valor_inicial);
    
    // Esperar un poco para que se complete la escritura
    vTaskDelay(100 / portTICK_PERIOD_MS);
    
    // ============================================================
    // PASO 3: Leer el valor guardado y mostrarlo por serial
    // ============================================================
    printf("[3] Leyendo valor desde NVS...\n");
    ret = nvs_read_registro(REGISTRO_FAN_KEY, &valor_leido);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error leyendo registro: %s", esp_err_to_name(ret));
    } else {
        printf("    ✓ Valor leído: %ld\n\n", (long)valor_leido);
    }
    
    // ============================================================
    // PASO 4: Actualizar el valor
    // ============================================================
    printf("[4] Actualizando valor en NVS...\n");
    int32_t valor_actualizado = 30; // Nuevo valor
    ret = nvs_update_registro(REGISTRO_FAN_KEY, valor_actualizado);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error actualizando registro: %s", esp_err_to_name(ret));
    } else {
        printf("    ✓ Valor actualizado a: %ld\n\n", (long)valor_actualizado);
    }
    
    // Esperar un poco para que se complete la escritura
    vTaskDelay(100 / portTICK_PERIOD_MS);
    
    // ============================================================
    // PASO 5: Leer el valor actualizado
    // ============================================================
    printf("[5] Leyendo valor actualizado desde NVS...\n");
    ret = nvs_read_registro(REGISTRO_FAN_KEY, &valor_leido);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error leyendo registro: %s", esp_err_to_name(ret));
    } else {
        printf("    ✓ Valor leído después de actualizar: %ld\n\n", (long)valor_leido);
    }
    
    // ============================================================
    // PASO 6: Borrar el registro
    // ============================================================
    printf("[6] Borrando registro de NVS...\n");
    ret = nvs_delete_registro(REGISTRO_FAN_KEY);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error borrando registro: %s", esp_err_to_name(ret));
    } else {
        printf("    ✓ Registro borrado correctamente\n\n");
    }
    
    // Esperar un poco
    vTaskDelay(100 / portTICK_PERIOD_MS);
    
    // ============================================================
    // PASO 7: Verificar que el registro ya no existe
    // ============================================================
    printf("[7] Verificando que el registro ya no existe...\n");
    ret = nvs_registro_exists(REGISTRO_FAN_KEY, &existe);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error verificando existencia: %s", esp_err_to_name(ret));
    } else {
        if (existe) {
            printf("    ✗ El registro todavía existe (no debería)\n");
        } else {
            printf("    ✓ El registro ya no existe (correcto)\n");
        }
    }
    
    // Intentar leer el registro borrado (debe fallar)
    printf("\n[8] Intentando leer registro borrado...\n");
    ret = nvs_read_registro(REGISTRO_FAN_KEY, &valor_leido);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        printf("    ✓ Correcto: El registro no se encuentra (esperado)\n");
    } else if (ret == ESP_OK) {
        printf("    ✗ Error: El registro todavía existe\n");
    } else {
        ESP_LOGE(TAG, "Error inesperado: %s", esp_err_to_name(ret));
    }
    
    // ============================================================
    // Limpieza
    // ============================================================
    printf("\n[9] Cerrando NVS...\n");
    nvs_manager_deinit();
    printf("    ✓ NVS cerrado correctamente\n\n");
    
    printf("=== Ejemplo completado exitosamente ===\n");
    printf("El sistema se reiniciará en 5 segundos...\n\n");
    
    // Esperar antes de reiniciar
    for (int i = 5; i > 0; i--) {
        printf("Reiniciando en %d segundos...\n", i);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    
    printf("Reiniciando ahora.\n");
    fflush(stdout);
    esp_restart();
}
