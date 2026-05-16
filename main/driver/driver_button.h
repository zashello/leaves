#pragma once

#include "esp_err.h"

typedef enum {
    BUTTON_EVENT_UP = 0,
    BUTTON_EVENT_DOWN,
    BUTTON_EVENT_CONFIRM,
    BUTTON_EVENT_BACK,
    BUTTON_EVENT_LONG_PRESS
} button_event_t;

typedef void (*button_callback_t)(button_event_t event);

esp_err_t buttonInitMulti(void);
esp_err_t buttonRegisterCallback(button_callback_t callback);
