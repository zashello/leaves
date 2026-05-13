#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "driver_scd41.h"
#include "app_config.h"
#include "scd4x.h"
#include "i2cdev.h"

static const char *TAG = "SCD41";

static i2c_dev_t g_scd41_dev;
static bool g_initialized = false;

esp_err_t scd41Init(void)
{
    if (g_initialized) {
        ESP_LOGW(TAG, "SCD41已初始化");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "初始化SCD41: SCL=GPIO%d, SDA=GPIO%d, 频率=%dHz",
             SCD41_I2C_SCL, SCD41_I2C_SDA, SCD41_I2C_FREQ);

    esp_err_t ret = i2cdev_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C子系统初始化失败: %s", esp_err_to_name(ret));
        return ret;
    }

    g_scd41_dev.port = SCD41_I2C_PORT;
    g_scd41_dev.addr = SCD4X_I2C_ADDR;
    g_scd41_dev.cfg.sda_io_num = SCD41_I2C_SDA;
    g_scd41_dev.cfg.scl_io_num = SCD41_I2C_SCL;
    g_scd41_dev.cfg.sda_pullup_en = true;
    g_scd41_dev.cfg.scl_pullup_en = true;
    g_scd41_dev.cfg.master.clk_speed = SCD41_I2C_FREQ;
    g_scd41_dev.addr_bit_len = I2C_ADDR_BIT_LEN_7;

    ret = scd4x_init_desc(&g_scd41_dev, SCD41_I2C_PORT, SCD41_I2C_SDA, SCD41_I2C_SCL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SCD41设备描述符创建失败: %s", esp_err_to_name(ret));
        i2cdev_done();
        return ret;
    }

    ret = i2c_dev_check_present(&g_scd41_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SCD41设备未检测到: %s", esp_err_to_name(ret));
        scd4x_free_desc(&g_scd41_dev);
        i2cdev_done();
        return ret;
    }

    uint16_t serial0, serial1, serial2;
    ret = scd4x_get_serial_number(&g_scd41_dev, &serial0, &serial1, &serial2);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "SCD41序列号: 0x%04x 0x%04x 0x%04x", serial0, serial1, serial2);
    } else {
        ESP_LOGW(TAG, "读取序列号失败: %s", esp_err_to_name(ret));
    }

    g_initialized = true;
    ESP_LOGI(TAG, "SCD41初始化成功，使用单次测量模式");
    return ESP_OK;
}

esp_err_t scd41ReadData(scd41_data_t *data)
{
    if (!g_initialized) {
        ESP_LOGE(TAG, "SCD41未初始化");
        return ESP_ERR_INVALID_STATE;
    }
    if (data == NULL) {
        ESP_LOGE(TAG, "无效的数据指针");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "启动SCD41单次测量...");
    esp_err_t ret = scd4x_measure_single_shot(&g_scd41_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "启动单次测量失败: %s", esp_err_to_name(ret));
        data->data_valid = false;
        return ret;
    }

    ESP_LOGI(TAG, "等待测量完成...");
    vTaskDelay(pdMS_TO_TICKS(5000));

    uint16_t co2_ticks, temp_ticks, hum_ticks;
    ret = scd4x_read_measurement_ticks(&g_scd41_dev, &co2_ticks, &temp_ticks, &hum_ticks);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "读取测量数据失败: %s", esp_err_to_name(ret));
        data->data_valid = false;
        return ret;
    }

    data->co2_ppm = co2_ticks;
    data->temperature_c = -45.0f + 175.0f * ((float)temp_ticks) / 65536.0f;
    data->humidity_rh = 100.0f * ((float)hum_ticks) / 65536.0f;
    data->data_valid = true;

    ESP_LOGI(TAG, "SCD41数据: CO2=%uppm, 温度=%.2fC, 湿度=%.2f%%RH",
             data->co2_ppm, data->temperature_c, data->humidity_rh);
    return ESP_OK;
}

void scd41Deinit(void)
{
    if (!g_initialized) return;
    scd4x_free_desc(&g_scd41_dev);
    i2cdev_done();
    g_initialized = false;
    ESP_LOGI(TAG, "SCD41资源已释放");
}
