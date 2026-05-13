#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "service/sensor_service.h"
#include "task_sensor.h"

static const char *TAG = "TASK_SENSOR";

void taskSensor(void *param)
{
    ESP_LOGI(TAG, "传感器定时任务启动");
    sensorServiceStart();
    vTaskDelete(NULL);
}
