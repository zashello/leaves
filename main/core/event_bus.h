#pragma once

#include <stdint.h>
#include <stddef.h>

typedef enum {
    EVENT_WIFI_CONNECTED,
    EVENT_WIFI_DISCONNECTED,
    EVENT_MQTT_CONNECTED,
    EVENT_MQTT_DISCONNECTED,
    EVENT_PROVISION_DONE,
    EVENT_AI_TRIGGER,
    EVENT_AI_COMPLETE,
    EVENT_SENSOR_DATA_READY,
    EVENT_BUTTON_LONG_PRESS,
} app_event_t;

typedef void (*event_callback_t)(app_event_t event, void *data);

void eventBusInit(void);
void eventBusPublish(app_event_t event, void *data, size_t dataLen);
void eventBusSubscribe(app_event_t event, event_callback_t callback);
