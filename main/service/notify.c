#include <string.h>
#include "esp_log.h"
#include "esp_http_client.h"
#include "app_config.h"
#include "notify.h"

static const char *TAG = "NOTIFY";

esp_err_t notifyServerchan(const char *title, const char *desp, const char *serverchanKey)
{
    if (serverchanKey == NULL || strlen(serverchanKey) == 0) {
        ESP_LOGE(TAG, "Server酱Key为空");
        return ESP_ERR_INVALID_ARG;
    }

    char url[256];
    snprintf(url, sizeof(url), "%s%s.send", SERVERCHAN_URL_PREFIX, serverchanKey);

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
