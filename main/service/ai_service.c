#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_client.h"
#include "esp_sntp.h"
#include "cJSON.h"
#include "app_config.h"
#include "storage/storage.h"
#include "driver/driver_as7341.h"
#include "driver/driver_scd41.h"
#include "service/notify.h"
#include "ai_service.h"
#include "network/wifi_sta.h"

static const char *TAG = "AI_SVC";

static char *extractAiContent(const char *jsonStr)
{
    cJSON *root = cJSON_Parse(jsonStr);
    if (!root) {
        const char *ep = cJSON_GetErrorPtr();
        ESP_LOGE(TAG, "JSON解析失败, 错误位置: %s", ep ? ep : "未知");
        return NULL;
    }

    cJSON *choices = cJSON_GetObjectItem(root, "choices");
    if (!cJSON_IsArray(choices) || cJSON_GetArraySize(choices) == 0) {
        ESP_LOGE(TAG, "AI响应中无choices数组");
        cJSON_Delete(root);
        return NULL;
    }

    cJSON *first = cJSON_GetArrayItem(choices, 0);
    cJSON *message = cJSON_GetObjectItem(first, "message");
    if (!message) {
        ESP_LOGE(TAG, "AI响应中无message字段");
        cJSON_Delete(root);
        return NULL;
    }

    cJSON *content = cJSON_GetObjectItem(message, "content");
    if (!cJSON_IsString(content)) {
        ESP_LOGE(TAG, "AI响应中无content字段");
        cJSON_Delete(root);
        return NULL;
    }

    char *result = strdup(content->valuestring);
    cJSON_Delete(root);
    return result;
}

static esp_err_t httpEventHandler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGE(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
        default:
            break;
    }
    return ESP_OK;
}

