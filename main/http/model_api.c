#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_client.h"
#include "esp_sntp.h"
#include "cJSON.h"
#include "model_api.h"
#include "hardware/sensor.h"
#include "config/nvs_config.h"

#define MAX_HTTP_OUTPUT_BUFFER 8192
static const char *TAG = "HTTP_AI_CLIENT";
#define API_ENDPOINT "http://api.siliconflow.cn/v1/chat/completions"

static esp_err_t httpEventHandler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGE(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADERS_SENT:
            ESP_LOGI(TAG, "HTTP_EVENT_HEADERS_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d, data=%.64s", evt->data_len, (char *)evt->data);
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
        default:
            break;
    }
    return ESP_OK;
}

esp_err_t sendToServerchan(const char *title, const char *desp, const char *serverchanKey)
{
    if (serverchanKey == NULL || strlen(serverchanKey) == 0) {
        ESP_LOGE(TAG, "Server酱Key为空");
        return ESP_ERR_INVALID_ARG;
    }

    char url[256];
    snprintf(url, sizeof(url), "http://sctapi.ftqq.com/%s.send", serverchanKey);

    int despLen = strlen(desp);
    int titleLen = strlen(title);
    int postDataSize = titleLen + despLen + 16;
    char *postData = malloc(postDataSize);
    if (!postData) {
        ESP_LOGE(TAG, "Server酱内存分配失败");
        return ESP_FAIL;
    }
    int written = snprintf(postData, postDataSize, "title=%s&desp=%s", title, desp);
    if (written < 0 || written >= postDataSize) {
        ESP_LOGE(TAG, "Server酱 POST 数据过长(%d字节)", written);
        free(postData);
        return ESP_FAIL;
    }

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 15000,
        .cert_pem = NULL,
        .skip_cert_common_name_check = true,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_http_client_set_header(client, "Content-Type", "application/x-www-form-urlencoded");

    esp_err_t err = esp_http_client_open(client, strlen(postData));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Server酱连接失败: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        free(postData);
        return err;
    }

    int wlen = esp_http_client_write(client, postData, strlen(postData));
    if (wlen < 0) {
        ESP_LOGE(TAG, "Server酱数据写入失败");
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        free(postData);
        return ESP_FAIL;
    }

    int contentLength = esp_http_client_fetch_headers(client);
    ESP_LOGI(TAG, "Server酱响应状态码 = %d, 内容长度 = %d",
             esp_http_client_get_status_code(client), contentLength);

    char respBuf[512] = {0};
    esp_http_client_read_response(client, respBuf, sizeof(respBuf) - 1);
    ESP_LOGI(TAG, "Server酱响应: %s", respBuf);

    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    free(postData);
    return ESP_OK;
}

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

static void httpPostRequest(void)
{
    device_config_t config;
    memset(&config, 0, sizeof(config));
    esp_err_t ret = configLoad(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "配置加载失败");
        return;
    }

    char *outputBuffer = calloc(1, MAX_HTTP_OUTPUT_BUFFER * 2);
    if (!outputBuffer) {
        ESP_LOGE(TAG, "内存分配失败");
        return;
    }

    ret = sensor_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "传感器初始化失败");
        sendToServerchan("错误", "植物光谱传感器初始化失败", config.serverchanKey);
        free(outputBuffer);
        return;
    }

    vTaskDelay(pdMS_TO_TICKS(500));

    as7341_channels_spectral_data_t sensorData;
    ret = sensor_read_data(&sensorData);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "传感器读取失败");
        sendToServerchan("错误", "无法读取植物光谱数据", config.serverchanKey);
        sensor_deinit();
        free(outputBuffer);
        return;
    }

    char *sensorJson = sensor_data_to_json(&sensorData);
    if (!sensorJson) {
        ESP_LOGE(TAG, "JSON转换失败");
        sendToServerchan("错误", "传感器数据处理失败", config.serverchanKey);
        sensor_deinit();
        free(outputBuffer);
        return;
    }

    ESP_LOGI(TAG, "传感器JSON: %s", sensorJson);

    char aiPrompt[1024];
    snprintf(aiPrompt, sizeof(aiPrompt),
        "光谱传感器数据：\n%s\n\n请根据以上光谱数据分析植物状态(包括健康度、营养状况、光照适应性等),并给出照料建议",
        sensorJson);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "model", "Qwen/Qwen3-VL-30B-A3B-Instruct");

    cJSON *messages = cJSON_CreateArray();
    cJSON *msg = cJSON_CreateObject();
    cJSON_AddStringToObject(msg, "role", "user");
    cJSON_AddStringToObject(msg, "content", aiPrompt);
    cJSON_AddItemToArray(messages, msg);
    cJSON_AddItemToObject(root, "messages", messages);

    cJSON_AddNumberToObject(root, "max_tokens", 10000);
    cJSON_AddNumberToObject(root, "temperature", 0.7);
    cJSON_AddNumberToObject(root, "top_p", 0.7);
    cJSON_AddNumberToObject(root, "n", 1);

    char *jsonData = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (!jsonData) {
        ESP_LOGE(TAG, "请求JSON构造失败");
        sendToServerchan("错误", "AI请求数据构造失败", config.serverchanKey);
        free(sensorJson);
        sensor_deinit();
        free(outputBuffer);
        return;
    }

    ESP_LOGI(TAG, "请求JSON长度: %d, 内容前128字节: %.128s", strlen(jsonData), jsonData);

    esp_http_client_config_t httpConfig = {
        .url = API_ENDPOINT,
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

    ESP_LOGI(TAG, "开始连接: %s", API_ENDPOINT);
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
                MAX_HTTP_OUTPUT_BUFFER * 2 - totalRead - 1
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
        esp_err_t scErr = sendToServerchan("植物分析报告", aiContent, config.serverchanKey);
        if (scErr == ESP_OK) {
            ESP_LOGI(TAG, "已推送到Server酱");
        } else {
            ESP_LOGE(TAG, "Server酱推送失败");
        }
        free(aiContent);
    } else {
        ESP_LOGE(TAG, "AI内容提取失败,发送原始响应到Server酱");
        sendToServerchan("植物分析报告(原始)", outputBuffer, config.serverchanKey);
    }

cleanup:
    free(jsonData);
    free(sensorJson);
    sensor_deinit();
    free(outputBuffer);
    if (client) {
        esp_http_client_cleanup(client);
    }
}

static void httpTask(void *pvParameters)
{
    httpPostRequest();
    ESP_LOGI(TAG, "HTTP任务完成");
    vTaskDelete(NULL);
}

void aiTest(void)
{
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_setservername(1, "time.nist.gov");
    sntp_init();

    int retry = 0;
    const int retryCount = 10;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retryCount) {
        ESP_LOGI(TAG, "等待时间同步... (%d/%d)", retry, retryCount);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
    xTaskCreate(&httpTask, "http_task", 24576, NULL, 5, NULL);
}
