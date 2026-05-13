#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

esp_err_t provHttpStart(httpd_handle_t *outServer);
void provHttpStop(httpd_handle_t server);
