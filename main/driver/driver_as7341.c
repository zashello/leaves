#include <stdio.h>
#include <string.h>
#include <esp_log.h>
#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/i2c_master.h>
#include <cJSON.h>
#include <as7341.h>
#include "driver_as7341.h"
#include "app_config.h"

static const char *TAG = "AS7341";

static i2c_master_bus_handle_t g_i2c_bus = NULL;
static as7341_handle_t g_as7341 = NULL;

esp_err_t as7341Init(void)
{
    // 幂等性保护：检查是否已经初始化
    if (g_as7341 != NULL) {
        ESP_LOGW(TAG, "传感器已初始化，跳过重复初始化");
        return ESP_OK;
    }

    // 幂等性保护：检查I2C总线状态
    if (g_i2c_bus != NULL) {
        ESP_LOGW(TAG, "I2C总线已存在，进行完整清理后重新初始化");
        // 清理可能存在的残留状态
        g_i2c_bus = NULL;
    }

    ESP_LOGI(TAG, "初始化AS7341: SCL=GPIO%d, SDA=GPIO%d, 频率=%dHz",
             AS7341_I2C_SCL, AS7341_I2C_SDA, AS7341_I2C_FREQ);

    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .scl_io_num = AS7341_I2C_SCL,
        .sda_io_num = AS7341_I2C_SDA,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
    };

    // 尝试创建I2C总线
    esp_err_t ret = i2c_new_master_bus(&bus_config, &g_i2c_bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C总线创建失败: %s (0x%x)", esp_err_to_name(ret), ret);
        g_i2c_bus = NULL;  // 确保状态一致
        return ret;
    }

    as7341_config_t as7341_config = {
        .i2c_address = I2C_AS7341_DEV_ADDR,
        .i2c_clock_speed = AS7341_I2C_FREQ,
        .spectral_gain = AS7341_SPECTRAL_GAIN_32X,
        .atime = 29,
        .astep = 599,
    };

    // 初始化AS7341设备
    ret = as7341_init(g_i2c_bus, &as7341_config, &g_as7341);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "AS7341初始化失败: %s", esp_err_to_name(ret));
        i2c_del_master_bus(g_i2c_bus);
        g_i2c_bus = NULL;
        g_as7341 = NULL;  // 确保状态一致
        return ret;
    }

    // 启用传感器电源
    ret = as7341_enable_power(g_as7341);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "AS7341上电失败: %s", esp_err_to_name(ret));
        as7341_delete(g_as7341);
        g_as7341 = NULL;
        i2c_del_master_bus(g_i2c_bus);
        g_i2c_bus = NULL;
        return ret;
    }

    // 配置LED控制 - 这是最关键的一步
    // 配置CONFIG寄存器的LED控制使能位（关键配置）
    as7341_config_register_t config;
    ret = as7341_get_config_register(g_as7341, &config);
    if (ret == ESP_OK) {
        config.bits.led_ldr_control_enabled = true;  // 使能CONFIG寄存器的LED控制
        ret = as7341_set_config_register(g_as7341, config);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "CONFIG寄存器LED控制位设置失败: %s", esp_err_to_name(ret));
        } else {
            ESP_LOGI(TAG, "CONFIG寄存器LED控制位已使能");
        }
    } else {
        ESP_LOGE(TAG, "读取CONFIG寄存器失败: %s", esp_err_to_name(ret));
    }

    // 配置LED寄存器的驱动电流和使能状态（低电平激活模式）
    as7341_led_register_t led_reg = {
        .bits.led_drive_strength = AS7341_LED_DRIVE_STRENGTH_12MA,
        .bits.led_ldr_enabled = false  // 低电平启动LED（LED阳极接VCC，阴极接LDR）
    };
    ret = as7341_set_led_register(g_as7341, led_reg);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "LED初始化失败: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "LED控制已启用(低电平激活模式)，驱动电流12mA(开始)，稳定时间%u ms", AS7341_LED_ON_TIME_MS);
    }

    // 启用光谱测量
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

