#pragma once
#include "esp_http_client.h"

#define SERVER_CHAN_SENDKEY "SCT252386TA-875rfSUzSdIvdpTqERz2INsr"
#define SERVER_CHAN_API_URL "http://sctapi.ftqq.com/"

static esp_err_t http_event_handler(esp_http_client_event_t *evt);
static void http_post_request(void);
static void http_task(void *pvParameters);
esp_err_t send_to_serverchan(const char *title, const char *desp);
void AI_test(void);