#pragma once

#define MQTT_DISCOVERY_PREFIX       "homeassistant"
#define MQTT_SENSOR_BASE_TOPIC      "leaves"
#define MQTT_DEVICE_ID              "leaves_sensor"
#define MQTT_DEVICE_NAME            "Leaves Plant Monitor"
#define MQTT_DEVICE_MANUFACTURER    "Leaves"
#define MQTT_DEVICE_MODEL           "Spectral Monitor v1.0"

#define MQTT_STATE_TOPIC            "leaves/status"
#define MQTT_LWT_TOPIC              "leaves/status"
#define MQTT_LWT_PAYLOAD_OFFLINE    "offline"
#define MQTT_LWT_PAYLOAD_ONLINE     "online"

#define MQTT_TRIGGER_AI_TOPIC       "leaves/trigger_ai"
#define MQTT_TRIGGER_AI_RESULT      "leaves/trigger_ai/result"

#define MQTT_SENSOR_UPDATE_INTERVAL_MS    600000  // 10分钟

typedef enum {
    HA_MQTT_STATE_DISCONNECTED = 0,
    HA_MQTT_STATE_CONNECTING,
    HA_MQTT_STATE_CONNECTED,
    HA_MQTT_STATE_RECONNECTING
} ha_mqtt_state_t;

#define HA_MQTT_RECONNECT_DELAY_MS    5000
#define HA_MQTT_CONNECT_TIMEOUT_MS    30000
