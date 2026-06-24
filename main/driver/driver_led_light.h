#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#define LED_LIGHT_GPIO           16
#define LEDC_LED_TIMER           LEDC_TIMER_0
#define LEDC_LED_CHANNEL         LEDC_CHANNEL_0
#define LEDC_LED_FREQ            4000
#define LEDC_LED_RESOLUTION      LEDC_TIMER_14_BIT
#define LEDC_LED_MAX_DUTY        8191

esp_err_t ledLightInit(void);
esp_err_t ledLightSetBrightness(uint8_t percent);
uint8_t ledLightGetBrightness(void);
bool ledLightIsOn(void);
