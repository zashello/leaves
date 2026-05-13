#include <string.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_http_server.h"
#include "cJSON.h"
#include "app_config.h"
#include "storage/storage.h"
#include "prov_html.h"
#include "prov_http.h"

static const char *TAG = "PROV_HTTP";

static esp_err_t indexHandler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, char_html, sizeof(char_html));
    return ESP_OK;
}

static esp_err_t scanHandler(httpd_req_t *req)
{
    uint16_t apCount = 0;
    esp_wifi_scan_start(NULL, true);
    esp_wifi_scan_get_ap_num(&apCount);

    wifi_ap_record_t *apList = calloc(apCount, sizeof(wifi_ap_record_t));
    if (!apList) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    esp_wifi_scan_get_ap_records(&apCount, apList);

    cJSON *root = cJSON_CreateArray();
    for (int i = 0; i < apCount; i++) {
        cJSON *ap = cJSON_CreateObject();
        cJSON_AddStringToObject(ap, "ssid", (const char *)apList[i].ssid);
        cJSON_AddNumberToObject(ap, "rssi", apList[i].rssi);
        cJSON_AddNumberToObject(ap, "auth", apList[i].authmode);
        cJSON_AddItemToArray(root, ap);
    }
    char *jsonStr = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    free(apList);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, jsonStr);
    free(jsonStr);
    return ESP_OK;
}

static esp_err_t configGetHandler(httpd_req_t *req)
{
    device_config_t config;
    memset(&config, 0, sizeof(config));
    storageLoad(&config);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "ssid", config.wifiSsid);
    cJSON_AddStringToObject(root, "sfKey", config.siliconflowKey);
    cJSON_AddStringToObject(root, "scKey", config.serverchanKey);
    cJSON_AddStringToObject(root, "mqttServer", config.mqttServer);
    cJSON_AddNumberToObject(root, "mqttPort", config.mqttPort);
    cJSON_AddStringToObject(root, "mqttUser", config.mqttUsername);
    cJSON_AddStringToObject(root, "mqttPass", config.mqttPassword);
    cJSON_AddStringToObject(root, "deviceName", config.deviceName);

    char *jsonStr = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, jsonStr);
    free(jsonStr);
    return ESP_OK;
}

static esp_err_t saveHandler(httpd_req_t *req)
{
    char buf[1024] = {0};
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"ok\":false,\"msg\":\"JSON解析失败\"}");
        return ESP_FAIL;
    }

    device_config_t config;
    memset(&config, 0, sizeof(config));

    cJSON *item = NULL;

    item = cJSON_GetObjectItem(root, "ssid");
    if (item && cJSON_IsString(item)) strncpy(config.wifiSsid, item->valuestring, sizeof(config.wifiSsid) - 1);

    item = cJSON_GetObjectItem(root, "password");
    if (item && cJSON_IsString(item)) strncpy(config.wifiPass, item->valuestring, sizeof(config.wifiPass) - 1);

    item = cJSON_GetObjectItem(root, "sfKey");
    if (item && cJSON_IsString(item)) strncpy(config.siliconflowKey, item->valuestring, sizeof(config.siliconflowKey) - 1);

    item = cJSON_GetObjectItem(root, "scKey");
    if (item && cJSON_IsString(item)) strncpy(config.serverchanKey, item->valuestring, sizeof(config.serverchanKey) - 1);

    item = cJSON_GetObjectItem(root, "mqttServer");
    if (item && cJSON_IsString(item)) strncpy(config.mqttServer, item->valuestring, sizeof(config.mqttServer) - 1);

    item = cJSON_GetObjectItem(root, "mqttPort");
    if (item && cJSON_IsNumber(item)) {
        config.mqttPort = (uint16_t)item->valueint;
    } else {
        config.mqttPort = 1883;
    }

    item = cJSON_GetObjectItem(root, "mqttUser");
    if (item && cJSON_IsString(item)) strncpy(config.mqttUsername, item->valuestring, sizeof(config.mqttUsername) - 1);

    item = cJSON_GetObjectItem(root, "mqttPass");
    if (item && cJSON_IsString(item)) strncpy(config.mqttPassword, item->valuestring, sizeof(config.mqttPassword) - 1);

    item = cJSON_GetObjectItem(root, "deviceName");
    if (item && cJSON_IsString(item)) {
        strncpy(config.deviceName, item->valuestring, sizeof(config.deviceName) - 1);
    } else {
        strncpy(config.deviceName, "leaves_device", sizeof(config.deviceName) - 1);
    }

    cJSON_Delete(root);

    if (strlen(config.wifiSsid) == 0 || strlen(config.siliconflowKey) == 0 || strlen(config.serverchanKey) == 0) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"ok\":false,\"msg\":\"必填字段不能为空\"}");
        return ESP_FAIL;
    }

    if (config.mqttPort == 0) config.mqttPort = 1883;
    if (strlen(config.deviceName) == 0) strncpy(config.deviceName, "leaves_device", sizeof(config.deviceName) - 1);

    config.configValid = true;
    esp_err_t err = storageSave(&config);
    if (err != ESP_OK) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"ok\":false,\"msg\":\"保存失败\"}");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true,\"msg\":\"配置已保存，设备即将重启\"}");

    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();

    return ESP_OK;
}

static esp_err_t resetHandler(httpd_req_t *req)
{
    storageClear();
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true,\"msg\":\"配置已清除，设备即将重启\"}");
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();
    return ESP_OK;
}

esp_err_t provHttpStart(httpd_handle_t *outServer)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.max_uri_handlers = 8;
    config.stack_size = 8192;

    httpd_handle_t server = NULL;
    esp_err_t ret = httpd_start(&server, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "HTTP服务器启动失败: %s", esp_err_to_name(ret));
        return ret;
    }

    httpd_uri_t indexUri = { .uri = "/", .method = HTTP_GET, .handler = indexHandler };
    httpd_uri_t scanUri = { .uri = "/api/scan", .method = HTTP_GET, .handler = scanHandler };
    httpd_uri_t configGetUri = { .uri = "/api/config", .method = HTTP_GET, .handler = configGetHandler };
    httpd_uri_t saveUri = { .uri = "/api/save", .method = HTTP_POST, .handler = saveHandler };
    httpd_uri_t resetUri = { .uri = "/api/reset", .method = HTTP_POST, .handler = resetHandler };

    httpd_register_uri_handler(server, &indexUri);
    httpd_register_uri_handler(server, &scanUri);
    httpd_register_uri_handler(server, &configGetUri);
    httpd_register_uri_handler(server, &saveUri);
    httpd_register_uri_handler(server, &resetUri);

    *outServer = server;
    ESP_LOGI(TAG, "HTTP配网服务器已启动，端口: 80");
    return ESP_OK;
}

void provHttpStop(httpd_handle_t server)
{
    if (server) {
        httpd_stop(server);
        ESP_LOGI(TAG, "HTTP配网服务器已停止");
    }
}
