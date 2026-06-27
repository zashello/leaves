#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "app_config.h"
#include "driver/driver_ssd1309.h"
#include "service/display_service.h"
#include "storage/storage.h"

static const char *TAG = "DISPLAY_SVC";
static bool g_initialized = false;

static void convertToUppercaseAndSpaces(char *str)
{
    if (str == NULL) return;

    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] >= 'a' && str[i] <= 'z') {
            str[i] = str[i] - ('a' - 'A');
        } else if (str[i] == '_') {
            str[i] = ' ';
        }
    }
}

esp_err_t displayServiceInit(void)
{
    if (g_initialized) {
        ESP_LOGW(TAG, "显示服务已初始化");
        return ESP_OK;
    }

    ssd1309_config_t config = {
        .width = OLED_WIDTH,
        .height = OLED_HEIGHT,
        .sclPin = OLED_I2C_SOFT_SCL,
        .sdaPin = OLED_I2C_SOFT_SDA,
        .rstPin = OLED_RST_GPIO,
        .address = OLED_I2C_ADDRESS,
        .columnOffset = OLED_COLUMN_OFFSET,
        .pageOffset = OLED_PAGE_OFFSET
    };

    esp_err_t ret = ssd1309Init(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SSD1309初始化失败");
        return ret;
    }

    g_initialized = true;
    ESP_LOGI(TAG, "显示服务初始化成功");
    return ESP_OK;
}

void displayServiceShowInitScreen(void)
{
    if (!g_initialized) {
        ESP_LOGW(TAG, "显示服务未初始化");
        return;
    }

    ESP_LOGI(TAG, "显示初始化画面");

    ssd1309Clear();

    ssd1309SetCursor(37, 0);
    ssd1309Print("LEAVES");

    ssd1309SetCursor(32, 8);
    ssd1309Print("MONITOR");

    ssd1309Display();

    vTaskDelay(pdMS_TO_TICKS(3000));

    ESP_LOGI(TAG, "初始化画面已显示，等待ML结果覆盖");
}

void displayServiceUpdate(const ei_inference_result_t *result)
{
    if (!g_initialized) {
        return;
    }

    if (result == NULL) {
        ESP_LOGW(TAG, "输入参数为空");
        return;
    }

    char label1[9], label2[9], label3[9], label4[9];

    snprintf(label1, sizeof(label1), "%s", result->results[0].label);
    snprintf(label2, sizeof(label2), "%s", result->results[1].label);
    snprintf(label3, sizeof(label3), "%s", result->results[2].label);
    snprintf(label4, sizeof(label4), "%s", result->results[3].label);

    convertToUppercaseAndSpaces(label1);
    convertToUppercaseAndSpaces(label2);
    convertToUppercaseAndSpaces(label3);
    convertToUppercaseAndSpaces(label4);

    ssd1309Clear();

    ssd1309SetCursor(0, 0);
    ssd1309Print(label1);
    ssd1309PrintFloat(result->results[0].value * 100.0f, 1);
    ssd1309Print("%");

    ssd1309SetCursor(0, 8);
    ssd1309Print(label2);
    ssd1309PrintFloat(result->results[1].value * 100.0f, 1);
    ssd1309Print("%");

    ssd1309SetCursor(0, 16);
    ssd1309Print(label3);
    ssd1309PrintFloat(result->results[2].value * 100.0f, 1);
    ssd1309Print("%");

    ssd1309SetCursor(0, 24);
    ssd1309Print(label4);
    ssd1309PrintFloat(result->results[3].value * 100.0f, 1);
    ssd1309Print("%");

    ssd1309Display();

    ESP_LOGI(TAG, "OLED显示已更新");
}

void displayServiceDeinit(void)
{
    if (!g_initialized) return;

    ssd1309Deinit();
    g_initialized = false;

    ESP_LOGI(TAG, "显示服务已释放");
}
