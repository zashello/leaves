#include "driver/ledc.h"
// 定义LED连接的GPIO引脚
#define LED_PIN 2
// 初始化LEDC定时器的配置结构体
ledc_timer_config_t ledc_timer = {
    .speed_mode = LEDC_LOW_SPEED_MODE, // 低速模式
    .duty_resolution = LEDC_TIMER_13_BIT, // 分辨率为13位
    .timer_num = LEDC_TIMER_0, // 定时器编号
    .freq_hz = 1000, // PWM信号的频率, 例如1000 Hz
    .clk_cfg = LEDC_AUTO_CLK, // 自动选择时钟源
};
// 初始化LEDC通道的配置结构体
ledc_channel_config_t ledc_channel = {
    .gpio_num   = LED_PIN,
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .channel    = LEDC_CHANNEL_0,
    .timer_sel  = LEDC_TIMER_0,
    .duty       = 0, // 初始占空比为0
    .hpoint     = 0
};
// 初始化LEDC定时器
ledc_timer_config(&amp;ledc_timer);
// 初始化LEDC通道
ledc_channel_config(&amp;ledc_channel);
// 设置通道的占空比到一定的值，例如50%
ledc_set_duty(ledc_channel.speed_mode, ledc_channel.channel, 1 &lt;&lt; (ledc_timer.duty_resolution - 1));
ledc_update_duty(ledc_channel.speed_mode, ledc_channel.channel);