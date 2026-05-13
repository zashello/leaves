#include <driver/gpio.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/timers.h>
#include "driver_button.h"
#include "app_config.h"

static const char *TAG = "BUTTON";

static button_callback_t gCallback = NULL;
static TimerHandle_t gDebounceTimer = NULL;
static TimerHandle_t gLongPressTimer = NULL;
static volatile bool gButtonPressed = false;

static void longPressCallback(TimerHandle_t xTimer)
{
    if (gpio_get_level(BUTTON_GPIO) == 0) {
        ESP_LOGW(TAG, "长按3秒触发");
        if (gCallback) {
            gCallback(BUTTON_EVENT_LONG_PRESS);
        }
    }
}

static void debounceCallback(TimerHandle_t xTimer)
{
    int level = gpio_get_level(BUTTON_GPIO);
    if (level == 0 && !gButtonPressed) {
        gButtonPressed = true;
        ESP_LOGI(TAG, "按键按下，开始长按计时...");
        xTimerStart(gLongPressTimer, 0);
    } else if (level != 0 && gButtonPressed) {
        gButtonPressed = false;
        ESP_LOGI(TAG, "按键松开，取消长按");
        xTimerStop(gLongPressTimer, 0);
    }
}

static void IRAM_ATTR buttonIsrHandler(void *arg)
{
    BaseType_t highTaskWakeup = pdFALSE;
    xTimerStartFromISR(gDebounceTimer, &highTaskWakeup);
    if (highTaskWakeup == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

esp_err_t buttonInit(void)
{
    gpio_config_t ioConf = {
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    esp_err_t ret = gpio_config(&ioConf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "GPIO配置失败: %s", esp_err_to_name(ret));
        return ret;
    }

    gDebounceTimer = xTimerCreate("btn_dbnc", pdMS_TO_TICKS(BUTTON_DEBOUNCE_MS),
                                  pdFALSE, NULL, debounceCallback);
    gLongPressTimer = xTimerCreate("btn_long", pdMS_TO_TICKS(BUTTON_LONG_PRESS_MS),
                                   pdFALSE, NULL, longPressCallback);
    if (!gDebounceTimer || !gLongPressTimer) {
        ESP_LOGE(TAG, "定时器创建失败");
        return ESP_FAIL;
    }

    ret = gpio_install_isr_service(0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "ISR服务安装失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = gpio_isr_handler_add(BUTTON_GPIO, buttonIsrHandler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ISR注册失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "按键初始化成功 (GPIO%d, 长按%ds触发重置)", BUTTON_GPIO, BUTTON_LONG_PRESS_MS / 1000);
    return ESP_OK;
}

esp_err_t buttonRegisterCallback(button_callback_t callback)
{
    if (callback == NULL) return ESP_ERR_INVALID_ARG;
    gCallback = callback;
    return ESP_OK;
}
