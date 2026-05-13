#pragma once

#include <stdint.h>
#include <stdbool.h>

#define WIFI_SSID_MAX_LEN      32
#define WIFI_PASS_MAX_LEN      64
#define API_KEY_MAX_LEN        128
#define SENDKEY_MAX_LEN        64
#define MQTT_SERVER_MAX_LEN    128
#define MQTT_USER_MAX_LEN      64
#define MQTT_PASS_MAX_LEN      64
#define DEVICE_NAME_MAX_LEN    32

#define PROVISION_SSID         "Leaves_Setup"
#define PROVISION_PASS         "12345678"

#define AS7341_I2C_SCL         5
#define AS7341_I2C_SDA         4
#define AS7341_I2C_FREQ        100000

#define SCD41_I2C_SCL          15
#define SCD41_I2C_SDA          18
#define SCD41_I2C_PORT         1
#define SCD41_I2C_FREQ         100000

#define BUTTON_GPIO            10
#define BUTTON_DEBOUNCE_MS     50
#define BUTTON_LONG_PRESS_MS   3000

#define AI_API_ENDPOINT        "http://api.siliconflow.cn/v1/chat/completions"
#define AI_MODEL_NAME          "Qwen/Qwen3-VL-30B-A3B-Instruct"
#define AI_MAX_TOKENS          10000
#define AI_TEMPERATURE         0.7
#define AI_TOP_P               0.7

#define MQTT_DISCOVERY_PREFIX  "homeassistant"
#define MQTT_SENSOR_BASE       "leaves"
#define MQTT_DEVICE_ID         "leaves_sensor"
#define MQTT_DEVICE_NAME       "Leaves Plant Monitor"
#define MQTT_MANUFACTURER      "Leaves"
#define MQTT_MODEL             "Spectral Monitor v1.0"
#define MQTT_STATE_TOPIC       "leaves/status"
#define MQTT_LWT_OFFLINE       "offline"
#define MQTT_LWT_ONLINE        "online"
#define MQTT_TRIGGER_TOPIC     "leaves/trigger_ai"
#define MQTT_TRIGGER_RESULT    "leaves/trigger_ai/result"
#define MQTT_RECONNECT_MS      5000
#define MQTT_CONNECT_TIMEOUT   30000
#define MQTT_SENSOR_INTERVAL   600000

#define HTTP_OUTPUT_BUFFER     8192
#define SERVERCHAN_URL_PREFIX  "http://sctapi.ftqq.com/"

typedef struct {
    char wifiSsid[WIFI_SSID_MAX_LEN];
    char wifiPass[WIFI_PASS_MAX_LEN];
    char siliconflowKey[API_KEY_MAX_LEN];
    char serverchanKey[SENDKEY_MAX_LEN];
    char mqttServer[MQTT_SERVER_MAX_LEN];
    uint16_t mqttPort;
    char mqttUsername[MQTT_USER_MAX_LEN];
    char mqttPassword[MQTT_PASS_MAX_LEN];
    char deviceName[DEVICE_NAME_MAX_LEN];
    bool configValid;
} device_config_t;
