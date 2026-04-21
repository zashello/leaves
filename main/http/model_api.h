#pragma once
static esp_err_t http_event_handler(esp_http_client_event_t *evt);
static void http_post_request(void);
static void http_task;
void AI_test(void);