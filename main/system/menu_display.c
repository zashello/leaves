#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "app_config.h"
#include "driver/driver_ssd1306.h"
#include "menu_display.h"

static const char *TAG = "MENU_DISP";

void menuDisplayShowMenu(const menu_context_t *ctx)
{
    if (ctx == NULL || ctx->current == NULL) return;

    ssd1306Clear();

    ssd1306SetCursor(0, 0);
    ssd1306Print("LEAVES MONITOR");

    int startItem = 0;
    if (ctx->selected >= MENU_ITEMS_PER_SCREEN) {
        startItem = ctx->selected - MENU_ITEMS_PER_SCREEN + 1;
    }

    for (int i = 0; i < MENU_ITEMS_PER_SCREEN && (startItem + i) < ctx->itemCount; i++) {
        int idx = startItem + i;
        int y = (i + 1) * MENU_LINE_HEIGHT;

        ssd1306SetCursor(0, y);
        if (idx == ctx->selected) {
            ssd1306Print(">");
        } else {
            ssd1306Print(" ");
        }

        ssd1306Print(ctx->current[idx].label);

        if (ctx->current[idx].type == MENU_ITEM_TYPE_SUBMENU) {
            ssd1306Print(" >");
        }
    }

    ssd1306Display();
}

void menuDisplayShowWaiting(const char *message)
{
    if (message == NULL) return;

    ssd1306Clear();

    ssd1306SetCursor(0, 10);
    ssd1306Print(message);

    ssd1306SetCursor(0, 30);
    ssd1306Print("PLEASE WAIT...");

    ssd1306Display();
}

void menuDisplayShowScd41Data(const scd41_data_t *data)
{
    if (data == NULL) return;

    char buf[64];

    ssd1306Clear();

    ssd1306SetCursor(0, 0);
    ssd1306Print("SCD41 DATA");

    ssd1306SetCursor(0, 12);
    snprintf(buf, sizeof(buf), "CO2 LEVEL : %u PPM", data->co2_ppm);
    ssd1306Print(buf);

    ssd1306SetCursor(0, 22);
    snprintf(buf, sizeof(buf), "TEMPERATURE: %.1f C", data->temperature_c);
    ssd1306Print(buf);

    ssd1306SetCursor(0, 32);
    snprintf(buf, sizeof(buf), "HUMIDITY   : %.1f RH", data->humidity_rh);
    ssd1306Print(buf);

    ssd1306SetCursor(0, 50);
    ssd1306Print("PRESS BACK");

    ssd1306Display();
}

void menuDisplayShowPlantAnalysis(const ei_inference_result_t *result)
{
    if (result == NULL) return;

    char buf[64];

    ssd1306Clear();

    ssd1306SetCursor(0, 0);
    ssd1306Print("PLANT ANALYSIS");

    ssd1306SetCursor(0, 11);
    snprintf(buf, sizeof(buf), "DISEASED   : %.1f%%", result->results[0].value * 100.0f);
    ssd1306Print(buf);

    ssd1306SetCursor(0, 21);
    snprintf(buf, sizeof(buf), "HEALTHY    : %.1f%%", result->results[1].value * 100.0f);
    ssd1306Print(buf);

    ssd1306SetCursor(0, 31);
    snprintf(buf, sizeof(buf), "LOW WATER  : %.1f%%", result->results[2].value * 100.0f);
    ssd1306Print(buf);

    if (result->bestLabel != NULL) {
        ssd1306SetCursor(0, 43);
        
        char upperLabel[32] = {0};
        int len = 0;
        for (int i = 0; i < 31 && result->bestLabel[i] != '\0'; i++) {
            if (result->bestLabel[i] >= 'a' && result->bestLabel[i] <= 'z') {
                upperLabel[i] = result->bestLabel[i] - 32;
            } else if (result->bestLabel[i] == '_') {
                upperLabel[i] = ' ';
            } else {
                upperLabel[i] = result->bestLabel[i];
            }
            len = i + 1;
        }
        upperLabel[len] = '\0';
        
        snprintf(buf, sizeof(buf), "BEST: %.26s", upperLabel);
        ssd1306Print(buf);
    }

    ssd1306SetCursor(0, 54);
    ssd1306Print("PRESS BACK");

    ssd1306Display();
}

void menuDisplayShowMessage(const char *message)
{
    if (message == NULL) return;

    ssd1306Clear();

    ssd1306SetCursor(0, 25);
    ssd1306Print(message);

    ssd1306Display();
}

void menuDisplayShowConfirm(const char *message)
{
    if (message == NULL) return;

    ssd1306Clear();

    ssd1306SetCursor(0, 10);
    ssd1306Print(message);

    ssd1306SetCursor(0, 35);
    ssd1306Print("OK=CONFIRM");

    ssd1306SetCursor(0, 47);
    ssd1306Print("BACK=CANCEL");

    ssd1306Display();
}

void menuDisplayShowWifiAp(const char *ssid, const char *ip)
{
    ssd1306Clear();

    ssd1306SetCursor(0, 0);
    ssd1306Print("PROVISION MODE");

    ssd1306SetCursor(0, 15);
    ssd1306Print("SSID:");
    if (ssid) ssd1306Print(ssid);

    ssd1306SetCursor(0, 27);
    ssd1306Print("IP:");
    if (ip) ssd1306Print(ip);

    ssd1306SetCursor(0, 45);
    ssd1306Print("PRESS BACK");

    ssd1306Display();
}

void menuDisplayShowError(const char *message)
{
    if (message == NULL) return;

    ssd1306Clear();

    ssd1306SetCursor(0, 10);
    ssd1306Print("ERROR!");

    ssd1306SetCursor(0, 25);
    ssd1306Print(message);

    ssd1306SetCursor(0, 50);
    ssd1306Print("PRESS BACK");

    ssd1306Display();
}

void menuDisplayShowSuccess(const char *message)
{
    ssd1306Clear();

    ssd1306SetCursor(0, 20);
    ssd1306Print("SUCCESS");

    if (message) {
        ssd1306SetCursor(0, 35);
        ssd1306Print(message);
    }

    ssd1306Display();
}

void menuDisplayShowLogEntry(const log_entry_t *entry, int index, int total)
{
    if (entry == NULL) return;

    char buf[64];

    ssd1306Clear();

    ssd1306SetCursor(0, 0);
    snprintf(buf, sizeof(buf), "LOG %d/%d", index + 1, total);
    ssd1306Print(buf);

    ssd1306SetCursor(0, 12);
    switch (entry->level) {
        case LOG_LEVEL_INFO:     ssd1306Print("[INFO]");   break;
        case LOG_LEVEL_WARNING:  ssd1306Print("[WARN]");   break;
        case LOG_LEVEL_ERROR:    ssd1306Print("[ERR]");    break;
        case LOG_LEVEL_CRITICAL: ssd1306Print("[CRIT]");   break;
    }

    ssd1306SetCursor(0, 24);
    ssd1306Print(entry->message);

    ssd1306SetCursor(0, 50);
    ssd1306Print("PRESS BACK");

    ssd1306Display();
}

void menuDisplayShowAbout(void)
{
    ssd1306Clear();

    ssd1306SetCursor(0, 0);
    ssd1306Print("LEAVES MONITOR");

    ssd1306SetCursor(0, 12);
    ssd1306Print("VER: 1.0");

    ssd1306SetCursor(0, 24);
    ssd1306Print("ESP32-S3");

    ssd1306SetCursor(0, 36);
    ssd1306Print("SPECTRAL+CO2");

    ssd1306SetCursor(0, 50);
    ssd1306Print("PRESS BACK");

    ssd1306Display();
}
