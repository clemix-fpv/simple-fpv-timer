#pragma once

#include <freertos/FreeRTOS.h>
#include <stdbool.h>
#include "simple_fpv_timer.h"
#include "rx5808.h"
#include "timer.h"

#define MAX_FREQ 8

typedef struct {
    int freq;

    int raw;
    int smoothed;

    int peak;
    int enter;
    int leave;

    float filter;
    float offset_enter;
    float offset_leave;

    bool drone_in_gate;
    int in_gate_peak_rssi;
    millis_t in_gate_peak_millis;

    bool calibration;
    int calibration_min_rssi;
    int calibration_lap_count;
    int calibration_max_laps;

    millis_t collect_next;
} rssi_t;

typedef struct {
    rx5808_t rx5808;

    rssi_t rssi_array[MAX_FREQ];
    uint16_t rssi_cnt;
    rssi_t *rssi;           /* pointer to current rssi_array[idx] */

    sft_event_rssi_update_t rssi_update_ev[MAX_FREQ];

} task_rssi_t;


void task_rssi_init(const ctx_t *ctx);