esp_err_t as7341ReadData(as7341_channels_spectral_data_t *data)
{
    if (g_as7341 == NULL) {
        ESP_LOGE(TAG, "传感器未初始化");
        return ESP_FAIL;
    }

    esp_err_t ret = as7341LedControl(true);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "LED开启失败，继续测量: %s", esp_err_to_name(ret));
    }

    // 验证LED是否真的开启了，并输出详细状态信息
    bool led_enabled;
    as7341_led_drive_strengths_t led_current;
    esp_err_t check_ret = as7341GetLedStatus(&led_enabled, &led_current);
    if (check_ret == ESP_OK) {
        ESP_LOGI(TAG, "LED状态验证: %s, 驱动电流: %dmA",
                 led_enabled ? "开启" : "关闭",
                 4 + (int)led_current * 2);
        if (!led_enabled) {
            ESP_LOGW(TAG, "⚠️  LED未实际开启，测量可能不准确");
        }
    } else {
        ESP_LOGW(TAG, "LED状态验证失败: %s", esp_err_to_name(check_ret));
    }

    // 输出关键寄存器状态用于调试
    as7341_config_register_t config_check;
    as7341_led_register_t led_check;
    if (as7341_get_config_register(g_as7341, &config_check) == ESP_OK) {
        ESP_LOGI(TAG, "CONFIG寄存器: 0x%02X, LED控制位: %d",
                 config_check.reg, config_check.bits.led_ldr_control_enabled);
    }
    if (as7341_get_led_register(g_as7341, &led_check) == ESP_OK) {
        ESP_LOGI(TAG, "LED寄存器: 0x%02X, LED启用位(低电平激活): %d(%s)",
                 led_check.reg, led_check.bits.led_ldr_enabled,
                 led_check.bits.led_ldr_enabled ? "关闭" : "开启");
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
        ret = as7341_get_data_status(g_as7341, &ready);
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

    ret = as7341_get_spectral_measurements(g_as7341, data);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "光谱数据读取失败: %s", esp_err_to_name(ret));
        as7341LedControl(false);
        return ret;
    }

    ESP_LOGI(TAG, "光谱数据: F1=%u F2=%u F3=%u F4=%u F5=%u F6=%u F7=%u F8=%u Clear=%u NIR=%u",
              data->f1, data->f2, data->f3, data->f4,
              data->f5, data->f6, data->f7, data->f8,
              data->clear, data->nir);

    as7341LedControl(false);
    return ESP_OK;
}

char* as7341DataToJson(const as7341_channels_spectral_data_t *data)
{
    if (data == NULL) return NULL;

    float ndvi = 0.0f;
    if ((data->f8 + data->f5) != 0) {
        ndvi = (float)(data->f8 - data->f5) / (float)(data->f8 + data->f5);
    }

    float sipi = 0.0f;
    if ((data->nir - data->f7) != 0) {
        sipi = (float)(data->nir - data->f2) / (float)(data->nir - data->f7);
    }

    float psri = 0.0f;
    if (data->f8 != 0) {
        psri = (float)(data->f7 - data->f5) / (float)data->f8;
    }

    float cri550 = 0.0f;
    if (data->f3 != 0 && data->f5 != 0) {
        cri550 = (1.0f / (float)data->f3) - (1.0f / (float)data->f5);
    }

    float cri700 = 0.0f;
    if (data->f3 != 0 && data->f8 != 0) {
        cri700 = (1.0f / (float)data->f3) - (1.0f / (float)data->f8);
    }

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) return NULL;

    cJSON_AddNumberToObject(root, "f1_415nm", data->f1);
    cJSON_AddNumberToObject(root, "f2_445nm", data->f2);
    cJSON_AddNumberToObject(root, "f3_480nm", data->f3);
    cJSON_AddNumberToObject(root, "f4_515nm", data->f4);
    cJSON_AddNumberToObject(root, "f5_555nm", data->f5);
    cJSON_AddNumberToObject(root, "f6_590nm", data->f6);
    cJSON_AddNumberToObject(root, "f7_620nm", data->f7);
    cJSON_AddNumberToObject(root, "f8_670nm", data->f8);
    cJSON_AddNumberToObject(root, "clear", data->clear);
    cJSON_AddNumberToObject(root, "nir_910nm", data->nir);
    cJSON_AddNumberToObject(root, "ndvi", ndvi);
    cJSON_AddNumberToObject(root, "sipi", sipi);
    cJSON_AddNumberToObject(root, "psri", psri);
    cJSON_AddNumberToObject(root, "cri550", cri550);
    cJSON_AddNumberToObject(root, "cri700", cri700);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_str;
}

esp_err_t as7341LedControl(bool enable)
{
    if (g_as7341 == NULL) {
        ESP_LOGE(TAG, "传感器未初始化");
        return ESP_FAIL;
    }

    esp_err_t ret;
    if (enable) {
        ret = as7341_enable_led(g_as7341);
        if (ret == ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(AS7341_LED_ON_TIME_MS));
            ESP_LOGI(TAG, "LED补光灯已开启(低电平激活)，稳定%u ms", AS7341_LED_ON_TIME_MS);
        } else {
            ESP_LOGE(TAG, "LED开启失败: %s", esp_err_to_name(ret));
        }
    } else {
        ret = as7341_disable_led(g_as7341);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "LED关闭失败: %s", esp_err_to_name(ret));
        } else {
            ESP_LOGI(TAG, "LED补光灯已关闭(低电平激活)");
        }
    }

    return ret;
}

