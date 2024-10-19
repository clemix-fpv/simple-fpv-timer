#include <task_rssi.h>
#include "simple_fpv_timer.h"
#include "timer.h"
#include <config.h>

#define PIN_NUM_MOSI 23
#define PIN_NUM_CLK  18
#define PIN_NUM_CS   5
#define PIN_RSSI     ADC_CHANNEL_6

#define STACK_SIZE 4096
StackType_t task_rssi_stack[ STACK_SIZE ];
StaticTask_t task_rssi_buffer;

void task_rssi_set_config(task_rssi_t *tsk, const config_data_t *cfg)
{
    printf("FREQ:%d", cfg->freq);
    if(cfg->freq != tsk->freq) {
        if (tsk->rx5808.spi)
            rx5808_set_channel(&tsk->rx5808, cfg->freq);
        tsk->freq = cfg->freq;
    }

    tsk->rssi_filter = cfg->rssi_filter / 100.0f;
    if (tsk->rssi_filter < 0.01)
        tsk->rssi_filter = 0.01;

    tsk->rssi_peak = cfg->rssi_peak;
    tsk->rssi_offset_enter = cfg->rssi_offset_enter / 100.0f;
    tsk->rssi_offset_leave = cfg->rssi_offset_leave / 100.0f;

    tsk->rssi_enter = cfg->rssi_peak * tsk->rssi_offset_enter;
    tsk->rssi_leave = cfg->rssi_peak * tsk->rssi_offset_leave;

    tsk->calibration_min_rssi = cfg->calib_min_rssi_peak;
    tsk->calibration_max_laps = cfg->calib_max_lap_count;
    tsk->calibration_lap_count = 0;
    tsk->calibration = false;

    tsk->drone_in_gate = 0;
    tsk->in_gate_peak_rssi = 0;
    tsk->in_gate_peak_millis = 0;
}

void task_rssi_on_update_cfg(void* priv, esp_event_base_t base, int32_t id, void* event_data)
{
    task_rssi_t *tsk = (task_rssi_t*) priv;
    config_data_t *cfg = &((sft_event_cfg_changed_t*) event_data)->cfg;

    task_rssi_set_config(tsk, cfg);
}

void task_rssi_process_rssi(task_rssi_t *tsk, sft_timer_t *gate_blocked, int rssi_raw)
{
    /*                    Drone
     *                    left
     *                      |
     * ----*>>>>>>>>>>>>+>>>+~~~~~~~~~~~~~~~~~~~|----
     *     |            |                       |
     *   Drone      COLLECT_MIN            GATE_BLOCKED
     *   enter
     */
    #define COLLECT_MIN  700     /* 1s */
    #define GATE_BLOCKED 2000    /* 2s */

    tsk->rssi_raw = rssi_raw;

    tsk->rssi_smoothed = (tsk->rssi_filter * tsk->rssi_raw) +
                         ((1.0f - tsk->rssi_filter) * tsk->rssi_smoothed);

    if( tsk->calibration) {
        if (tsk->rssi_smoothed > tsk->rssi_peak &&
            tsk->rssi_smoothed > tsk->calibration_min_rssi) {

            tsk->rssi_peak = tsk->rssi_smoothed;
            tsk->rssi_enter = tsk->rssi_peak * tsk->rssi_offset_enter;
            tsk->rssi_leave = tsk->rssi_peak * tsk->rssi_offset_leave;
            tsk->drone_in_gate = false;

            timer_start(gate_blocked, COLLECT_MIN, NULL, NULL);
        }
    }

        /*ESP_LOGI(TAG, "rssi %d  %d %d", tsk->rssi_smoothed,*/
        /*                tsk->rssi_enter , tsk->rssi_leave);*/
    if (!tsk->rssi_enter || !tsk->rssi_leave)
        return;

    if (tsk->rssi_enter < tsk->rssi_smoothed &&
            timer_over(gate_blocked, NULL) &&
            !tsk->drone_in_gate) {
//        ESP_LOGI(TAG, "Drone enter gate! rssi: %d", tsk->rssi_smoothed);
        timer_start(gate_blocked, COLLECT_MIN, NULL, NULL);
        tsk->drone_in_gate = true;
        tsk->in_gate_peak_rssi = tsk->rssi_smoothed;
        tsk->in_gate_peak_millis = get_millis();

    } else if (tsk->drone_in_gate &&
            timer_over(gate_blocked, NULL) &&
            tsk->rssi_leave > tsk->rssi_smoothed) {
        tsk->drone_in_gate = false;

        timer_start(gate_blocked, GATE_BLOCKED, NULL, NULL);

        sft_event_drone_passed_t e = {
            .abs_time_ms = tsk->in_gate_peak_millis,
            .rssi = tsk->in_gate_peak_rssi,
        };

        ESP_ERROR_CHECK(
            esp_event_post(SFT_EVENT, SFT_EVENT_DRONE_PASSED,
                           &e, sizeof(e), pdMS_TO_TICKS(500)));

    } else if (tsk->drone_in_gate) {
        if (tsk->in_gate_peak_rssi < tsk->rssi_smoothed) {
            tsk->in_gate_peak_rssi = tsk->rssi_smoothed;
            tsk->in_gate_peak_millis = get_millis();
        }
    }
}

