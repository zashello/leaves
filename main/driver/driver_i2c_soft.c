#include <string.h>
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "driver_i2c_soft.h"

static const char *TAG = "I2C_SOFT";

static int g_sclPin = -1;
static int g_sdaPin = -1;
static bool g_initialized = false;

#define SCL_H()  gpio_set_level(g_sclPin, 1)
#define SCL_L()  gpio_set_level(g_sclPin, 0)
#define SDA_H()  gpio_set_level(g_sdaPin, 1)
#define SDA_L()  gpio_set_level(g_sdaPin, 0)
#define SDA_OUT() gpio_set_direction(g_sdaPin, GPIO_MODE_OUTPUT)
#define SDA_IN()  gpio_set_direction(g_sdaPin, GPIO_MODE_INPUT)
#define SDA_READ() gpio_get_level(g_sdaPin)
#define I2C_DELAY() esp_rom_delay_us(5)

esp_err_t i2cSoftInit(int sclPin, int sdaPin)
{
    if (g_initialized) {
        ESP_LOGW(TAG, "软件I2C已初始化");
        return ESP_OK;
    }

    g_sclPin = sclPin;
    g_sdaPin = sdaPin;

    ESP_LOGI(TAG, "初始化软件I2C: SCL=GPIO%d, SDA=GPIO%d", sclPin, sdaPin);

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << sclPin) | (1ULL << sdaPin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "GPIO配置失败: %s", esp_err_to_name(ret));
        return ret;
    }

    SCL_H();
    SDA_H();

    g_initialized = true;
    ESP_LOGI(TAG, "软件I2C初始化成功");
    return ESP_OK;
}

void i2cSoftDeinit(void)
{
    if (!g_initialized) return;

    g_sclPin = -1;
    g_sdaPin = -1;
    g_initialized = false;
    ESP_LOGI(TAG, "软件I2C已释放");
}

void i2cSoftStart(void)
{
    if (!g_initialized) return;

    SDA_H();
    SCL_H();
    I2C_DELAY();

    SDA_L();
    I2C_DELAY();

    SCL_L();
    I2C_DELAY();
}

void i2cSoftStop(void)
{
    if (!g_initialized) return;

    SCL_L();
    SDA_L();
    I2C_DELAY();

    SCL_H();
    I2C_DELAY();

    SDA_H();
    I2C_DELAY();
}

void i2cSoftSendByte(uint8_t byte)
{
    if (!g_initialized) return;

    SDA_OUT();

    for (int i = 7; i >= 0; i--) {
        if (byte & (1 << i)) {
            SDA_H();
        } else {
            SDA_L();
        }
        I2C_DELAY();
        SCL_H();
        I2C_DELAY();
        SCL_L();
        I2C_DELAY();
    }

    SDA_H();
}

bool i2cSoftWaitAck(void)
{
    if (!g_initialized) return false;

    SCL_L();
    I2C_DELAY();
    SDA_IN();
    SDA_H();
    I2C_DELAY();

    SCL_H();
    I2C_DELAY();

    bool ack = SDA_READ();

    SCL_L();
    I2C_DELAY();

    SDA_OUT();
    SDA_H();

    return ack == 0;
}

void i2cSoftReadByte(uint8_t *byte, bool ack)
{
    if (!g_initialized || byte == NULL) return;

    *byte = 0;
    SDA_IN();
    SDA_H();

    for (int i = 7; i >= 0; i--) {
        SCL_L();
        I2C_DELAY();

        SCL_H();
        I2C_DELAY();

        if (SDA_READ()) {
            *byte |= (1 << i);
        }

        SCL_L();
        I2C_DELAY();
    }

    SDA_OUT();

    if (ack) {
        SDA_L();
    } else {
        SDA_H();
    }

    I2C_DELAY();

    SCL_H();
    I2C_DELAY();

    SCL_L();
    I2C_DELAY();

    SDA_H();
}
