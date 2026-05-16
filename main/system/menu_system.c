#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "app_config.h"
#include "menu_system.h"
#include "menu_display.h"
#include "menu_actions.h"
#include "system/system_log.h"

extern const menu_item_t gSysConfigMenu[];
extern const menu_item_t gWifiConfigMenu[];
extern const menu_item_t gNetworkMenu[];

static const char *TAG = "MENU_SYS";

static menu_context_t gCtx;

static const menu_item_t gRootMenu[] = {
    {"SCD41 DATA",      MENU_ITEM_TYPE_ACTION,   actionShowScd41Data,    NULL},
    {"PLANT ANALYSIS",   MENU_ITEM_TYPE_ACTION,   actionShowPlantAnalysis, NULL},
    {"SYSTEM CONFIG",   MENU_ITEM_TYPE_SUBMENU,  NULL,                   gSysConfigMenu},
    {"NETWORK",         MENU_ITEM_TYPE_SUBMENU,  NULL,                   gNetworkMenu},
    {"ABOUT",           MENU_ITEM_TYPE_ACTION,   actionShowAbout,        NULL},
    {NULL}
};

const menu_item_t gSysConfigMenu[] = {
    {"WIFI CONFIG",     MENU_ITEM_TYPE_SUBMENU,  NULL,                   gWifiConfigMenu},
    {"SYSTEM LOG",      MENU_ITEM_TYPE_ACTION,   actionShowLog,          NULL},
    {"CLEAR LOG",       MENU_ITEM_TYPE_ACTION,   actionClearLog,         NULL},
    {"REBOOT",          MENU_ITEM_TYPE_ACTION,   actionReboot,           NULL},
    {"FACTORY RESET",   MENU_ITEM_TYPE_ACTION,   actionFactoryReset,     NULL},
    {"BACK",            MENU_ITEM_TYPE_BACK,     NULL,                   NULL},
    {NULL}
};

const menu_item_t gWifiConfigMenu[] = {
    {"START PROVISION", MENU_ITEM_TYPE_ACTION,   actionStartProvision,   NULL},
    {"CONNECT WIFI",    MENU_ITEM_TYPE_ACTION,   actionConnectWifi,      NULL},
    {"DISCONNECT WIFI", MENU_ITEM_TYPE_ACTION,   actionDisconnectWifi,   NULL},
    {"BACK",            MENU_ITEM_TYPE_BACK,     NULL,                   NULL},
    {NULL}
};

const menu_item_t gNetworkMenu[] = {
    {"MQTT STATUS",     MENU_ITEM_TYPE_ACTION,   actionMqttStatus,       NULL},
    {"TRIGGER UPLOAD",  MENU_ITEM_TYPE_ACTION,   actionTriggerUpload,    NULL},
    {"AI ANALYSIS",     MENU_ITEM_TYPE_ACTION,   actionTriggerAi,        NULL},
    {"BACK",            MENU_ITEM_TYPE_BACK,     NULL,                   NULL},
    {NULL}
};

static int countItems(const menu_item_t *menu)
{
    int count = 0;
    while (menu != NULL && menu[count].label != NULL) {
        count++;
    }
    return count;
}

void menuSystemInit(void)
{
    memset(&gCtx, 0, sizeof(gCtx));
    gCtx.current = gRootMenu;
    gCtx.depth = 0;
    gCtx.selected = 0;
    gCtx.itemCount = countItems(gRootMenu);
    gCtx.state = MENU_STATE_READY;
    gCtx.initialized = true;
    gCtx.lastActivity = xTaskGetTickCount();
    gCtx.confirmAction = NULL;

    ESP_LOGI(TAG, "MENU SYSTEM INITIALIZED, %d ITEMS", gCtx.itemCount);
}

void menuSystemShow(void)
{
    if (!gCtx.initialized) return;

    switch (gCtx.state) {
        case MENU_STATE_READY:
            menuDisplayShowMenu(&gCtx);
            break;
        case MENU_STATE_WAITING:
            break;
        case MENU_STATE_DATA_VIEW:
            break;
        case MENU_STATE_CONFIRM:
            break;
    }
}

