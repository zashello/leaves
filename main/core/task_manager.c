#include "task_manager.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "TASK_MGR";

#define MAX_TASKS 16

typedef struct {
    char name[16];
    TaskHandle_t handle;
    bool active;
} task_entry_t;

static task_entry_t g_tasks[MAX_TASKS];
static int g_task_count = 0;

void taskManagerInit(void)
{
    memset(g_tasks, 0, sizeof(g_tasks));
    g_task_count = 0;
    ESP_LOGI(TAG, "任务管理器初始化完成");
}

esp_err_t taskManagerCreate(const task_config_t *config)
{
    if (config == NULL || g_task_count >= MAX_TASKS) {
        ESP_LOGE(TAG, "任务注册失败: 参数无效或已满");
        return ESP_FAIL;
    }

    task_entry_t *entry = &g_tasks[g_task_count];
    strncpy(entry->name, config->name, sizeof(entry->name) - 1);
    entry->active = false;
    entry->handle = NULL;

    BaseType_t ret = xTaskCreate(
        config->function,
        config->name,
        config->stackSize,
        config->param,
        config->priority,
        &entry->handle
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "任务创建失败: %s", config->name);
        entry->handle = NULL;
        return ESP_FAIL;
    }

    entry->active = true;
    g_task_count++;
    ESP_LOGI(TAG, "任务已创建: %s (优先级=%d, 栈=%u)", config->name, config->priority, config->stackSize);
    return ESP_OK;
}

esp_err_t taskManagerCreateAndRun(const task_config_t *config)
{
    return taskManagerCreate(config);
}

void taskManagerPrintStatus(void)
{
    ESP_LOGI(TAG, "===== 任务状态 =====");
    for (int i = 0; i < g_task_count; i++) {
        ESP_LOGI(TAG, "  [%d] %s - %s", i, g_tasks[i].name, g_tasks[i].active ? "运行中" : "已停止");
    }
    ESP_LOGI(TAG, "空闲内存: %u bytes", (unsigned int)xPortGetFreeHeapSize());
}
