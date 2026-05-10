#pragma once
#include "esp_http_client.h"

esp_err_t sendToServerchan(const char *title, const char *desp, const char *serverchanKey);
void aiTest(void);