void task_rssi_collect_rssi(task_rssi_t *tsk, millis_t time)
{
    #define TIME_SLOT 300

    sft_event_rssi_update_t *ev = &tsk->rssi_update_ev;
    if (ev->cnt < SFT_RSSI_UPDATE_MAX) {
        int idx = ev->cnt++;
        ev->data[idx].abs_time_ms = time;
        ev->data[idx].rssi = tsk->rssi_smoothed;
        ev->data[idx].rssi_raw = tsk->rssi_raw;
        ev->data[idx].drone_in_gate = tsk->drone_in_gate;
    }

    if (ev->cnt >= SFT_RSSI_UPDATE_MAX ||
        (ev->cnt > 0 &&
         ev->data[ev->cnt - 1].abs_time_ms - ev->data[0].abs_time_ms >= TIME_SLOT)){

        ESP_ERROR_CHECK(
            esp_event_post(SFT_EVENT, SFT_EVENT_RSSI_UPDATE,
                           ev, sizeof(*ev), pdMS_TO_TICKS(100)));
        ev->cnt = 0;
    }

}

void task_rssi( void * priv )
{
    task_rssi_t *tsk = (task_rssi_t*) priv;
    sft_timer_t gate_blocked;
    sft_timer_t loop;
    millis_t ms;
    int adc_raw = 0;
    int voltage = 0;

    printf("rx5808 init\n");
    ESP_ERROR_CHECK(rx5808_init(&tsk->rx5808, PIN_NUM_MOSI,
                                PIN_NUM_CLK, PIN_NUM_CS, PIN_RSSI));

    printf("rx5808 set_channel(%d)\n", tsk->freq);
    rx5808_set_channel(&tsk->rx5808, tsk->freq);

    ESP_ERROR_CHECK(esp_event_handler_instance_register(SFT_EVENT, SFT_EVENT_CFG_CHANGED,
                                                        task_rssi_on_update_cfg,
                                                        tsk, NULL));
    for(;;) {
        timer_start(&loop, 50, NULL, NULL);

        rx5808_read_rssi(&tsk->rx5808, &adc_raw, &voltage);
        ms = get_millis();
        task_rssi_process_rssi(tsk, &gate_blocked, voltage);
        task_rssi_collect_rssi(tsk, ms);

        ms = get_millis();
        if (loop.end > ms)
            vTaskDelay(pdMS_TO_TICKS(loop.end - ms));
    }
}

void task_rssi_init(const ctx_t *ctx)
{
    static task_rssi_t tsk;

    memset (&tsk, 0, sizeof(tsk));

    task_rssi_set_config(&tsk, &ctx->cfg.eeprom);

    printf("START TASK\n");
    xTaskCreateStaticPinnedToCore(task_rssi, "task_rssi",
                                  STACK_SIZE, &tsk, tskIDLE_PRIORITY,
                                  task_rssi_stack, &task_rssi_buffer, 1);
}


