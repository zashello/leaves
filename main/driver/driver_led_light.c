#include "driver_led_light.h"
#include "esp_log.h"
#include "driver/ledc.h"

static const char *TAG = "LED_LIGHT";

static uint8_t sBrightnessPercent = 0;

esp_err_t ledLightInit(void)
{
    ledc_timer_config_t timerConfig = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_LED_TIMER,
        .duty_resolution = LEDC_LED_RESOLUTION,
        .freq_hz = LEDC_LED_FREQ,
        .clk_cfg = LEDC_AUTO_CLK
    };

    esp_err_t ret = ledc_timer_config(&timerConfig);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LEDC timer config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ledc_channel_config_t channelConfig = {
        .gpio_num = LED_LIGHT_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_LED_CHANNEL,
        .timer_sel = LEDC_LED_TIMER,
        .duty = 0,
        .hpoint = 0
    };

    ret = ledc_channel_config(&channelConfig);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LEDC channel config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    sBrightnessPercent = 0;
    ESP_LOGI(TAG, "LED light init ok, GPIO=%d", LED_LIGHT_GPIO);
    return ESP_OK;
}

esp_err_t ledLightSetBrightness(uint8_t percent)
{
    if (percent > 100) percent = 100;

    uint32_t duty = (uint32_t)percent * LEDC_LED_MAX_DUTY / 100;

    esp_err_t ret = ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_LED_CHANNEL, duty);
    if (ret != ESP_OK) return ret;

    ret = ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_LED_CHANNEL);
    if (ret != ESP_OK) return ret;

    sBrightnessPercent = percent;
    ESP_LOGI(TAG, "LED brightness set to %d%% (duty=%lu)", percent, (unsigned long)duty);
    return ESP_OK;
}

uint8_t ledLightGetBrightness(void)
{
    return sBrightnessPercent;
}

bool ledLightIsOn(void)
{
    return sBrightnessPercent > 0;
}
