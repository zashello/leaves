#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "driver/gpio.h"
#include "driver/ledc.h"

// 360度电机PWM信号定义
#define SERVO_MIN_PULSE_US  500    // 最大正向速度: 0.5ms
#define SERVO_NEUTRAL_PULSE_US 1500 // 停止位置: 1.5ms
#define SERVO_MAX_PULSE_US  2500    // 最大反向速度: 2.5ms
#define SERVO_PERIOD_US     20000   // PWM周期: 20ms (50Hz)

/**
 * @brief 电机速度类型定义
 */
 typedef enum {
    SERVO_STOP = 0,
    SERVO_FORWARD = 1,
    SERVO_BACKWARD = -1
} servo_direction_t;

/**
 * @brief 电机控制API函数声明
 */

/**
 * @brief 初始化360度电机控制
 * @param channel LEDC通道号
 * @param gpio_num 电机信号输出的GPIO引脚
 * @return esp_err_t ESP_OK on success
 */
 esp_err_t servo_init(ledc_channel_t channel, gpio_num_t gpio_num);

/**
 * @brief 设置电机速度
 * @param speed 范围: -1.0(最大反向) ~ 0.0(停止) ~ 1.0(最大正向)
 * @return esp_err_t ESP_OK on success
 */
 esp_err_t servo_set_speed(float speed);

/**
 * @brief 设置电机方向速度
 * @param direction 电机方向
 * @param speed 速度系数 (0.0 ~ 1.0)
 * @return esp_err_t ESP_OK on success
 */
 esp_err_t servo_set_direction_speed(servo_direction_t direction, float speed);

/**
 * @brief 电机正向旋转指定时间
 * @param milliseconds 正向旋转时间(毫秒)
 * @return esp_err_t ESP_OK on success
 */
 esp_err_t servo_forward_for_time(uint32_t milliseconds);

/**
 * @brief 停止电机
 * @return esp_err_t ESP_OK on success
 */
 esp_err_t servo_stop(void);

/**
 * @brief 清理电机控制资源
 * @return esp_err_t ESP_OK on success
 */
 esp_err_t servo_deinit(void);

/**
 * @brief 将脉冲宽度转换为LEDC占空比
 * @param pulse_us 脉冲宽度(微秒)
 * @return uint32_t LEDC占空比值
 */
 static inline uint32_t servo_pulse_to_duty(uint32_t pulse_us) {
    return (pulse_us * (1 << LEDC_TIMER_13_BIT)) / SERVO_PERIOD_US;
}

