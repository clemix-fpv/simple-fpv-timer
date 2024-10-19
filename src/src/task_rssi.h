#pragma once

#include <freertos/FreeRTOS.h>
#include <stdbool.h>
#include "simple_fpv_timer.h"
#include "rx5808.h"
#include "timer.h"

typedef struct {
    rx5808_t rx5808;
    int freq;
    float rssi_offset_enter;
    float rssi_offset_leave;

    int rssi_raw;
    int rssi_smoothed;

    float rssi_filter;

    int rssi_peak;
    int rssi_enter;
    int rssi_leave;

    bool drone_in_gate;
    int in_gate_peak_rssi;
    millis_t in_gate_peak_millis;

    bool calibration;
    int calibration_min_rssi;
    int calibration_lap_count;
    int calibration_max_laps;

    sft_event_rssi_update_t rssi_update_ev;

} task_rssi_t;


void task_rssi_init(const ctx_t *ctx);
