#pragma once

#include <stdbool.h>

typedef enum {
    APP_STATE_INIT = 0,
    APP_STATE_PROVISION,
    APP_STATE_CONNECTING,
    APP_STATE_RUNNING,
    APP_STATE_ERROR
} app_state_t;

typedef void (*state_callback_t)(app_state_t from, app_state_t to);

void appStateInit(void);
void appStateSet(app_state_t state);
app_state_t appStateGet(void);
void appStateRegisterCallback(state_callback_t cb);
const char* appStateGetName(app_state_t state);
