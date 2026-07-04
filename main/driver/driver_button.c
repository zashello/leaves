#include <driver/gpio.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/timers.h>
#include "driver_button.h"
#include "app_config.h"

static const char *TAG = "BUTTON";

static button_callback_t gCallback = NULL;
static TimerHandle_t gLongPressTimer = NULL;

static struct {
    int gpio;
    button_event_t event;
} g_buttons[4] = {
    {BUTTON_UP_GPIO,      BUTTON_EVENT_UP,       },
    {BUTTON_DOWN_GPIO,    BUTTON_EVENT_DOWN,     },
    {BUTTON_CONFIRM_GPIO, BUTTON_EVENT_CONFIRM,  },
    {BUTTON_BACK_GPIO,    BUTTON_EVENT_BACK,     },
};

static void longPressCallback(TimerHandle_t xTimer)
{
    if (gCallback) {
        gCallback(BUTTON_EVENT_LONG_PRESS);
    }
}

static void buttonTask(void *param)
{
    int lastLevel[4];
    int stableLevel[4];
    TickType_t stableSince[4];

    for (int i = 0; i < 4; i++) {
        lastLevel[i] = gpio_get_level(g_buttons[i].gpio);
        stableLevel[i] = lastLevel[i];
        stableSince[i] = xTaskGetTickCount();
    }

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10));

        TickType_t now = xTaskGetTickCount();

        for (int i = 0; i < 4; i++) {
            int level = gpio_get_level(g_buttons[i].gpio);

            if (level != lastLevel[i]) {
                lastLevel[i] = level;
                stableSince[i] = now;
                continue;
            }

            if (level != stableLevel[i] &&
                now - stableSince[i] >= pdMS_TO_TICKS(50)) {
                stableLevel[i] = level;

                if (level == 0) {
                    if (i == 1) {
                        xTimerStart(gLongPressTimer, 0);
                    }
                    if (gCallback) {
                        gCallback(g_buttons[i].event);
                    }
                } else {
                    if (i == 1) {
                        xTimerStop(gLongPressTimer, 0);
                    }
                }
            }
        }
    }
}

esp_err_t buttonInitMulti(void)
{
    gLongPressTimer = xTimerCreate("btn_long", pdMS_TO_TICKS(BUTTON_LONG_PRESS_MS),
                                    pdFALSE, NULL, longPressCallback);
    if (gLongPressTimer == NULL) {
        ESP_LOGE(TAG, "LONG PRESS TIMER CREATE FAILED");
        return ESP_FAIL;
    }

    for (int i = 0; i < 4; i++) {
        gpio_config_t ioConf = {
            .pin_bit_mask = (1ULL << g_buttons[i].gpio),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        esp_err_t ret = gpio_config(&ioConf);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "GPIO%d CONFIG FAILED: %s", g_buttons[i].gpio, esp_err_to_name(ret));
            continue;
        }

        ESP_LOGI(TAG, "BUTTON GPIO%d INIT OK (%s)", g_buttons[i].gpio,
                 i == 0 ? "UP" : i == 1 ? "DOWN" : i == 2 ? "CONFIRM" : "BACK");
    }

    xTaskCreate(buttonTask, "btn_task", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "BUTTON SYSTEM INIT OK (POLLING MODE)");
    return ESP_OK;
}

esp_err_t buttonRegisterCallback(button_callback_t callback)
{
    if (callback == NULL) return ESP_ERR_INVALID_ARG;
    gCallback = callback;
    return ESP_OK;
}
