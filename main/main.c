#include <stdio.h>
#include "servo.h"
#include <as7341.h>

void app_main(void) {
    // 初始化电机控制
    ESP_LOGI("APP", "Starting leaves application");
    
    // 初始化360度电机控制
    esp_err_t ret = servo_init(LEDC_CHANNEL_0, GPIO_NUM_18);
    if (ret != ESP_OK) {
        ESP_LOGE("APP", "Failed to initialize servo motor: %s", esp_err_to_name(ret));
        return;
    }
    
    ESP_LOGI("APP", "Motor initialized successfully");
    
    // 测试电机正向旋转3秒
    vTaskDelay(pdMS_TO_TICKS(1000));  // 等待1秒
    ESP_LOGI("APP", "Testing forward rotation for 3 seconds...");
    servo_forward_for_time(3000);
    
    // 等待电机停止
    vTaskDelay(pdMS_TO_TICKS(5000));  // 等待5秒让电机自然停止

    // 测试其他功能
    ESP_LOGI("APP", "Testing different speeds...");
    servo_set_speed(0.5f);  // 50%速度正向
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    servo_set_speed(-0.5f); // 50%速度反向
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // 停止电机
    servo_stop();
    ESP_LOGI("APP", "Motor stopped");

    // 清理资源
    servo_deinit();
    ESP_LOGI("APP", "Application completed");
}
