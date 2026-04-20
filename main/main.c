#include <stdio.h>
#include "servo.h"
#include <as7341.h>

void app_main(void) {
    // 移动舵机到90度
    servo_move_to_90_degrees();
    printf("Servo moved to 90 degrees\n");
    
    // 可选：延迟一段时间后移到其他角度
    // vTaskDelay(2000 / portTICK_PERIOD_MS);  // 延迟2秒
    // servo_move_to_angle(45);
    // printf("Servo moved to 45 degrees\n");
}
