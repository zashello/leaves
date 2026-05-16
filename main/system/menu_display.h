#pragma once

#include "driver/driver_scd41.h"
#include "driver/ei_classifier.h"
#include "system/system_log.h"
#include "menu_system.h"

void menuDisplayShowMenu(const menu_context_t *ctx);
void menuDisplayShowWaiting(const char *message);
void menuDisplayShowScd41Data(const scd41_data_t *data);
void menuDisplayShowPlantAnalysis(const ei_inference_result_t *result);
void menuDisplayShowMessage(const char *message);
void menuDisplayShowConfirm(const char *message);
void menuDisplayShowWifiAp(const char *ssid, const char *ip);
void menuDisplayShowError(const char *message);
void menuDisplayShowSuccess(const char *message);
void menuDisplayShowLogEntry(const log_entry_t *entry, int index, int total);
void menuDisplayShowAbout(void);
