#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

typedef struct {
    const char *name;
    TaskFunction_t function;
    uint32_t stackSize;
    UBaseType_t priority;
    void *param;
} task_config_t;

void taskManagerInit(void);
esp_err_t taskManagerCreate(const task_config_t *config);
esp_err_t taskManagerCreateAndRun(const task_config_t *config);
void taskManagerPrintStatus(void);
