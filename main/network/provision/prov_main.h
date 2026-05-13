#pragma once

#include "esp_err.h"

typedef enum {
    PROV_STATE_IDLE,
    PROV_STATE_PROVISIONING,
    PROV_STATE_DONE,
    PROV_STATE_FAILED
} prov_state_t;

esp_err_t provStart(void);
void provStop(void);
prov_state_t provGetState(void);
