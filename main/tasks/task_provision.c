#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "core/app_state.h"
#include "network/provision/prov_main.h"
#include "task_provision.h"

static const char *TAG = "TASK_PROV";

void taskProvision(void *param)
{
    ESP_LOGI(TAG, "配网任务启动");
    appStateSet(APP_STATE_PROVISION);

    esp_err_t ret = provStart();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "配网启动失败");
        appStateSet(APP_STATE_ERROR);
    }

    vTaskDelete(NULL);
}