esp_err_t as7341SetLedCurrent(as7341_led_drive_strengths_t current)
{
    if (g_as7341 == NULL) {
        ESP_LOGE(TAG, "传感器未初始化");
        return ESP_FAIL;
    }

    as7341_config_register_t config_reg;
    esp_err_t ret = as7341_get_config_register(g_as7341, &config_reg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "读取CONFIG寄存器失败: %s", esp_err_to_name(ret));
        return ret;
    }

    config_reg.bits.led_ldr_control_enabled = true;
    ret = as7341_set_config_register(g_as7341, config_reg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "设置CONFIG寄存器失败: %s", esp_err_to_name(ret));
        return ret;
    }

    as7341_led_register_t led_reg;
    ret = as7341_get_led_register(g_as7341, &led_reg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "读取LED寄存器失败: %s", esp_err_to_name(ret));
        return ret;
    }

    led_reg.bits.led_drive_strength = current;
    led_reg.bits.led_ldr_enabled = false;  // 低电平激活模式
    ret = as7341_set_led_register(g_as7341, led_reg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "设置LED驱动电流失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "LED驱动电流已设置为%d mA(低电平激活模式)", 4 + (int)current * 2);
    return ESP_OK;
}

esp_err_t as7341GetLedStatus(bool *enabled, as7341_led_drive_strengths_t *current)
{
    if (g_as7341 == NULL) {
        ESP_LOGE(TAG, "传感器未初始化");
        return ESP_FAIL;
    }

    if (enabled == NULL || current == NULL) {
        ESP_LOGE(TAG, "参数不能为NULL");
        return ESP_ERR_INVALID_ARG;
    }

    as7341_led_register_t led_reg;
    esp_err_t ret = as7341_get_led_register(g_as7341, &led_reg);
    if (ret == ESP_OK) {
        *enabled = !led_reg.bits.led_ldr_enabled;  // 反转逻辑：false=开启, true=关闭
        *current = led_reg.bits.led_drive_strength;
        ESP_LOGI(TAG, "LED状态(低电平激活): %s, 驱动电流: %dmA, LDR电平: %s",
                 *enabled ? "开启" : "关闭",
                 4 + (int)(*current) * 2,
                 led_reg.bits.led_ldr_enabled ? "HIGH(关闭)" : "LOW(开启)");
    } else {
        ESP_LOGE(TAG, "读取LED寄存器失败: %s", esp_err_to_name(ret));
    }

    return ret;
}



void as7341Deinit(void)
{
    // 安全的清理，支持多次调用
    if (g_as7341 != NULL) {
        ESP_LOGI(TAG, "释放AS7341设备资源");
        as7341_delete(g_as7341);
        g_as7341 = NULL;
    }

    if (g_i2c_bus != NULL) {
        ESP_LOGI(TAG, "释放I2C总线资源");
        i2c_del_master_bus(g_i2c_bus);
        g_i2c_bus = NULL;
    }

    ESP_LOGI(TAG, "传感器资源已完整释放");
}

esp_err_t as7341SetLedCurrentMA(int current_ma) {
    if (g_as7341 == NULL) {
        ESP_LOGE(TAG, "传感器未初始化");
        return ESP_FAIL;
    }

    int reg_value = (current_ma - 4) / 2;

    if (reg_value < 0 || reg_value > 127) {
        ESP_LOGE(TAG, "电流值%d超出范围(4-258mA)", current_ma);
        return ESP_FAIL;
    }

    as7341_led_register_t led;
    esp_err_t ret = as7341_get_led_register(g_as7341, &led);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "读取LED寄存器失败: %s", esp_err_to_name(ret));
        return ret;
    }

    led.bits.led_drive_strength = reg_value;
    led.bits.led_ldr_enabled = false;  // 低电平激活模式
    ret = as7341_set_led_register(g_as7341, led);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "设置LED驱动电流失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "LED驱动电流设置为: %dmA (寄存器值: %d, 低电平激活模式)", current_ma, reg_value);
    return ESP_OK;
}

bool as7341IsInitialized(void) {
    return (g_as7341 != NULL) && (g_i2c_bus != NULL);
}
