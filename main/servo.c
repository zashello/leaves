#include "driver/ledc.h"
#include "esp_err.h"

// 定义舵机连接的GPIO引脚
#define SERVO_PIN 18

// 舵机PWM标准参数
#define SERVO_MIN_PULSE_WIDTH 500000  // 最小脉冲宽度500μs
#define SERVO_MAX_PULSE_WIDTH 2500000 // 最大脉冲宽度2500μs (2.5ms)
#define SERVO_FREQ 50                 // 舵机标准频率50Hz

typedef struct {
    uint8_t pin;
    ledc_channel_t channel;
    ledc_timer_t timer;
} servo_t;

/**
 * 将角度映射到脉冲宽度
 * @param angle 0-180度
 * @return 对应的脉冲宽度(微秒)
 */
uint32_t angle_to_pulse_width(uint8_t angle) {
    return SERVO_MIN_PULSE_WIDTH + (angle * (SERVO_MAX_PULSE_WIDTH - SERVO_MIN_PULSE_WIDTH) / 180);
}

/**
 * 将角度映射到LEDC占空比
 * @param angle 0-180度
 * @param max_duty 最大占空比值
 * @return 对应的占空比值
 */
uint32_t angle_to_duty(uint8_t angle, uint32_t max_duty) {
    uint32_t pulse_width = angle_to_pulse_width(angle);
    return (pulse_width * max_duty) / ((uint32_t)(1000000 / SERVO_FREQ));
}

/**
 * 初始化舵机
 * @param servo 舵机结构体指针
 * @param gpio_pin GPIO引脚号
 * @param channel LEDC通道号
 * @return 成功返回ESP_OK
 */
esp_err_t servo_init(servo_t *servo, uint8_t gpio_pin, ledc_channel_t channel) {
    if (!servo) return ESP_ERR_INVALID_ARG;
    
    servo->pin = gpio_pin;
    servo->channel = channel;
    servo->timer = LEDC_TIMER_0;
    
    // 使用12位分辨率(0-4095)
    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_12_BIT,
        .timer_num = servo->timer,
        .freq_hz = SERVO_FREQ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    
    ledc_channel_config_t channel_conf = {
        .gpio_num = gpio_pin,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = channel,
        .timer_sel = servo->timer,
        .duty = 0,
        .hpoint = 0
    };
    
    // 初始化定时器和通道
    ledc_timer_config(&timer_conf);
    ledc_channel_config(&channel_conf);
    
    return ESP_OK;
}

/**
 * 设置舵机角度
 * @param servo 舵机结构体指针
 * @param angle 0-180度
 * @return 成功返回ESP_OK
 */
esp_err_t servo_set_angle(servo_t *servo, uint8_t angle) {
    if (!servo || angle > 180) return ESP_ERR_INVALID_ARG;
    
    uint32_t max_duty = (1 << 12) - 1;  // 12位分辨率的最大值
    uint32_t duty = angle_to_duty(angle, max_duty);
    
    ledc_set_duty(LEDC_LOW_SPEED_MODE, servo->channel, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, servo->channel);
    
    return ESP_OK;
}

/**
 * 将舵机移动到90度
 * @return 成功返回ESP_OK
 */
void servo_move_to_90_degrees(void) {
    servo_t servo;
    esp_err_t err;
    
    // 初始化舵机
    err = servo_init(&servo, SERVO_PIN, LEDC_CHANNEL_0);
    if (err != ESP_OK) {
        return;
    }
    
    // 移动到90度
    err = servo_set_angle(&servo, 90);
    if (err != ESP_OK) {
        return;
    }
}

/**
 * 将舵机移动到指定角度
 * @param angle 0-180度
 * @return 成功返回ESP_OK
 */
void servo_move_to_angle(uint8_t angle) {
    servo_t servo;
    esp_err_t err;
    
    // 初始化舵机
    err = servo_init(&servo, SERVO_PIN, LEDC_CHANNEL_0);
    if (err != ESP_OK) {
        return;
    }
    
    // 移动到指定角度
    err = servo_set_angle(&servo, angle);
    if (err != ESP_OK) {
        return;
    }
}