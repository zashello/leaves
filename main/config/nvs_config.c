#include <string.h>
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "nvs_config.h"

static const char *TAG = "NVS_CONFIG";
static const char *NVS_NAMESPACE = "leaf_cfg";
static const char *KEY_WIFI_SSID = "wifi_ssid";
static const char *KEY_WIFI_PASS = "wifi_pass";
static const char *KEY_SF_KEY = "sf_key";
static const char *KEY_SC_KEY = "sc_key";
static const char *KEY_VALID = "cfg_valid";

esp_err_t configInit(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS分区异常，擦除后重新初始化");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NVS初始化失败: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "NVS初始化成功");
    return ESP_OK;
}

esp_err_t configLoad(device_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "打开NVS命名空间失败: %s", esp_err_to_name(ret));
        memset(config, 0, sizeof(device_config_t));
        config->configValid = false;
        return ret;
    }

    size_t len = 0;
    bool valid = false;

    ret = nvs_get_u8(handle, KEY_VALID, (uint8_t *)&valid);
    if (ret != ESP_OK || !valid) {
        ESP_LOGW(TAG, "无有效配置");
        memset(config, 0, sizeof(device_config_t));
        config->configValid = false;
        nvs_close(handle);
        return ESP_ERR_NOT_FOUND;
    }

    len = sizeof(config->wifiSsid);
    nvs_get_str(handle, KEY_WIFI_SSID, config->wifiSsid, &len);

    len = sizeof(config->wifiPass);
    nvs_get_str(handle, KEY_WIFI_PASS, config->wifiPass, &len);

    len = sizeof(config->siliconflowKey);
    nvs_get_str(handle, KEY_SF_KEY, config->siliconflowKey, &len);

    len = sizeof(config->serverchanKey);
    nvs_get_str(handle, KEY_SC_KEY, config->serverchanKey, &len);

    config->configValid = true;
    nvs_close(handle);

    ESP_LOGI(TAG, "配置加载成功: SSID=%s", config->wifiSsid);
    return ESP_OK;
}

esp_err_t configSave(const device_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "打开NVS命名空间失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = nvs_set_str(handle, KEY_WIFI_SSID, config->wifiSsid);
    if (ret != ESP_OK) goto save_err;

    ret = nvs_set_str(handle, KEY_WIFI_PASS, config->wifiPass);
    if (ret != ESP_OK) goto save_err;

    ret = nvs_set_str(handle, KEY_SF_KEY, config->siliconflowKey);
    if (ret != ESP_OK) goto save_err;

    ret = nvs_set_str(handle, KEY_SC_KEY, config->serverchanKey);
    if (ret != ESP_OK) goto save_err;

    ret = nvs_set_u8(handle, KEY_VALID, 1);
    if (ret != ESP_OK) goto save_err;

    ret = nvs_commit(handle);
    if (ret != ESP_OK) goto save_err;

    nvs_close(handle);
    ESP_LOGI(TAG, "配置保存成功");
    return ESP_OK;

save_err:
    ESP_LOGE(TAG, "配置保存失败: %s", esp_err_to_name(ret));
    nvs_close(handle);
    return ret;
}

esp_err_t configClear(void)
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        return ret;
    }

    nvs_set_u8(handle, KEY_VALID, 0);
    nvs_commit(handle);
    nvs_close(handle);

    ESP_LOGI(TAG, "配置已清除");
    return ESP_OK;
}

bool configIsValid(void)
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (ret != ESP_OK) {
        return false;
    }

    uint8_t valid = 0;
    nvs_get_u8(handle, KEY_VALID, &valid);
    nvs_close(handle);

    return valid == 1;
}