static void enterMenu(const menu_item_t *menu)
{
    if (gCtx.depth >= MENU_MAX_DEPTH - 1) return;

    gCtx.path[gCtx.depth] = gCtx.current;
    gCtx.depth++;
    gCtx.current = menu;
    gCtx.selected = 0;
    gCtx.itemCount = countItems(menu);
    gCtx.state = MENU_STATE_READY;

    systemLogAdd(LOG_LEVEL_INFO, "ENTER MENU");
}

static void goBack(void)
{
    if (gCtx.depth <= 0) return;

    gCtx.depth--;
    gCtx.current = gCtx.path[gCtx.depth];
    gCtx.selected = 0;
    gCtx.itemCount = countItems(gCtx.current);
    gCtx.state = MENU_STATE_READY;

    systemLogAdd(LOG_LEVEL_INFO, "BACK MENU");
}

void menuReturnToRoot(void)
{
    gCtx.depth = 0;
    gCtx.current = gRootMenu;
    gCtx.selected = 0;
    gCtx.itemCount = countItems(gRootMenu);
    gCtx.state = MENU_STATE_READY;

    systemLogAdd(LOG_LEVEL_INFO, "RETURN ROOT MENU");
}

void menuSystemHandleEvent(button_event_t event)
{
    if (!gCtx.initialized) return;

    gCtx.lastActivity = xTaskGetTickCount();

    switch (gCtx.state) {
        case MENU_STATE_READY:
            if (event == BUTTON_EVENT_UP) {
                if (gCtx.selected > 0) {
                    gCtx.selected--;
                }
            } else if (event == BUTTON_EVENT_DOWN) {
                if (gCtx.selected < gCtx.itemCount - 1) {
                    gCtx.selected++;
                }
            } else if (event == BUTTON_EVENT_CONFIRM) {
                const menu_item_t *item = &gCtx.current[gCtx.selected];
                if (item->type == MENU_ITEM_TYPE_SUBMENU && item->submenu != NULL) {
                    enterMenu(item->submenu);
                } else if (item->type == MENU_ITEM_TYPE_ACTION && item->action != NULL) {
                    item->action();
                } else if (item->type == MENU_ITEM_TYPE_BACK) {
                    goBack();
                }
            } else if (event == BUTTON_EVENT_BACK) {
                goBack();
            }
            break;

        case MENU_STATE_DATA_VIEW:
            if (event == BUTTON_EVENT_BACK) {
                gCtx.state = MENU_STATE_READY;
                systemLogAdd(LOG_LEVEL_INFO, "EXIT DATA VIEW");
            }
            break;

        case MENU_STATE_WAITING:
            break;

        case MENU_STATE_CONFIRM:
            if (event == BUTTON_EVENT_CONFIRM) {
                if (gCtx.confirmAction != NULL) {
                    gCtx.confirmAction();
                    gCtx.confirmAction = NULL;
                }
                gCtx.state = MENU_STATE_READY;
            } else if (event == BUTTON_EVENT_BACK) {
                gCtx.confirmAction = NULL;
                gCtx.state = MENU_STATE_READY;
                systemLogAdd(LOG_LEVEL_INFO, "CONFIRM CANCELLED");
            }
            break;

        default:
            break;
    }
}

void menuSystemEnterWaiting(const char *message)
{
    gCtx.state = MENU_STATE_WAITING;
    menuDisplayShowWaiting(message);
    systemLogAdd(LOG_LEVEL_INFO, message);
}

void menuSystemExitWaiting(void)
{
    gCtx.state = MENU_STATE_READY;
}

void menuSystemSetState(menu_state_t state)
{
    gCtx.state = state;
}

menu_context_t* menuSystemGetContext(void)
{
    return &gCtx;
}

void menuSystemResetTimeout(void)
{
    gCtx.lastActivity = xTaskGetTickCount();
}

void menuSystemCheckTimeout(void)
{
    if (!gCtx.initialized) return;
    if (gCtx.state == MENU_STATE_WAITING) return;

    TickType_t elapsed = xTaskGetTickCount() - gCtx.lastActivity;
    if (elapsed > pdMS_TO_TICKS(MENU_TIMEOUT_MS)) {
        if (gCtx.depth != 0 || gCtx.selected != 0 || gCtx.state != MENU_STATE_READY) {
            ESP_LOGI(TAG, "MENU TIMEOUT, RETURN TO ROOT");
            systemLogAdd(LOG_LEVEL_INFO, "MENU TIMEOUT");
            menuReturnToRoot();
        }
    }
}
