/**
 * @file sensor.c
 * @brief AS7341 光谱传感器驱动
 */

#include <stdio.h>
#include <string.h>
#include <esp_log.h>
#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/i2c_master.h>
#include <cJSON.h>

#include "as7341.h"
#include "sensor.h"

static const char *TAG = "leaf_spectrum";

#define SENSOR_I2C_SCL    5
#define SENSOR_I2C_SDA    4
#define SENSOR_I2C_FREQ   100000

static i2c_master_bus_handle_t g_i2c_bus = NULL;
static as7341_handle_t g_as7341 = NULL;

esp_err_t sensor_init(void)
{
    if (g_as7341 != NULL) {
        ESP_LOGW(TAG, "传感器已初始化");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "初始化AS7341: SCL=GPIO%d, SDA=GPIO%d, 频率=%dHz",
             SENSOR_I2C_SCL, SENSOR_I2C_SDA, SENSOR_I2C_FREQ);

    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .scl_io_num = SENSOR_I2C_SCL,
        .sda_io_num = SENSOR_I2C_SDA,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
    };
    esp_err_t ret = i2c_new_master_bus(&bus_config, &g_i2c_bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C总线创建失败: %s (0x%x)", esp_err_to_name(ret), ret);
        return ret;
    }

    as7341_config_t as7341_config = {
        .i2c_address = I2C_AS7341_DEV_ADDR,
        .i2c_clock_speed = SENSOR_I2C_FREQ,
        .spectral_gain = AS7341_SPECTRAL_GAIN_32X,
        .atime = 29,
        .astep = 599,
    };
    ret = as7341_init(g_i2c_bus, &as7341_config, &g_as7341);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "AS7341初始化失败: %s", esp_err_to_name(ret));
        i2c_del_master_bus(g_i2c_bus);
        g_i2c_bus = NULL;
        return ret;
    }

    ret = as7341_enable_power(g_as7341);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "AS7341上电失败: %s", esp_err_to_name(ret));
        as7341_delete(g_as7341);
        g_as7341 = NULL;
        i2c_del_master_bus(g_i2c_bus);
        g_i2c_bus = NULL;
        return ret;
    }

    ret = as7341_enable_spectral_measurement(g_as7341);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "启用光谱测量失败: %s", esp_err_to_name(ret));
        as7341_delete(g_as7341);
        g_as7341 = NULL;
        i2c_del_master_bus(g_i2c_bus);
        g_i2c_bus = NULL;
        return ret;
    }

    ESP_LOGI(TAG, "AS7341传感器初始化成功");
    return ESP_OK;
}

esp_err_t sensor_read_data(as7341_channels_spectral_data_t *data)
{
    if (g_as7341 == NULL) {
        ESP_LOGE(TAG, "传感器未初始化");
        return ESP_FAIL;
    }

    as7341_astatus_register_t astatus;
    as7341_status2_register_t status2;
    esp_err_t sret = as7341_get_astatus_register(g_as7341, &astatus);
    if (sret == ESP_OK) {
        ESP_LOGI(TAG, "传感器状态: 饱和=%d, 增益=%d",
                 astatus.bits.asat_status, astatus.bits.again_status);
    }
    sret = as7341_get_status2_register(g_as7341, &status2);
    if (sret == ESP_OK) {
        ESP_LOGI(TAG, "STATUS2: 模拟饱和=%d, 数字饱和=%d, 数据有效=%d",
                 status2.bits.analog_saturation, status2.bits.digital_saturation,
                 status2.bits.spectral_valid);
    }

    bool ready = false;
    int retry = 0;
    const int max_retry = 20;
    while (!ready && retry < max_retry) {
        esp_err_t ret = as7341_get_data_status(g_as7341, &ready);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "数据状态查询失败: %s", esp_err_to_name(ret));
            return ret;
        }
        if (!ready) {
            vTaskDelay(pdMS_TO_TICKS(50));
            retry++;
        }
    }
    if (!ready) {
        ESP_LOGE(TAG, "传感器数据未就绪，超时");
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t ret = as7341_get_spectral_measurements(g_as7341, data);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "光谱数据读取失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "光谱数据: F1=%u F2=%u F3=%u F4=%u F5=%u F6=%u F7=%u F8=%u Clear=%u NIR=%u",
             data->f1, data->f2, data->f3, data->f4,
             data->f5, data->f6, data->f7, data->f8,
             data->clear, data->nir);
    return ESP_OK;
}

char* sensor_data_to_json(const as7341_channels_spectral_data_t *data)
{
    if (data == NULL) return NULL;

    float ndvi = 0.0f;
    if ((data->f8 + data->nir) != 0) {
        ndvi = (float)(data->nir - data->f8) / (float)(data->nir + data->f8);
    }

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) return NULL;

    cJSON_AddNumberToObject(root, "f1", data->f1);
    cJSON_AddNumberToObject(root, "f2", data->f2);
    cJSON_AddNumberToObject(root, "f3", data->f3);
    cJSON_AddNumberToObject(root, "f4", data->f4);
    cJSON_AddNumberToObject(root, "f5", data->f5);
    cJSON_AddNumberToObject(root, "f6", data->f6);
    cJSON_AddNumberToObject(root, "f7", data->f7);
    cJSON_AddNumberToObject(root, "f8", data->f8);
    cJSON_AddNumberToObject(root, "clear", data->clear);
    cJSON_AddNumberToObject(root, "nir", data->nir);
    cJSON_AddNumberToObject(root, "ndvi", ndvi);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_str;
}

void sensor_deinit(void)
{
    if (g_as7341 != NULL) {
        as7341_delete(g_as7341);
        g_as7341 = NULL;
    }
    if (g_i2c_bus != NULL) {
        i2c_del_master_bus(g_i2c_bus);
        g_i2c_bus = NULL;
    }
    ESP_LOGI(TAG, "传感器资源已释放");
}
