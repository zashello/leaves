#include <string.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "captive_portal.h"

static const char *TAG = "CAPTIVE";
static int g_dns_socket = -1;
static TaskHandle_t g_dns_task = NULL;
static bool g_running = false;

static void dnsServerTask(void *pvParameters)
{
    uint8_t rxBuffer[128];
    struct sockaddr_in clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);
    esp_netif_t *ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");

    esp_netif_ip_info_t ipInfo;
    esp_netif_get_ip_info(ap_netif, &ipInfo);
    uint32_t apIp = ipInfo.ip.addr;

    while (g_running) {
        int recvLen = recvfrom(g_dns_socket, rxBuffer, sizeof(rxBuffer), 0,
                               (struct sockaddr *)&clientAddr, &clientAddrLen);
        if (recvLen < 0) {
            if (!g_running) break;
            continue;
        }
        if (recvLen < 12) continue;

        uint16_t queryLen = (rxBuffer[12] << 8) | (rxBuffer[12 + 1]);
        if (queryLen > 63) continue;

        uint8_t txBuffer[256];
        int txLen = recvLen;

        memcpy(txBuffer, rxBuffer, recvLen);

        txBuffer[2] = 0x81;
        txBuffer[3] = 0x80;

        uint16_t qdCount = (rxBuffer[4] << 8) | rxBuffer[5];
        txBuffer[6] = (qdCount >> 8) & 0xFF;
        txBuffer[7] = qdCount & 0xFF;

        uint16_t anCount = qdCount;
        txBuffer[8] = (anCount >> 8) & 0xFF;
        txBuffer[9] = anCount & 0xFF;

        txBuffer[10] = 0x00;
        txBuffer[11] = 0x00;

        int offset = recvLen;
        if (offset + 16 > (int)sizeof(txBuffer)) continue;

        txBuffer[offset++] = 0xC0;
        txBuffer[offset++] = 0x0C;

        txBuffer[offset++] = 0x00;
        txBuffer[offset++] = 0x01;

        txBuffer[offset++] = 0x80;
        txBuffer[offset++] = 0x00;

        txBuffer[offset++] = 0x00;
        txBuffer[offset++] = 0x00;
        txBuffer[offset++] = 0x00;
        txBuffer[offset++] = 0x3C;

        txBuffer[offset++] = 0x00;
        txBuffer[offset++] = 0x04;

        memcpy(&txBuffer[offset], &apIp, 4);
        offset += 4;

        txLen = offset;

        sendto(g_dns_socket, txBuffer, txLen, 0,
               (struct sockaddr *)&clientAddr, clientAddrLen);
    }

    close(g_dns_socket);
    g_dns_socket = -1;
    vTaskDelete(NULL);
    g_dns_task = NULL;
}

esp_err_t captivePortalStart(void)
{
    if (g_running) {
        ESP_LOGW(TAG, "Captive Portal已在运行");
        return ESP_OK;
    }

    g_dns_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (g_dns_socket < 0) {
        ESP_LOGE(TAG, "创建DNS socket失败");
        return ESP_FAIL;
    }

    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(53);
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(g_dns_socket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        ESP_LOGE(TAG, "DNS bind失败");
        close(g_dns_socket);
        g_dns_socket = -1;
        return ESP_FAIL;
    }

    g_running = true;

    BaseType_t ret = xTaskCreate(dnsServerTask, "dns_server", 4096, NULL, 5, &g_dns_task);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "创建DNS任务失败");
        close(g_dns_socket);
        g_dns_socket = -1;
        g_running = false;
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Captive Portal DNS服务已启动");
    return ESP_OK;
}

void captivePortalStop(void)
{
    g_running = false;
    if (g_dns_socket >= 0) {
        shutdown(g_dns_socket, SHUT_RDWR);
        close(g_dns_socket);
        g_dns_socket = -1;
    }
    if (g_dns_task != NULL) {
        vTaskDelay(pdMS_TO_TICKS(500));
        g_dns_task = NULL;
    }
    ESP_LOGI(TAG, "Captive Portal已停止");
}
