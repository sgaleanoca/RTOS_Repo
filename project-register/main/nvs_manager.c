/*
 * Gestor de Registros NVS - Implementación
 * 
 * Implementación de las funciones para gestionar registros persistentes
 * en la memoria flash del ESP32 usando NVS.
 */

#include "nvs_manager.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

static const char* TAG = "NVS_MANAGER";
static nvs_handle_t nvs_handle = 0;
static bool nvs_initialized = false;

esp_err_t nvs_manager_init(void)
{
    esp_err_t ret;
    
    // Inicializa el subsistema NVS
    // Si es la primera vez, formatea la partición NVS
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // Si la partición NVS está llena o hay una nueva versión, la formateamos
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Abre el namespace de NVS
    ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) abriendo namespace NVS!", esp_err_to_name(ret));
        return ret;
    }
    
    nvs_initialized = true;
    ESP_LOGI(TAG, "NVS inicializado correctamente");
    return ESP_OK;
}

esp_err_t nvs_manager_deinit(void)
{
    if (nvs_initialized && nvs_handle != 0) {
        nvs_close(nvs_handle);
        nvs_handle = 0;
        nvs_initialized = false;
        ESP_LOGI(TAG, "NVS cerrado correctamente");
    }
    return ESP_OK;
}

esp_err_t nvs_create_registro(const char* key, int32_t value)
{
    if (!nvs_initialized) {
        ESP_LOGE(TAG, "NVS no inicializado. Llama a nvs_manager_init() primero.");
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t ret = nvs_set_i32(nvs_handle, key, value);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) guardando clave '%s'", esp_err_to_name(ret), key);
        return ret;
    }
    
    // Commit para asegurar que los datos se escriban en flash
    ret = nvs_commit(nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) haciendo commit de la clave '%s'", esp_err_to_name(ret), key);
        return ret;
    }
    
    ESP_LOGI(TAG, "Registro '%s' creado/actualizado con valor: %ld", key, (long)value);
    return ESP_OK;
}

esp_err_t nvs_read_registro(const char* key, int32_t* value)
{
    if (!nvs_initialized) {
        ESP_LOGE(TAG, "NVS no inicializado. Llama a nvs_manager_init() primero.");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (value == NULL) {
        ESP_LOGE(TAG, "Puntero de valor es NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t ret = nvs_get_i32(nvs_handle, key, value);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Clave '%s' no encontrada en NVS", key);
        return ret;
    } else if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) leyendo clave '%s'", esp_err_to_name(ret), key);
        return ret;
    }
    
    ESP_LOGI(TAG, "Registro '%s' leído con valor: %ld", key, (long)*value);
    return ESP_OK;
}

esp_err_t nvs_update_registro(const char* key, int32_t value)
{
    // Actualizar es lo mismo que crear (sobrescribe si existe)
    return nvs_create_registro(key, value);
}

esp_err_t nvs_delete_registro(const char* key)
{
    if (!nvs_initialized) {
        ESP_LOGE(TAG, "NVS no inicializado. Llama a nvs_manager_init() primero.");
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t ret = nvs_erase_key(nvs_handle, key);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        // Si la clave no existe, consideramos la operación exitosa (idempotente)
        ESP_LOGW(TAG, "Clave '%s' no existe, no hay nada que borrar", key);
        return ESP_OK;
    } else if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) borrando clave '%s'", esp_err_to_name(ret), key);
        return ret;
    }
    
    // Commit para asegurar que los cambios se escriban en flash
    ret = nvs_commit(nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) haciendo commit después de borrar '%s'", esp_err_to_name(ret), key);
        return ret;
    }
    
    ESP_LOGI(TAG, "Registro '%s' borrado correctamente", key);
    return ESP_OK;
}

esp_err_t nvs_registro_exists(const char* key, bool* exists)
{
    if (!nvs_initialized) {
        ESP_LOGE(TAG, "NVS no inicializado. Llama a nvs_manager_init() primero.");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (exists == NULL) {
        ESP_LOGE(TAG, "Puntero 'exists' es NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    int32_t dummy_value;
    esp_err_t ret = nvs_get_i32(nvs_handle, key, &dummy_value);
    
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        *exists = false;
        return ESP_OK;
    } else if (ret == ESP_OK) {
        *exists = true;
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Error (%s) verificando existencia de clave '%s'", esp_err_to_name(ret), key);
        return ret;
    }
}

