#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_client.h"
#include "esp_sntp.h"
#include "cJSON.h"
#include "model_api.h"
#include "sensor/sensor.h"
#include "wifi/wifi_connect.h"

#define MAX_HTTP_OUTPUT_BUFFER 8192
static const char *TAG = "HTTP_AI_CLIENT";
#define API_ENDPOINT "http://api.siliconflow.cn/v1/chat/completions"

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
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
            ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d, data=%.64s", evt->data_len, (char*)evt->data);
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

esp_err_t send_to_serverchan(const char *title, const char *desp)
{
    char url[256];
    snprintf(url, sizeof(url), "%s%s.send", SERVER_CHAN_API_URL, SERVER_CHAN_SENDKEY);

    char *truncated_desp = NULL;
    const char *send_desp = desp;
    if (strlen(desp) > 900) {
        truncated_desp = malloc(960);
        if (truncated_desp) {
            snprintf(truncated_desp, 960, "%.900s\n\n...(内容过长已截断)", desp);
            send_desp = truncated_desp;
        }
    }

    char post_data[2048];
    int written = snprintf(post_data, sizeof(post_data), "title=%s&desp=%s", title, send_desp);
    if (truncated_desp) free(truncated_desp);
    if (written < 0 || written >= sizeof(post_data)) {
        ESP_LOGE(TAG, "Server酱 POST 数据过长");
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

    esp_err_t err = esp_http_client_open(client, strlen(post_data));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Server酱连接失败: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }

    int wlen = esp_http_client_write(client, post_data, strlen(post_data));
    if (wlen < 0) {
        ESP_LOGE(TAG, "Server酱数据写入失败");
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    int content_length = esp_http_client_fetch_headers(client);
    ESP_LOGI(TAG, "Server酱响应状态码 = %d, 内容长度 = %d",
             esp_http_client_get_status_code(client), content_length);

    char resp_buf[512] = {0};
    esp_http_client_read_response(client, resp_buf, sizeof(resp_buf) - 1);
    ESP_LOGI(TAG, "Server酱响应: %s", resp_buf);

    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return ESP_OK;
}

static char *extract_ai_content(const char *json_str)
{
    cJSON *root = cJSON_Parse(json_str);
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

static void http_post_request(void)
{
    char *output_buffer = calloc(1, MAX_HTTP_OUTPUT_BUFFER * 2);
    if (!output_buffer) {
        ESP_LOGE(TAG, "内存分配失败");
        return;
    }

    esp_err_t ret = sensor_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "传感器初始化失败");
        send_to_serverchan("错误", "植物光谱传感器初始化失败");
        free(output_buffer);
        return;
    }

    vTaskDelay(pdMS_TO_TICKS(500));

    as7341_channels_spectral_data_t sensor_data;
    ret = sensor_read_data(&sensor_data);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "传感器读取失败");
        send_to_serverchan("错误", "无法读取植物光谱数据");
        sensor_deinit();
        free(output_buffer);
        return;
    }

    char *sensor_json = sensor_data_to_json(&sensor_data);
    if (!sensor_json) {
        ESP_LOGE(TAG, "JSON转换失败");
        send_to_serverchan("错误", "传感器数据处理失败");
        sensor_deinit();
        free(output_buffer);
        return;
    }

    ESP_LOGI(TAG, "传感器JSON: %s", sensor_json);

    char ai_prompt[1024];
    snprintf(ai_prompt, sizeof(ai_prompt),
        "光谱传感器数据：\n%s\n\n请根据以上光谱数据分析植物状态(包括健康度、营养状况、光照适应性等),并给出照料建议",
        sensor_json);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "model", "Qwen/Qwen2.5-7B-Instruct");

    cJSON *messages = cJSON_CreateArray();
    cJSON *msg = cJSON_CreateObject();
    cJSON_AddStringToObject(msg, "role", "user");
    cJSON_AddStringToObject(msg, "content", ai_prompt);
    cJSON_AddItemToArray(messages, msg);
    cJSON_AddItemToObject(root, "messages", messages);

    cJSON_AddNumberToObject(root, "max_tokens", 512);
    cJSON_AddNumberToObject(root, "temperature", 0.7);
    cJSON_AddNumberToObject(root, "top_p", 0.7);
    cJSON_AddNumberToObject(root, "top_k", 50);
    cJSON_AddNumberToObject(root, "frequency_penalty", 0.5);
    cJSON_AddNumberToObject(root, "n", 1);

    char *json_data = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (!json_data) {
        ESP_LOGE(TAG, "请求JSON构造失败");
        send_to_serverchan("错误", "AI请求数据构造失败");
        free(sensor_json);
        sensor_deinit();
        free(output_buffer);
        return;
    }

    ESP_LOGI(TAG, "请求JSON长度: %d, 内容前128字节: %.128s", strlen(json_data), json_data);

    esp_http_client_config_t config = {
        .url = API_ENDPOINT,
        .event_handler = http_event_handler,
        .timeout_ms = 120000,
        .disable_auto_redirect = false,
        .keep_alive_enable = false,
        .skip_cert_common_name_check = true,
        .cert_pem = NULL
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "HTTP客户端初始化失败");
        goto cleanup;
    }

    ESP_LOGI(TAG, "开始连接: %s", API_ENDPOINT);
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Authorization", "Bearer sk-vocmaeyaahithsxiffslunznftimbiwglcftvnubyuhhpemp");

    esp_err_t err = esp_http_client_open(client, strlen(json_data));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "AI接口连接失败: %s (0x%x)", esp_err_to_name(err), err);
        goto cleanup;
    }
    ESP_LOGI(TAG, "TCP连接成功");

    int wlen = esp_http_client_write(client, json_data, strlen(json_data));
    ESP_LOGI(TAG, "写入字节数: %d / %d", wlen, (int)strlen(json_data));
    if (wlen < 0) {
        ESP_LOGE(TAG, "数据写入失败");
        goto cleanup;
    }

    int fetch_ret = esp_http_client_fetch_headers(client);
    int status_code = esp_http_client_get_status_code(client);
    ESP_LOGI(TAG, "fetch_headers返回: %d, HTTP状态码: %d, 内容长度: %lld",
             fetch_ret, status_code, (long long)esp_http_client_get_content_length(client));

    int total_read = 0;
    if (fetch_ret >= 0) {
        int read_len;
        do {
            read_len = esp_http_client_read(client,
                output_buffer + total_read,
                MAX_HTTP_OUTPUT_BUFFER * 2 - total_read - 1
            );
            if (read_len > 0) {
                total_read += read_len;
                ESP_LOGI(TAG, "已读取: %d 字节", total_read);
            }
        } while (read_len > 0);
        output_buffer[total_read] = '\0';
    }

    ESP_LOGI(TAG, "总计读取: %d 字节, HTTP状态码: %d", total_read, status_code);
    if (total_read > 0) {
        ESP_LOGI(TAG, "响应前256字节: %.256s", output_buffer);
    } else {
        ESP_LOGE(TAG, "响应为空");
    }

    char *ai_content = extract_ai_content(output_buffer);
    if (ai_content) {
        ESP_LOGI(TAG, "AI分析结果: %s", ai_content);
        esp_err_t sc_err = send_to_serverchan("植物分析报告", ai_content);
        if (sc_err == ESP_OK) {
            ESP_LOGI(TAG, "已推送到Server酱");
        } else {
            ESP_LOGE(TAG, "Server酱推送失败");
        }
        free(ai_content);
    } else {
        ESP_LOGE(TAG, "AI内容提取失败,发送原始响应到Server酱");
        send_to_serverchan("植物分析报告(原始)", output_buffer);
    }

cleanup:
    free(json_data);
    free(sensor_json);
    sensor_deinit();
    free(output_buffer);
    esp_http_client_cleanup(client);
}

static void http_task(void *pvParameters)
{
    http_post_request();
    ESP_LOGI(TAG, "HTTP任务完成");
    vTaskDelete(NULL);
}

void AI_test(void)
{
    wifi_wait_connected();

    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_setservername(1, "time.nist.gov");
    sntp_init();

    int retry = 0;
    const int retry_count = 10;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
        ESP_LOGI(TAG, "等待时间同步... (%d/%d)", retry, retry_count);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
    xTaskCreate(&http_task, "http_task", 24576, NULL, 5, NULL);
}
