#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/driver_button.h"
#include "app_config.h"

typedef enum {
    MENU_ITEM_TYPE_BACK = 0,
    MENU_ITEM_TYPE_ACTION,
    MENU_ITEM_TYPE_SUBMENU
} menu_item_type_t;

typedef struct menu_item {
    const char *label;
    menu_item_type_t type;
    void (*action)(void);
    const struct menu_item *submenu;
} menu_item_t;

typedef enum {
    MENU_STATE_READY = 0,
    MENU_STATE_WAITING,
    MENU_STATE_DATA_VIEW,
    MENU_STATE_CONFIRM
} menu_state_t;

typedef struct {
    const menu_item_t *current;
    const menu_item_t *path[MENU_MAX_DEPTH];
    int depth;
    int selected;
    int itemCount;
    menu_state_t state;
    bool initialized;
    TickType_t lastActivity;
    void (*confirmAction)(void);
} menu_context_t;

void menuSystemInit(void);
void menuSystemShow(void);
void menuSystemHandleEvent(button_event_t event);
void menuSystemEnterWaiting(const char *message);
void menuSystemExitWaiting(void);
void menuSystemSetState(menu_state_t state);
menu_context_t* menuSystemGetContext(void);
void menuSystemResetTimeout(void);
void menuSystemCheckTimeout(void);
