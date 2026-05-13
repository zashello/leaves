#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_err.h"
#include "wifi_sta.h"
#include "core/event_bus.h"

static const char *TAG = "WIFI_STA";
static EventGroupHandle_t s_wifi_event_group = NULL;
#define WIFI_CONNECTED_BIT BIT0
static bool g_connected = false;

static void eventHandler(void *arg, esp_event_base_t event_base,
                         int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        g_connected = false;
        esp_wifi_connect();
        eventBusPublish(EVENT_WIFI_DISCONNECTED, NULL, 0);
        ESP_LOGI(TAG, "重试连接 WiFi...");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "获取IP地址:" IPSTR, IP2STR(&event->ip_info.ip));
        g_connected = true;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        eventBusPublish(EVENT_WIFI_CONNECTED, NULL, 0);
    }
}

esp_err_t wifiStaConnect(const device_config_t *config)
{
    if (config == NULL || strlen(config->wifiSsid) == 0) {
        ESP_LOGE(TAG, "无效的WiFi配置");
        return ESP_ERR_INVALID_ARG;
    }

    s_wifi_event_group = xEventGroupCreate();
    if (s_wifi_event_group == NULL) {
        ESP_LOGE(TAG, "事件组创建失败");
        return ESP_FAIL;
    }

    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instanceAnyId;
    esp_event_handler_instance_t instanceGotIp;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &eventHandler,
                                                        NULL,
                                                        &instanceAnyId));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &eventHandler,
                                                        NULL,
                                                        &instanceGotIp));

    wifi_config_t wifiConfig = {0};
    strncpy((char *)wifiConfig.sta.ssid, config->wifiSsid, sizeof(wifiConfig.sta.ssid) - 1);
    strncpy((char *)wifiConfig.sta.password, config->wifiPass, sizeof(wifiConfig.sta.password) - 1);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifiConfig));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi初始化完成，正在连接: %s", config->wifiSsid);
    return ESP_OK;
}

bool wifiStaWaitConnected(uint32_t timeoutMs)
{
    if (s_wifi_event_group == NULL) {
        ESP_LOGE(TAG, "WiFi未初始化");
        return false;
    }
    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_event_group,
        WIFI_CONNECTED_BIT,
        pdFALSE,
        pdTRUE,
        pdMS_TO_TICKS(timeoutMs)
    );
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "WiFi已连接");
        return true;
    }
    ESP_LOGW(TAG, "WiFi连接超时");
    return false;
}

bool wifiStaIsConnected(void)
{
    return g_connected;
}
