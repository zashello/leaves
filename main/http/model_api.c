#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"
#include "esp_http_client.h"
#include "esp_sntp.h"


#define MAX_HTTP_OUTPUT_BUFFER 2048
static const char *TAG = "HTTP_AI_CLIENT";
#define API_ENDPOINT "http://api.siliconflow.cn/v1/chat/completions"

// 添加事件处理函数
static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
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
            ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
        default:
            break;
    }
    return ESP_OK;
}

static void http_post_request(void)
{
    // char output_buffer[MAX_HTTP_OUTPUT_BUFFER] = {0};
       // 增大缓冲区防止数据截断
    char *output_buffer = calloc(1, MAX_HTTP_OUTPUT_BUFFER * 2); // 扩容至4096
    if (!output_buffer) {
        ESP_LOGE(TAG, "内存分配失败");
        return;
    } 
    esp_http_client_config_t config = {
        .url = API_ENDPOINT,
        .event_handler = http_event_handler,
        .timeout_ms = 120000,                // 超时延长至120秒
        .disable_auto_redirect = false,      // 允许重定向
        .keep_alive_enable = true,           // 启用长连接
        .skip_cert_common_name_check = true,
        .cert_pem = NULL
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    const char *json_data = 
    "{"
        "\"model\": \"deepseek-ai/DeepSeek-R1\","//选择模型
        "\"messages\": ["
            "{"
                "\"role\": \"user\","
                "\"content\": \"写一篇散文\""//填入问题内容
            "}"
        "],"
        "\"max_tokens\": 512,"
        "\"temperature\": 0.7,"
        "\"top_p\": 0.7,"
        "\"top_k\": 50,"
        "\"frequency_penalty\": 0.5,"
        "\"n\": 1"
    "}";

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Authorization", "sk-vocmaeyaahithsxiffslunznftimbiwglcftvnubyuhhpemp");//key_API

     // 新增分段接收逻辑
    int total_read = 0;
    esp_err_t err = esp_http_client_open(client, strlen(json_data));
    if (err == ESP_OK) {
        // 分段写入数据（应对大请求体）
        const char *ptr = json_data;
        int remaining = strlen(json_data);
        while (remaining > 0) {
            int written = esp_http_client_write(client, ptr, remaining);
            if (written <= 0) break;
            ptr += written;
            remaining -= written;
        }
    }

    int wlen = esp_http_client_write(client, json_data, strlen(json_data));
    if (wlen < 0) {
        ESP_LOGE(TAG, "数据写入失败");
        goto cleanup;
    }

 // 新增分段读取逻辑
    if (esp_http_client_fetch_headers(client) >= 0) {
        int read_len;
        do {
            read_len = esp_http_client_read(client, 
                output_buffer + total_read, 
                MAX_HTTP_OUTPUT_BUFFER * 2 - total_read - 1
            );
            if (read_len > 0) {
                total_read += read_len;
            }
        } while (read_len > 0);
        
        output_buffer[total_read] = '\0'; // 确保字符串终止
    }

    int data_read = esp_http_client_read_response(client, output_buffer, MAX_HTTP_OUTPUT_BUFFER);
    if (data_read >= 0) {
        ESP_LOGI(TAG, "HTTP状态码 = %d, 内容长度 = %"PRIu64,
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
        ESP_LOGI(TAG, "响应内容: %s", output_buffer);
    } else {
        ESP_LOGE(TAG, "读取响应失败");
    }

cleanup:
    esp_http_client_cleanup(client);
}

static void http_task(void *pvParameters)
{
    http_post_request();
    ESP_LOGI(TAG, "HTTP示例完成");
    vTaskDelete(NULL);
}

void AI_test(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect());

    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");  // 第1个NTP服务器
    sntp_setservername(1, "time.nist.gov"); // 第2个NTP服务器（可选）
    sntp_init();
    // 在 sntp_init() 后添加时间同步等待
    int retry = 0;
    const int retry_count = 10;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
        printf("Waiting for system time to sync... (%d/%d)\n", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
    xTaskCreate(&http_task, "http_task", 18192, NULL, 5, NULL);
}
