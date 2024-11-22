// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include <freertos/FreeRTOS.h>
#include <lwip/ip4_addr.h>
#include <esp_http_server.h>
#include <stdbool.h>
#include "timer.h"
#include "wifi.h"
#include "config.h"
#include "osd.h"
#include "led.h"

#define min(a,b) \
({ __typeof__ (a) _a = (a); \
    __typeof__ (b) _b = (b); \
    _a < _b ? _a : _b; })

#define max(a,b) \
({ __typeof__ (a) _a = (a); \
    __typeof__ (b) _b = (b); \
    _a > _b ? _a : _b; })

typedef enum {
    SFT_EVENT_DRONE_PASSED,
    SFT_EVENT_DRONE_ENTER,
    SFT_EVENT_DRONE_LEAVE,
    SFT_EVENT_CFG_CHANGED,
    SFT_EVENT_RSSI_UPDATE,
    SFT_EVENT_START_RACE,
} sft_event_t;

typedef struct {
    int freq;
    millis_t abs_time_ms;
    int rssi;
} sft_event_drone_passed_t;

typedef struct {
    config_data_t cfg;
} sft_event_cfg_changed_t;

#define SFT_RSSI_UPDATE_MAX 32
typedef struct {
    int cnt;
    int freq;
    struct  {
        millis_t abs_time_ms;
        int rssi;
        int rssi_raw;
        bool drone_in_gate;
    } data[SFT_RSSI_UPDATE_MAX];
} sft_event_rssi_update_t;

typedef struct {
    millis_t offset;
} sft_event_start_race_t;


ESP_EVENT_DECLARE_BASE(SFT_EVENT);


typedef struct lap_s {
    int id;
    int rssi;
    millis_t duration_ms;
    millis_t abs_time_ms;
} lap_t;

#define MAX_NAME_LEN 32
#define MAX_LAPS 16

typedef struct player_s {
    char name[MAX_NAME_LEN];
    lap_t laps[MAX_LAPS];
    int next_idx;
    ip4_addr_t ip4;
} player_t;

#define MAX_PLAYER 8

typedef struct lap_counter_s {
    bool in_calib_mode[CFG_MAX_FREQ];
    int in_calib_lap_count[CFG_MAX_FREQ];

    int num_player;
    player_t players[MAX_PLAYER];

} lap_counter_t;

typedef struct {
    config_t cfg;
    SemaphoreHandle_t sem;
    httpd_handle_t gui;
    osd_t osd;

    wifi_t wifi;
    lap_counter_t lc;
    led_t led;

    ip4_addr_t server_ipv4;
} ctx_t;


void sft_init(ctx_t *ctx);

bool sft_encode_lapcounter(lap_counter_t *lc, json_writer_t *jw);
bool sft_encode_settings(ctx_t *ctx, json_writer_t *jw);
player_t* sft_player_get_or_create(lap_counter_t *lc, ip4_addr_t ip4, const char *name);
lap_t* sft_player_add_lap(player_t *player, int id, int rssi, millis_t duration, millis_t abs_time);
bool sft_update_settings(ctx_t *ctx);
void sft_start_calibration(ctx_t *ctx);
