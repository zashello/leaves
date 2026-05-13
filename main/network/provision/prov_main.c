#include <string.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "prov_main.h"
#include "prov_http.h"
#include "prov_dns.h"
#include "app_config.h"

static const char *TAG = "PROVISION";
static prov_state_t g_state = PROV_STATE_IDLE;
static httpd_handle_t g_http_server = NULL;

static void printApInfo(void)
{
    esp_netif_ip_info_t ipInfo;
    esp_netif_t *ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");

    if (esp_netif_get_ip_info(ap_netif, &ipInfo) == ESP_OK) {
        ESP_LOGI(TAG, "配网热点已启动:");
        ESP_LOGI(TAG, "  SSID: " PROVISION_SSID);
        ESP_LOGI(TAG, "  密码: " PROVISION_PASS);
        ESP_LOGI(TAG, "  IP: " IPSTR, IP2STR(&ipInfo.ip));
    }
}

esp_err_t provStart(void)
{
    if (g_state == PROV_STATE_PROVISIONING) {
        ESP_LOGW(TAG, "配网模式已在运行");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "启动配网模式...");

    esp_err_t ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "netif初始化失败");
        return ret;
    }

    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "事件循环创建失败");
        return ret;
    }

    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t staConfig = {0};
    wifi_config_t apConfig = {
        .ap = {
            .ssid = PROVISION_SSID,
            .ssid_len = strlen(PROVISION_SSID),
            .password = PROVISION_PASS,
            .channel = 1,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .max_connection = 4,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &staConfig));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &apConfig));
    ESP_ERROR_CHECK(esp_wifi_start());

    vTaskDelay(pdMS_TO_TICKS(1000));
    printApInfo();

    ret = provHttpStart(&g_http_server);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "HTTP服务器启动失败");
        g_state = PROV_STATE_FAILED;
        return ret;
    }

    ret = provDnsStart();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Captive Portal启动失败，配网页面仍可手动访问");
    }

    g_state = PROV_STATE_PROVISIONING;
    ESP_LOGI(TAG, "配网模式已就绪，等待用户配置");
    return ESP_OK;
}

void provStop(void)
{
    if (g_state != PROV_STATE_PROVISIONING) return;

    provDnsStop();
    provHttpStop(g_http_server);
    g_http_server = NULL;

    g_state = PROV_STATE_IDLE;
    ESP_LOGI(TAG, "配网模式已停止");
}

prov_state_t provGetState(void)
{
    return g_state;
}
