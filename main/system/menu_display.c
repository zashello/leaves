#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "app_config.h"
#include "driver/driver_ssd1309.h"
#include "menu_display.h"

static const char *TAG = "MENU_DISP";

void menuDisplayShowMenu(const menu_context_t *ctx)
{
    if (ctx == NULL || ctx->current == NULL) return;

    ssd1309Clear();

    ssd1309SetCursor(0, 0);
    ssd1309Print("LEAVES MONITOR");

    int startItem = 0;
    if (ctx->selected >= MENU_ITEMS_PER_SCREEN) {
        startItem = ctx->selected - MENU_ITEMS_PER_SCREEN + 1;
    }

    for (int i = 0; i < MENU_ITEMS_PER_SCREEN && (startItem + i) < ctx->itemCount; i++) {
        int idx = startItem + i;
        int y = (i + 1) * MENU_LINE_HEIGHT;

        ssd1309SetCursor(0, y);
        if (idx == ctx->selected) {
            ssd1309Print(">");
        } else {
            ssd1309Print(" ");
        }

        ssd1309Print(ctx->current[idx].label);

        if (ctx->current[idx].type == MENU_ITEM_TYPE_SUBMENU) {
            ssd1309Print(" >");
        }
    }

    ssd1309Display();
}

void menuDisplayShowWaiting(const char *message)
{
    if (message == NULL) return;

    ssd1309Clear();

    ssd1309SetCursor(0, 0);
    ssd1309Print(message);

    ssd1309SetCursor(0, 8);
    ssd1309Print("PLEASE WAIT...");

    ssd1309Display();
}

void menuDisplayShowScd41Data(const scd41_data_t *data)
{
    if (data == NULL) return;

    char buf[64];

    ssd1309Clear();

    ssd1309SetCursor(0, 0);
    ssd1309Print("SCD41 DATA");

    ssd1309SetCursor(0, 8);
    snprintf(buf, sizeof(buf), "CO2:%u PPM", data->co2_ppm);
    ssd1309Print(buf);

    ssd1309SetCursor(0, 16);
    snprintf(buf, sizeof(buf), "T:%.1fC H:%.1f%%", data->temperature_c, data->humidity_rh);
    ssd1309Print(buf);

    ssd1309SetCursor(0, 24);
    ssd1309Print("PRESS BACK");

    ssd1309Display();
}

void menuDisplayShowPlantAnalysis(const ei_inference_result_t *result)
{
    if (result == NULL) return;

    char buf[64];
    char upLabel[9] = {0};

    ssd1309Clear();

    ssd1309SetCursor(0, 0);
    ssd1309Print("PLANT ANALYSIS");

    if (result->bestLabel != NULL) {
        int len = 0;
        for (int i = 0; i < 8 && result->bestLabel[i] != '\0'; i++) {
            upLabel[i] = (result->bestLabel[i] >= 'a' && result->bestLabel[i] <= 'z')
                         ? result->bestLabel[i] - 32 : result->bestLabel[i];
            if (upLabel[i] == '_') upLabel[i] = ' ';
            len = i + 1;
        }
        upLabel[len] = '\0';
    }

    ssd1309SetCursor(0, 8);
    snprintf(buf, sizeof(buf), "BEST:%.8s %.1f%%", upLabel, result->results[0].value * 100.0f);
    ssd1309Print(buf);

    ssd1309SetCursor(0, 16);
    snprintf(buf, sizeof(buf), "2ND: %.1f%%", result->results[1].value * 100.0f);
    ssd1309Print(buf);

    ssd1309SetCursor(0, 24);
    ssd1309Print("PRESS BACK");

    ssd1309Display();
}

void menuDisplayShowMessage(const char *message)
{
    if (message == NULL) return;

    ssd1309Clear();

    ssd1309SetCursor(0, 0);
    ssd1309Print(message);

    ssd1309Display();
}

void menuDisplayShowConfirm(const char *message)
{
    if (message == NULL) return;

    ssd1309Clear();

    ssd1309SetCursor(0, 0);
    ssd1309Print(message);

    ssd1309SetCursor(0, 8);
    ssd1309Print("OK=CONFIRM");

    ssd1309SetCursor(0, 16);
    ssd1309Print("BACK=CANCEL");

    ssd1309Display();
}

void menuDisplayShowWifiAp(const char *ssid, const char *ip)
{
    ssd1309Clear();

    ssd1309SetCursor(0, 0);
    ssd1309Print("PROVISION MODE");

    ssd1309SetCursor(0, 8);
    ssd1309Print("SSID:");
    if (ssid) ssd1309Print(ssid);

    ssd1309SetCursor(0, 16);
    ssd1309Print("IP:");
    if (ip) ssd1309Print(ip);

    ssd1309SetCursor(0, 24);
    ssd1309Print("PRESS BACK");

    ssd1309Display();
}

void menuDisplayShowError(const char *message)
{
    if (message == NULL) return;

    ssd1309Clear();

    ssd1309SetCursor(0, 0);
    ssd1309Print("ERROR!");

    ssd1309SetCursor(0, 8);
    ssd1309Print(message);

    ssd1309SetCursor(0, 24);
    ssd1309Print("PRESS BACK");

    ssd1309Display();
}

void menuDisplayShowSuccess(const char *message)
{
    ssd1309Clear();

    ssd1309SetCursor(0, 0);
    ssd1309Print("SUCCESS");

    if (message) {
        ssd1309SetCursor(0, 8);
        ssd1309Print(message);
    }

    ssd1309Display();
}

void menuDisplayShowLogEntry(const log_entry_t *entry, int index, int total)
{
    if (entry == NULL) return;

    char buf[64];

    ssd1309Clear();

    ssd1309SetCursor(0, 0);
    snprintf(buf, sizeof(buf), "LOG %d/%d", index + 1, total);
    ssd1309Print(buf);

    ssd1309SetCursor(0, 8);
    switch (entry->level) {
        case LOG_LEVEL_INFO:     ssd1309Print("[INFO]");   break;
        case LOG_LEVEL_WARNING:  ssd1309Print("[WARN]");   break;
        case LOG_LEVEL_ERROR:    ssd1309Print("[ERR]");    break;
        case LOG_LEVEL_CRITICAL: ssd1309Print("[CRIT]");   break;
    }

    ssd1309SetCursor(0, 16);
    ssd1309Print(entry->message);

    ssd1309SetCursor(0, 24);
    ssd1309Print("PRESS BACK");

    ssd1309Display();
}

void menuDisplayShowAbout(void)
{
    ssd1309Clear();

    ssd1309SetCursor(0, 0);
    ssd1309Print("LEAVES MONITOR");

    ssd1309SetCursor(0, 8);
    ssd1309Print("VER: 1.0");

    ssd1309SetCursor(0, 16);
    ssd1309Print("ESP32-S3");

    ssd1309SetCursor(0, 24);
    ssd1309Print("PRESS BACK");

    ssd1309Display();
}

void menuDisplayShowBrightness(uint8_t brightness)
{
    char buf[32];

    ssd1309Clear();

    ssd1309SetCursor(0, 0);
    ssd1309Print("LED BRIGHTNESS");

    ssd1309SetCursor(0, 8);
    snprintf(buf, sizeof(buf), "VAL: %d%%", brightness);
    ssd1309Print(buf);

    ssd1309SetCursor(0, 16);
    ssd1309Print("OK=SAVE");

    ssd1309SetCursor(0, 24);
    ssd1309Print("BACK=CANCEL");

    ssd1309Display();
}