static void aiHttpPostRequest(void)
{
    device_config_t config;
    memset(&config, 0, sizeof(config));
    esp_err_t ret = storageLoad(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "配置加载失败");
        return;
    }

    char *outputBuffer = calloc(1, HTTP_OUTPUT_BUFFER * 2);
    if (!outputBuffer) {
        ESP_LOGE(TAG, "内存分配失败");
        return;
    }

    ret = as7341Init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "传感器初始化失败");
        notifyServerchan("错误", "植物光谱传感器初始化失败", config.serverchanKey);
        free(outputBuffer);
        return;
    }

    vTaskDelay(pdMS_TO_TICKS(500));

    as7341_channels_spectral_data_t sensorData;
    ret = as7341ReadData(&sensorData);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "传感器读取失败");
        notifyServerchan("错误", "无法读取植物光谱数据", config.serverchanKey);
        as7341Deinit();
        free(outputBuffer);
        return;
    }

    char *sensorJson = as7341DataToJson(&sensorData);
    if (!sensorJson) {
        ESP_LOGE(TAG, "JSON转换失败");
        notifyServerchan("错误", "传感器数据处理失败", config.serverchanKey);
        as7341Deinit();
        free(outputBuffer);
        return;
    }

    ESP_LOGI(TAG, "传感器JSON: %s", sensorJson);

    char aiPrompt[1024];
    int promptLen = snprintf(aiPrompt, sizeof(aiPrompt),
        "光谱传感器数据：\n%s\n", sensorJson);

    scd41_data_t scdData;
    esp_err_t scdRet = scd41ReadData(&scdData);
    if (scdRet == ESP_OK && scdData.data_valid) {
        promptLen += snprintf(aiPrompt + promptLen, sizeof(aiPrompt) - promptLen,
            "\n环境传感器数据：\nCO2: %uppm\n温度: %.1f°C\n湿度: %.1f%%RH\n",
            scdData.co2_ppm, scdData.temperature_c, scdData.humidity_rh);
    } else {
        ESP_LOGW(TAG, "SCD41数据读取失败，AI分析仅使用光谱数据");
        promptLen += snprintf(aiPrompt + promptLen, sizeof(aiPrompt) - promptLen,
            "\n环境传感器数据：不可用\n");
    }

    snprintf(aiPrompt + promptLen, sizeof(aiPrompt) - promptLen,
        "\n请根据以上全部传感器数据分析植物状态(包括健康度、营养状况、光照适应性、环境适宜度等),并给出照料建议");

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "model", AI_MODEL_NAME);

    cJSON *messages = cJSON_CreateArray();
    cJSON *msg = cJSON_CreateObject();
    cJSON_AddStringToObject(msg, "role", "user");
    cJSON_AddStringToObject(msg, "content", aiPrompt);
    cJSON_AddItemToArray(messages, msg);
    cJSON_AddItemToObject(root, "messages", messages);

    cJSON_AddNumberToObject(root, "max_tokens", AI_MAX_TOKENS);
    cJSON_AddNumberToObject(root, "temperature", AI_TEMPERATURE);
    cJSON_AddNumberToObject(root, "top_p", AI_TOP_P);
    cJSON_AddNumberToObject(root, "n", 1);

    char *jsonData = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (!jsonData) {
        ESP_LOGE(TAG, "请求JSON构造失败");
        notifyServerchan("错误", "AI请求数据构造失败", config.serverchanKey);
        free(sensorJson);
        as7341Deinit();
        free(outputBuffer);
        return;
    }

    ESP_LOGI(TAG, "请求JSON长度: %d, 内容前128字节: %.128s", strlen(jsonData), jsonData);

    esp_http_client_config_t httpConfig = {
        .url = AI_API_ENDPOINT,
        .event_handler = httpEventHandler,
        .timeout_ms = 300000,
        .disable_auto_redirect = false,
        .keep_alive_enable = false,
        .skip_cert_common_name_check = true,
        .cert_pem = NULL
    };
    esp_http_client_handle_t client = esp_http_client_init(&httpConfig);
    if (client == NULL) {
        ESP_LOGE(TAG, "HTTP客户端初始化失败");
        goto cleanup;
    }

    char authHeader[256];
    snprintf(authHeader, sizeof(authHeader), "Bearer %s", config.siliconflowKey);

    ESP_LOGI(TAG, "开始连接: %s", AI_API_ENDPOINT);
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Authorization", authHeader);

    esp_err_t err = esp_http_client_open(client, strlen(jsonData));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "AI接口连接失败: %s (0x%x)", esp_err_to_name(err), err);
        goto cleanup;
    }
    ESP_LOGI(TAG, "TCP连接成功");

    int wlen = esp_http_client_write(client, jsonData, strlen(jsonData));
    ESP_LOGI(TAG, "写入字节数: %d / %d", wlen, (int)strlen(jsonData));
    if (wlen < 0) {
        ESP_LOGE(TAG, "数据写入失败");
        goto cleanup;
    }

    int fetchRet = esp_http_client_fetch_headers(client);
    int statusCode = esp_http_client_get_status_code(client);
    ESP_LOGI(TAG, "fetch_headers返回: %d, HTTP状态码: %d, 内容长度: %lld",
             fetchRet, statusCode, (long long)esp_http_client_get_content_length(client));

    int totalRead = 0;
    if (fetchRet >= 0) {
        int readLen;
        do {
            readLen = esp_http_client_read(client,
                outputBuffer + totalRead,
                HTTP_OUTPUT_BUFFER * 2 - totalRead - 1
            );
            if (readLen > 0) {
                totalRead += readLen;
                ESP_LOGI(TAG, "已读取: %d 字节", totalRead);
            }
        } while (readLen > 0);
        outputBuffer[totalRead] = '\0';
    }

    ESP_LOGI(TAG, "总计读取: %d 字节, HTTP状态码: %d", totalRead, statusCode);
    if (totalRead > 0) {
        ESP_LOGI(TAG, "响应前256字节: %.256s", outputBuffer);
    } else {
        ESP_LOGE(TAG, "响应为空");
    }

    char *aiContent = extractAiContent(outputBuffer);
    if (aiContent) {
        ESP_LOGI(TAG, "AI分析结果: %s", aiContent);
        esp_err_t scErr = notifyServerchan("植物分析报告", aiContent, config.serverchanKey);
        if (scErr == ESP_OK) {
            ESP_LOGI(TAG, "已推送到Server酱");
        } else {
            ESP_LOGE(TAG, "Server酱推送失败");
        }
        free(aiContent);
    } else {
        ESP_LOGE(TAG, "AI内容提取失败,发送原始响应到Server酱");
        notifyServerchan("植物分析报告(原始)", outputBuffer, config.serverchanKey);
    }

cleanup:
    free(jsonData);
    free(sensorJson);
    as7341Deinit();
    free(outputBuffer);
    if (client) {
        esp_http_client_cleanup(client);
    }
}

static void aiHttpTask(void *pvParameters)
{
    aiHttpPostRequest();
    ESP_LOGI(TAG, "HTTP任务完成");
    vTaskDelete(NULL);
}

void aiServiceRun(void)
{
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(1, "time.nist.gov");
    esp_sntp_init();

    int retry = 0;
    const int retryCount = 10;
    while (esp_sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retryCount) {
        ESP_LOGI(TAG, "等待时间同步... (%d/%d)", retry, retryCount);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
    xTaskCreate(&aiHttpTask, "ai_http", 24576, NULL, 5, NULL);
}
