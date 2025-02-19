// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "simple_fpv_timer.h"
#include <esp_err.h>
#include <esp_http_server.h>

esp_err_t gui_start(ctx_t *ctx);
esp_err_t gui_send_all(ctx_t *ctx, const char *msg);
esp_err_t gui_send_http(ctx_t *ctx, const char *url, const char *json);
esp_err_t gui_stop(ctx_t *ctx);
