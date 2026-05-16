#include <driver/gpio.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/Queue.h>
#include <freertos/timers.h>
#include "driver_button.h"
#include "app_config.h"

static const char *TAG = "BUTTON";

static button_callback_t gCallback = NULL;
static QueueHandle_t gEventQueue = NULL;
static TimerHandle_t gLongPressTimer = NULL;
static volatile bool gDownPressed = false;

static struct {
    int gpio;
    button_event_t event;
    volatile TickType_t lastInterrupt;
} g_buttons[4] = {
    {BUTTON_UP_GPIO,      BUTTON_EVENT_UP,      0},
    {BUTTON_DOWN_GPIO,    BUTTON_EVENT_DOWN,     0},
    {BUTTON_CONFIRM_GPIO, BUTTON_EVENT_CONFIRM,  0},
    {BUTTON_BACK_GPIO,    BUTTON_EVENT_BACK,     0},
};

static void longPressCallback(TimerHandle_t xTimer)
{
    if (gpio_get_level(BUTTON_DOWN_GPIO) == 0 && gDownPressed) {
        ESP_LOGW(TAG, "LONG PRESS TRIGGERED");
        if (gCallback) {
            gCallback(BUTTON_EVENT_LONG_PRESS);
        }
    }
}

static void sendEvent(button_event_t event)
{
    if (gEventQueue != NULL) {
        xQueueSend(gEventQueue, &event, 0);
    }
}

static void IRAM_ATTR buttonIsrHandler(void *arg)
{
    int idx = (int)arg;
    if (idx < 0 || idx >= 4) return;

    TickType_t now = xTaskGetTickCountFromISR();
    if (now - g_buttons[idx].lastInterrupt < pdMS_TO_TICKS(BUTTON_DEBOUNCE_MS)) {
        return;
    }
    g_buttons[idx].lastInterrupt = now;

    int level = gpio_get_level(g_buttons[idx].gpio);
    if (level == 0) {
        if (idx == 1) {
            gDownPressed = true;
            BaseType_t higherTaskWoken = pdFALSE;
            xTimerStartFromISR(gLongPressTimer, &higherTaskWoken);
            if (higherTaskWoken == pdTRUE) portYIELD_FROM_ISR();
        }
        sendEvent(g_buttons[idx].event);
    } else {
        if (idx == 1 && gDownPressed) {
            gDownPressed = false;
            BaseType_t higherTaskWoken = pdFALSE;
            xTimerStopFromISR(gLongPressTimer, &higherTaskWoken);
            if (higherTaskWoken == pdTRUE) portYIELD_FROM_ISR();
        }
    }
}

static void buttonTask(void *param)
{
    button_event_t event;
    while (1) {
        if (xQueueReceive(gEventQueue, &event, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (gCallback) {
                gCallback(event);
            }
        }
    }
}

esp_err_t buttonInitMulti(void)
{
    gEventQueue = xQueueCreate(16, sizeof(button_event_t));
    if (gEventQueue == NULL) {
        ESP_LOGE(TAG, "EVENT QUEUE CREATE FAILED");
        return ESP_FAIL;
    }

    gLongPressTimer = xTimerCreate("btn_long", pdMS_TO_TICKS(BUTTON_LONG_PRESS_MS),
                                    pdFALSE, NULL, longPressCallback);
    if (gLongPressTimer == NULL) {
        ESP_LOGE(TAG, "LONG PRESS TIMER CREATE FAILED");
        return ESP_FAIL;
    }

    esp_err_t ret = gpio_install_isr_service(0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "ISR SERVICE INSTALL FAILED: %s", esp_err_to_name(ret));
        return ret;
    }

    for (int i = 0; i < 4; i++) {
        gpio_config_t ioConf = {
            .pin_bit_mask = (1ULL << g_buttons[i].gpio),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_ANYEDGE,
        };
        ret = gpio_config(&ioConf);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "GPIO%d CONFIG FAILED: %s", g_buttons[i].gpio, esp_err_to_name(ret));
            continue;
        }

        ret = gpio_isr_handler_add(g_buttons[i].gpio, buttonIsrHandler, (void *)(intptr_t)i);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "GPIO%d ISR ADD FAILED: %s", g_buttons[i].gpio, esp_err_to_name(ret));
            continue;
        }

        ESP_LOGI(TAG, "BUTTON GPIO%d INIT OK (%s)", g_buttons[i].gpio,
                 i == 0 ? "UP" : i == 1 ? "DOWN" : i == 2 ? "CONFIRM" : "BACK");
    }

    xTaskCreate(buttonTask, "btn_task", 8192, NULL, 5, NULL);

    ESP_LOGI(TAG, "BUTTON SYSTEM INIT OK");
    return ESP_OK;
}

esp_err_t buttonRegisterCallback(button_callback_t callback)
{
    if (callback == NULL) return ESP_ERR_INVALID_ARG;
    gCallback = callback;
    return ESP_OK;
}
