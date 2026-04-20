#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "servo.h"

// 日志标签
#define TAG "SERVO"

// 硬件配置
#define SERVO_LEDC_CHANNEL LEDC_CHANNEL_0
#define SERVO_LEDC_TIMER   LEDC_TIMER_0
#define SERVO_GPIO_NUM GPIO_NUM_18  // 可根据实际硬件连接修改

typedef struct {
    bool initialized;
    ledc_channel_t channel;
    gpio_num_t gpio_num;
    TimerHandle_t auto_stop_timer;
} servo_handle_t;

static servo_handle_t servo_data;

/**
 * @brief 裁剪值到指定范围
 * @param value 值
 * @param min 最小值
 * @param max 最大值
 * @return float 裁剪后的值
 */
static inline float CLAMP(float value, float min, float max) {
    return value < min ? min : (value > max ? max : value);
}

/**
 * @brief 启动自动停止定时器
 * @param milliseconds 延迟时间(毫秒)
 * @return esp_err_t ESP_OK on success
 */
static esp_err_t start_auto_stop_timer(uint32_t milliseconds) {
    if (servo_data.auto_stop_timer != NULL) {
        // 停止之前的定时器
        xTimerStop(servo_data.auto_stop_timer, portMAX_DELAY);
    }
    
    // 创建新的定时器
    servo_data.auto_stop_timer = xTimerCreate(
        "ServoAutoStop",
        pdMS_TO_TICKS(milliseconds),
        pdFALSE,  // 单次定时器
        (void*)0,
        auto_stop_timer_callback
    );
    
    if (servo_data.auto_stop_timer == NULL) {
        ESP_LOGE(TAG, "Failed to create auto-stop timer");
        return ESP_FAIL;
    }
    
    // 启动定时器
    if (xTimerStart(servo_data.auto_stop_timer, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to start auto-stop timer");
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

/**
 * @brief 自动停止定时器回调函数
 * @param xTimer 定时器句柄
 */
static void auto_stop_timer_callback(TimerHandle_t xTimer) {
    ESP_LOGI(TAG, "Auto-stop timer expired, stopping servo");
    servo_stop();
}

/**
 * @brief 配置LEDC定时器和通道
 * @param gpio_num GPIO引脚号
 * @return esp_err_t ESP_OK on success
 */
static esp_err_t ledc_init(uint32_t gpio_num) {
    // 配置LEDC定时器
    ledc_timer_config_t timer_config = {
        .duty_resolution = LEDC_TIMER_13_BIT,    // 13位分辨率 (0-8191)
        .freq_hz = 50,                          // 50Hz频率 (20ms周期)
        .timer_num = SERVO_LEDC_TIMER,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    
    ESP_LOGI(TAG, "Configuring LEDC timer %d with 50Hz frequency", SERVO_LEDC_TIMER);
    esp_err_t ret = ledc_timer_config(&timer_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure LEDC timer: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 配置LEDC通道
    ledc_channel_config_t channel_config = {
        .channel    = SERVO_LEDC_CHANNEL,
        .duty       = 0,
        .gpio_num   = gpio_num,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_sel  = SERVO_LEDC_TIMER,
        .hpoint     = 0,
    };
    
    ESP_LOGI(TAG, "Configuring LEDC channel %d on GPIO %d", SERVO_LEDC_CHANNEL, gpio_num);
    ret = ledc_channel_config(&channel_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure LEDC channel: %s", esp_err_to_name(ret));
        return ret;
    }
    
    return ESP_OK;
}

/**
 * @brief 将速度值转换为脉冲宽度
 * @param speed 速度值 (-1.0 ~ 1.0)
 * @return uint32_t 脉冲宽度(微秒)
 */
static uint32_t speed_to_pulse_width(float speed) {
    float pulse_us;
    
    if (speed >= 0.0f) {
        // 正向速度: 0.0 ~ 1.0 -> 1500 ~ 500μs
        pulse_us = SERVO_MIN_PULSE_US + (SERVO_NEUTRAL_PULSE_US - SERVO_MIN_PULSE_US) * (1.0f - speed);
    } else {
        // 反向速度: -1.0 ~ 0.0 -> 2500 ~ 1500μs
        pulse_us = SERVO_MAX_PULSE_US + (SERVO_NEUTRAL_PULSE_US - SERVO_MAX_PULSE_US) * (1.0f + speed);
    }
    
    // 限制在有效范围内
    pulse_us = CLAMP(pulse_us, SERVO_MIN_PULSE_US, SERVO_MAX_PULSE_US);
    
    return (uint32_t)pulse_us;
}

/**
 * @brief 将脉冲宽度转换为LEDC占空比
 * @param pulse_us 脉冲宽度(微秒)
 * @return uint32_t LEDC占空比值
 */
static uint32_t servo_pulse_to_duty(uint32_t pulse_us) {
    return (pulse_us * (1 << LEDC_TIMER_13_BIT)) / SERVO_PERIOD_US;
}

/**
 * @brief 初始化360度电机控制
 * @param channel LEDC通道号
 * @param gpio_num 电机信号输出的GPIO引脚
 * @return esp_err_t ESP_OK on success
 */
 esp_err_t servo_init(ledc_channel_t channel, gpio_num_t gpio_num) {
    if (servo_data.initialized) {
        ESP_LOGW(TAG, "Servo already initialized");
        return ESP_OK;
    }
    
    // 保存配置
    servo_data.channel = channel;
    servo_data.gpio_num = gpio_num;
    
    ESP_LOGI(TAG, "Initializing servo motor on channel %d, GPIO %d", channel, gpio_num);
    
    // 初始化LEDC硬件
    esp_err_t ret = ledc_init(gpio_num);
    if (ret != ESP_OK) {
        return ret;
    }
    
    // 初始化完成
    servo_data.initialized = true;
    
    // 默认停止电机
    ret = servo_stop();
    
    ESP_LOGI(TAG, "Servo motor initialized successfully");
    return ret;
}

/**
 * @brief 设置电机速度
 * @param speed 速度值 (-1.0 ~ 1.0)
 * @return esp_err_t ESP_OK on success
 */
 esp_err_t servo_set_speed(float speed) {
    if (!servo_data.initialized) {
        ESP_LOGE(TAG, "Servo not initialized");
        return ESP_ERR_NOT_FINISHED;
    }
    
    // 裁剪速度值
    speed = CLAMP(speed, -1.0f, 1.0f);
    
    uint32_t pulse_us = speed_to_pulse_width(speed);
    uint32_t duty = servo_pulse_to_duty(pulse_us);
    
    ESP_LOGD(TAG, "Setting servo speed to %.2f, pulse %uum, duty %u", speed, pulse_us, duty);
    
    // 设置占空比
    return ledc_set_duty(LEDC_LOW_SPEED_MODE, servo_data.channel, duty);
}

/**
 * @brief 设置电机方向速度
 * @param direction 电机方向
 * @param speed 速度系数 (0.0 ~ 1.0)
 * @return esp_err_t ESP_OK on success
 */
 esp_err_t servo_set_direction_speed(servo_direction_t direction, float speed) {
    if (!servo_data.initialized) {
        ESP_LOGE(TAG, "Servo not initialized");
        return ESP_ERR_NOT_FINISHED;
    }
    
    speed = CLAMP(speed, 0.0f, 1.0f);
    
    float final_speed = (direction == SERVO_FORWARD) ? speed : -speed;
    
    return servo_set_speed(final_speed);
}

/**
 * @brief 电机正向旋转指定时间
 * @param milliseconds 正向旋转时间(毫秒)
 * @return esp_err_t ESP_OK on success
 */
 esp_err_t servo_forward_for_time(uint32_t milliseconds) {
    if (!servo_data.initialized) {
        ESP_LOGE(TAG, "Servo not initialized");
        return ESP_ERR_NOT_FINISHED;
    }
    
    ESP_LOGI(TAG, "Starting servo forward rotation for %u ms", milliseconds);
    
    // 启动正向旋转
    esp_err_t ret = servo_set_direction_speed(SERVO_FORWARD, 1.0f);
    if (ret != ESP_OK) {
        return ret;
    }
    
    // 启动自动停止定时器
    ret = start_auto_stop_timer(milliseconds);
    if (ret != ESP_OK) {
        // 如果定时器创建失败，提醒用户手动停止
        ESP_LOGW(TAG, "Auto-stop timer failed, please call servo_stop() manually");
    }
    
    return ESP_OK;
}

/**
 * @brief 停止电机
 * @return esp_err_t ESP_OK on success
 */
 esp_err_t servo_stop(void) {
    if (!servo_data.initialized) {
        ESP_LOGE(TAG, "Servo not initialized");
        return ESP_ERR_NOT_FINISHED;
    }
    
    // 停止自动停止定时器
    if (servo_data.auto_stop_timer != NULL) {
        xTimerStop(servo_data.auto_stop_timer, 0);
        xTimerDelete(servo_data.auto_stop_timer, 0);
        servo_data.auto_stop_timer = NULL;
    }
    
    // 设置中立位置(停止)
    uint32_t duty = servo_pulse_to_duty(SERVO_NEUTRAL_PULSE_US);
    esp_err_t ret = ledc_set_duty(LEDC_LOW_SPEED_MODE, servo_data.channel, duty);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Servo motor stopped");
    }
    
    return ret;
}

/**
 * @brief 清理电机控制资源
 * @return esp_err_t ESP_OK on success
 */
 esp_err_t servo_deinit(void) {
    if (!servo_data.initialized) {
        ESP_LOGW(TAG, "Servo not initialized");
        return ESP_OK;
    }
    
    // 停止电机
    servo_stop();
    
    // 清理定时器
    if (servo_data.auto_stop_timer != NULL) {
        xTimerDelete(servo_data.auto_stop_timer, 0);
        servo_data.auto_stop_timer = NULL;
    }
    
    // 清理LEDC通道
    ledc_stop(LEDC_LOW_SPEED_MODE, servo_data.channel, false);
    
    // 重置状态
    servo_data.initialized = false;
    
    ESP_LOGI(TAG, "Servo motor deinitialized");
    return ESP_OK;
}