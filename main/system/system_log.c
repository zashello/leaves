#include <string.h>
#include <time.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "system_log.h"
#include "storage/storage.h"

static const char *TAG = "SYS_LOG";

void systemLogInit(void)
{
    ESP_LOGI(TAG, "SYSTEM LOG INITIALIZED");
}

static void toUpperCase(char *dst, const char *src, int maxLen)
{
    for (int i = 0; i < maxLen - 1 && src[i] != '\0'; i++) {
        if (src[i] >= 'a' && src[i] <= 'z') {
            dst[i] = src[i] - ('a' - 'A');
        } else if ((src[i] >= 'A' && src[i] <= 'Z') ||
                   (src[i] >= '0' && src[i] <= '9') ||
                   src[i] == ' ' || src[i] == ':' ||
                   src[i] == '=' || src[i] == '.' ||
                   src[i] == '-' || src[i] == '_' ||
                   src[i] == '/') {
            dst[i] = src[i];
        } else {
            dst[i] = ' ';
        }
    }
    dst[maxLen - 1] = '\0';
}

esp_err_t systemLogAdd(log_level_t level, const char *message)
{
    log_entry_t entry;
    memset(&entry, 0, sizeof(entry));

    time_t now;
    time(&now);
    entry.timestamp = (uint32_t)now;
    entry.level = level;

    toUpperCase(entry.message, message, LOG_ENTRY_SIZE);

    ESP_LOGI(TAG, "LOG: [%d] %s", level, entry.message);

    return storageSaveLogEntry(-1, &entry);
}

esp_err_t systemLogGetEntry(int index, log_entry_t *entry)
{
    if (index < 0 || index >= LOG_MAX_ENTRIES || entry == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return storageLoadLogEntry(index, entry);
}

int systemLogGetCount(void)
{
    int count = 0;
    for (int i = 0; i < LOG_MAX_ENTRIES; i++) {
        log_entry_t entry;
        if (storageLoadLogEntry(i, &entry) == ESP_OK && entry.timestamp > 0) {
            count++;
        } else {
            break;
        }
    }
    return count;
}

void systemLogClear(void)
{
    for (int i = 0; i < LOG_MAX_ENTRIES; i++) {
        log_entry_t empty;
        memset(&empty, 0, sizeof(empty));
        storageSaveLogEntry(i, &empty);
    }
    ESP_LOGI(TAG, "SYSTEM LOG CLEARED");
}
