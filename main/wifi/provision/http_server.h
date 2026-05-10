#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

esp_err_t httpServerStart(httpd_handle_t *outServer);
void httpServerStop(httpd_handle_t server);
