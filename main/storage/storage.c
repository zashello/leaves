#include <string.h>
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "storage.h"
#include "system/system_log.h"

static const char *TAG = "STORAGE";
static const char *NVS_NAMESPACE = "leaf_cfg";
static const char *NVS_LOG_NAMESPACE = "leaf_log";
static const char *KEY_WIFI_SSID = "wifi_ssid";
static const char *KEY_WIFI_PASS = "wifi_pass";
static const char *KEY_SF_KEY = "sf_key";
static const char *KEY_SC_KEY = "sc_key";
static const char *KEY_MQTT_SERVER = "mqtt_server";
static const char *KEY_MQTT_PORT = "mqtt_port";
static const char *KEY_MQTT_USER = "mqtt_user";
static const char *KEY_MQTT_PASS = "mqtt_pass";
static const char *KEY_DEVICE_NAME = "device_name";
static const char *KEY_VALID = "cfg_valid";
static const char *KEY_ENABLE_MQTT = "enable_mqtt";
static const char *KEY_ENABLE_AI = "enable_ai";
static const char *KEY_ENABLE_AUTO_NET = "enable_auto_net";
static const char *KEY_MQTT_REPORT_INTERVAL = "mqtt_interval";

esp_err_t storageInit(void)
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

esp_err_t storageLoad(device_config_t *config)
{
    if (config == NULL) return ESP_ERR_INVALID_ARG;

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

    len = sizeof(config->mqttServer);
    nvs_get_str(handle, KEY_MQTT_SERVER, config->mqttServer, &len);

    nvs_get_u16(handle, KEY_MQTT_PORT, &config->mqttPort);

    len = sizeof(config->mqttUsername);
    nvs_get_str(handle, KEY_MQTT_USER, config->mqttUsername, &len);

    len = sizeof(config->mqttPassword);
    nvs_get_str(handle, KEY_MQTT_PASS, config->mqttPassword, &len);

    len = sizeof(config->deviceName);
    nvs_get_str(handle, KEY_DEVICE_NAME, config->deviceName, &len);

    uint8_t enableMqtt = 0;
    nvs_get_u8(handle, KEY_ENABLE_MQTT, &enableMqtt);
    config->enableMqtt = (enableMqtt == 1);

    uint8_t enableAi = 0;
    nvs_get_u8(handle, KEY_ENABLE_AI, &enableAi);
    config->enableAiService = (enableAi == 1);

    uint8_t enableAutoNet = 0;
    nvs_get_u8(handle, KEY_ENABLE_AUTO_NET, &enableAutoNet);
    config->enableAutoNetwork = (enableAutoNet == 1);

    uint16_t reportInterval = 0;
    nvs_get_u16(handle, KEY_MQTT_REPORT_INTERVAL, &reportInterval);
    if (reportInterval == 0) {
        config->mqttReportInterval = 10;
    } else {
        config->mqttReportInterval = reportInterval;
    }

    config->configValid = true;
    nvs_close(handle);

    ESP_LOGI(TAG, "配置加载成功: SSID=%s", config->wifiSsid);
    return ESP_OK;
}

esp_err_t storageSave(const device_config_t *config)
{
    if (config == NULL) return ESP_ERR_INVALID_ARG;

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

    ret = nvs_set_str(handle, KEY_MQTT_SERVER, config->mqttServer);
    if (ret != ESP_OK) goto save_err;

    ret = nvs_set_u16(handle, KEY_MQTT_PORT, config->mqttPort);
    if (ret != ESP_OK) goto save_err;

    ret = nvs_set_str(handle, KEY_MQTT_USER, config->mqttUsername);
    if (ret != ESP_OK) goto save_err;

    ret = nvs_set_str(handle, KEY_MQTT_PASS, config->mqttPassword);
    if (ret != ESP_OK) goto save_err;

    ret = nvs_set_str(handle, KEY_DEVICE_NAME, config->deviceName);
    if (ret != ESP_OK) goto save_err;

    ret = nvs_set_u8(handle, KEY_ENABLE_MQTT, config->enableMqtt ? 1 : 0);
    if (ret != ESP_OK) goto save_err;

    ret = nvs_set_u8(handle, KEY_ENABLE_AI, config->enableAiService ? 1 : 0);
    if (ret != ESP_OK) goto save_err;

    ret = nvs_set_u8(handle, KEY_ENABLE_AUTO_NET, config->enableAutoNetwork ? 1 : 0);
    if (ret != ESP_OK) goto save_err;

    ret = nvs_set_u16(handle, KEY_MQTT_REPORT_INTERVAL, config->mqttReportInterval);
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

esp_err_t storageClear(void)
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) return ret;

    nvs_set_u8(handle, KEY_VALID, 0);
    nvs_commit(handle);
    nvs_close(handle);

    ESP_LOGI(TAG, "配置已清除");
    return ESP_OK;
}

bool storageIsValid(void)
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (ret != ESP_OK) return false;

    uint8_t valid = 0;
    nvs_get_u8(handle, KEY_VALID, &valid);
    nvs_close(handle);

    return valid == 1;
}

static const char *logKey(int index)
{
    static char key[16];
    snprintf(key, sizeof(key), "log_%d", index);
    return key;
}

esp_err_t storageSaveLogEntry(int index, const log_entry_t *entry)
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_LOG_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) return ret;

    if (index == -1) {
        int slot = -1;
        for (int i = 0; i < LOG_MAX_ENTRIES; i++) {
            log_entry_t temp;
            size_t len = sizeof(log_entry_t);
            if (nvs_get_blob(handle, logKey(i), &temp, &len) != ESP_OK || temp.timestamp == 0) {
                slot = i;
                break;
            }
        }
        if (slot == -1) {
            for (int i = 0; i < LOG_MAX_ENTRIES - 1; i++) {
                log_entry_t temp;
                size_t len = sizeof(log_entry_t);
                if (nvs_get_blob(handle, logKey(i + 1), &temp, &len) == ESP_OK) {
                    nvs_set_blob(handle, logKey(i), &temp, len);
                }
            }
            slot = LOG_MAX_ENTRIES - 1;
        }
        index = slot;
    }

    if (index < 0 || index >= LOG_MAX_ENTRIES) {
        nvs_close(handle);
        return ESP_ERR_INVALID_ARG;
    }

    ret = nvs_set_blob(handle, logKey(index), entry, sizeof(log_entry_t));
    nvs_commit(handle);
    nvs_close(handle);
    return ret;
}

esp_err_t storageLoadLogEntry(int index, log_entry_t *entry)
{
    if (index < 0 || index >= LOG_MAX_ENTRIES || entry == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_LOG_NAMESPACE, NVS_READONLY, &handle);
    if (ret != ESP_OK) return ret;

    size_t len = sizeof(log_entry_t);
    ret = nvs_get_blob(handle, logKey(index), entry, &len);
    nvs_close(handle);
    return ret;
}
