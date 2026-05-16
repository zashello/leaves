#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "app_config.h"

typedef enum {
    LOG_LEVEL_INFO = 0,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_CRITICAL
} log_level_t;

typedef struct {
    uint32_t timestamp;
    log_level_t level;
    char message[LOG_ENTRY_SIZE];
} log_entry_t;

void systemLogInit(void);
esp_err_t systemLogAdd(log_level_t level, const char *message);
esp_err_t systemLogGetEntry(int index, log_entry_t *entry);
int systemLogGetCount(void);
void systemLogClear(void);
