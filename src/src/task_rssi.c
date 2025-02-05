#include <stdint.h>
#include <task_rssi.h>
#include "esp_err.h"
#include "simple_fpv_timer.h"
#include "timer.h"
#include <config.h>
#include "esp_log.h"

#define PIN_NUM_MOSI 23
#define PIN_NUM_CLK  18
#define PIN_NUM_CS   5
#define PIN_RSSI     ADC_CHANNEL_6

#define STACK_SIZE 4096
StackType_t task_rssi_stack[ STACK_SIZE ];
StaticTask_t task_rssi_buffer;

static const char * TAG = "task-rssi";

static esp_err_t task_rssi_next_channel(task_rssi_t *tsk);


static void task_rssi_set_config(task_rssi_t *tsk, const config_data_t *cfg)
{
    ESP_LOGI(TAG, "%s - ENTER", __func__);

    for (int i=0; i< CFG_MAX_FREQ; i++)
        printf("%d ", cfg->rssi[i].freq);


    tsk->rssi_cnt = 0;
    for (int i=0; i< CFG_MAX_FREQ; i++) {
        rssi_t *rssi = &tsk->rssi_array[i];
        const config_rssi_t *cfg_rssi = &cfg->rssi[i];
        sft_event_rssi_update_t *ev = &tsk->rssi_update_ev[i];

        if (cfg_rssi->freq == 0) {
            memset(rssi, 0, sizeof(rssi_t) * (MAX_FREQ - i));
            memset(ev, 0, sizeof(sft_event_rssi_update_t) * (MAX_FREQ - i));
            break;
        }

        tsk->rssi_cnt++;
        rssi->freq = cfg_rssi->freq;
        rssi->filter = cfg_rssi->filter / 100.0f;
        if (rssi->filter < 0.01)
            rssi->filter = 0.01;

        rssi->peak = cfg_rssi->peak;
        rssi->offset_enter = cfg_rssi->offset_enter / 100.0f;
        rssi->offset_leave = cfg_rssi->offset_leave / 100.0f;

        rssi->enter = rssi->peak * rssi->offset_enter;
        rssi->leave = rssi->peak * rssi->offset_leave;

        rssi->calibration_min_rssi = cfg_rssi->calib_min_rssi_peak;
        rssi->calibration_max_laps = cfg_rssi->calib_max_lap_count;
        rssi->calibration_lap_count = 0;
        rssi->calibration = false;

        rssi->drone_in_gate = 0;
        rssi->in_gate_peak_rssi = 0;
        rssi->in_gate_peak_millis = 0;


        if (rssi->freq != ev->freq) {
            memset(ev, 0, sizeof(sft_event_rssi_update_t));
            ev->freq = rssi->freq;
        }
    }

    tsk->rssi = NULL;

    task_rssi_next_channel(tsk);
}

static void task_rssi_on_update_cfg(void* priv, esp_event_base_t base, int32_t id, void* event_data)
{
    task_rssi_t *tsk = (task_rssi_t*) priv;
    config_data_t *cfg = &((sft_event_cfg_changed_t*) event_data)->cfg;

    ESP_LOGI(TAG, "On config update!");
    task_rssi_set_config(tsk, cfg);
}

static void task_rssi_process_rssi(task_rssi_t *tsk, sft_timer_t *gate_blocked, int rssi_raw)
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

    rssi_t *rssi = tsk->rssi;

    if (!rssi)
        return;


    rssi->raw = rssi_raw;

    rssi->smoothed = (rssi->filter * rssi->raw) +
                         ((1.0f - rssi->filter) * rssi->smoothed);

    if( rssi->calibration) {
        if (rssi->smoothed > rssi->peak &&
            rssi->smoothed > rssi->calibration_min_rssi) {

            rssi->peak = rssi->smoothed;
            rssi->enter = rssi->peak * rssi->offset_enter;
            rssi->leave = rssi->peak * rssi->offset_leave;
            rssi->drone_in_gate = false;

            timer_start(gate_blocked, COLLECT_MIN, NULL, NULL);
        }
    }

    /*ESP_LOGI(TAG, "rssi %d  enter:%d leave:%d in-gate:%d blocked:%d", */
    /*                    rssi->smoothed,*/
    /*                    rssi->enter , rssi->leave, rssi->drone_in_gate,*/
    /*                     !timer_over(gate_blocked, NULL));*/

    if (!rssi->enter || !rssi->leave)
        return;

    if (rssi->enter < rssi->smoothed &&
            timer_over(gate_blocked, NULL) &&
            !rssi->drone_in_gate) {
        ESP_LOGI(TAG, "Drone enter gate! rssi: %d", rssi->smoothed);
        timer_start(gate_blocked, COLLECT_MIN, NULL, NULL);
        rssi->drone_in_gate = true;
        rssi->in_gate_peak_rssi = rssi->smoothed;
        rssi->in_gate_peak_millis = get_millis();

        sft_event_drone_enter_t e = {
            .freq = rssi->freq,
            .abs_time_ms = rssi->in_gate_peak_millis,
            .rssi = rssi->in_gate_peak_rssi,
        };

        ESP_ERROR_CHECK(
            esp_event_post(SFT_EVENT, SFT_EVENT_DRONE_ENTER,
                           &e, sizeof(e), pdMS_TO_TICKS(500)));

    } else if (rssi->drone_in_gate &&
            timer_over(gate_blocked, NULL) &&
            rssi->leave > rssi->smoothed) {
        rssi->drone_in_gate = false;

        timer_start(gate_blocked, GATE_BLOCKED, NULL, NULL);

        sft_event_drone_passed_t e = {
            .freq = rssi->freq,
            .abs_time_ms = rssi->in_gate_peak_millis,
            .rssi = rssi->in_gate_peak_rssi,
        };

        ESP_LOGI(TAG, "DRONE PASSED");
        ESP_ERROR_CHECK(
            esp_event_post(SFT_EVENT, SFT_EVENT_DRONE_PASSED,
                           &e, sizeof(e), pdMS_TO_TICKS(500)));

    } else if (rssi->drone_in_gate) {
        if (rssi->in_gate_peak_rssi < rssi->smoothed) {
            rssi->in_gate_peak_rssi = rssi->smoothed;
            rssi->in_gate_peak_millis = get_millis();
        }
    }
}

static void task_rssi_collect_rssi(task_rssi_t *tsk, millis_t time)
{
#define TIME_SLOT 300    /* Send out collected data after at 300s */
#define TIME_OFFSET 100  /* only every 100ms one datapoint for displaying */

    rssi_t *rssi = tsk->rssi;

    if (!rssi)
        return;
    int idx = rssi - tsk->rssi_array;

    sft_event_rssi_update_t *ev = &tsk->rssi_update_ev[idx];

    idx = ev->cnt > 0 ? ev->cnt -1 : 0;

    if (time >= rssi->collect_next ||
        ev->data[idx].drone_in_gate != rssi->drone_in_gate) {

        if (ev->cnt +1 >= SFT_RSSI_UPDATE_MAX ||
            (ev->cnt > 0 &&
            ev->data[idx].abs_time_ms - ev->data[0].abs_time_ms >= TIME_SLOT)){

            if (esp_event_post(SFT_EVENT, SFT_EVENT_RSSI_UPDATE,
                               ev, sizeof(*ev), pdMS_TO_TICKS(100)) == ESP_OK) {
                ev->cnt = 0;
            } else {
                if (ev->cnt +1 >= SFT_RSSI_UPDATE_MAX)
                    ev->cnt = 0;
            }
        }
        idx = ev->cnt++;
        ev->data[idx].abs_time_ms = time;
        ev->data[idx].rssi = rssi->smoothed;
        ev->data[idx].rssi_raw = rssi->raw;
        ev->data[idx].drone_in_gate = rssi->drone_in_gate;

        rssi->collect_next = time + TIME_OFFSET;

    } else if (ev->data[idx].rssi < rssi->smoothed) {
        ev->data[idx].rssi = rssi->smoothed;
        ev->data[idx].rssi_raw = rssi->raw;
    }
}

static esp_err_t task_rssi_set_channel(task_rssi_t *tsk, rssi_t *rssi)
{
    int idx;
    esp_err_t e;

    if (tsk->rssi == rssi) {
        return ESP_OK;
    }

    /* SANITY CHECK */
    idx = rssi - tsk->rssi_array;
    if (idx < 0 || idx >= MAX_FREQ) {
        tsk->rssi = NULL;
        return ESP_ERR_INVALID_ARG;
    }

    if (!tsk->rx5808.spi) {
        tsk->rssi = NULL;
        return ESP_ERR_NOT_ALLOWED;
    }

    if ((e = rx5808_set_channel(&tsk->rx5808, rssi->freq)) != ESP_OK) {
        tsk->rssi = NULL;
        return e;
    }

    tsk->rssi = rssi;
    return ESP_OK;
}

static esp_err_t task_rssi_set_channel_by_freq(task_rssi_t *tsk, int freq)
{
    int idx;
    rssi_t *ptr;

    if (tsk->rssi && tsk->rssi->freq == freq) {
        return ESP_OK;
    }

    for (idx=0, ptr = tsk->rssi_array; idx < MAX_FREQ; idx++, ptr++) {
        if (ptr->freq == freq)
            break;
    }

    return task_rssi_set_channel(tsk, ptr);
}

static esp_err_t task_rssi_next_channel(task_rssi_t *tsk)
{
    int idx = 0;

    if (tsk->rssi) {
        idx = tsk->rssi - tsk->rssi_array;

        if (idx < 0 || idx >= MAX_FREQ)
            return ESP_ERR_NOT_ALLOWED;

        idx = (idx + 1) % MAX_FREQ;
    }

    if (tsk->rssi_array[idx].freq == 0) {
        idx = 0;
    }

    return task_rssi_set_channel(tsk, &tsk->rssi_array[idx]);

}

void task_rssi( void * priv )
{
    task_rssi_t *tsk = (task_rssi_t*) priv;
    sft_timer_t gate_blocked = {0};
    sft_timer_t loop = {0};
    sft_timer_t s1 = {0};
    uint32_t read_cnt = 0;
    millis_t ms;
    int adc_raw = 0;
    int voltage = 0;
    int freq_plan[8] = { 5658, 5695, 5732, 5769, 5806, 5843, 5880, 5917 };

    printf("rx5808 init\n");
    ESP_ERROR_CHECK(rx5808_init(&tsk->rx5808, PIN_NUM_MOSI,
                                PIN_NUM_CLK, PIN_NUM_CS, PIN_RSSI));

    ESP_ERROR_CHECK_WITHOUT_ABORT(task_rssi_next_channel(tsk));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(SFT_EVENT, SFT_EVENT_CFG_CHANGED,
                                                        task_rssi_on_update_cfg,
                                                        tsk, NULL));
    timer_start(&s1, 1000, NULL, NULL);
    uint16_t change_channel_counter = 0;
    for(;;) {
        timer_start(&loop, 5, NULL, NULL);

        read_cnt++;
        rx5808_read_rssi(&tsk->rx5808, &adc_raw, &voltage);
        ms = get_millis();
        task_rssi_process_rssi(tsk, &gate_blocked, voltage);
        task_rssi_collect_rssi(tsk, ms);

        if (tsk->rssi_cnt > 1 && change_channel_counter++ > 10) {
            ESP_ERROR_CHECK_WITHOUT_ABORT(task_rssi_next_channel(tsk));
            change_channel_counter = 0;
        }

        ms = get_millis();
        if (loop.end > ms)
            vTaskDelay(pdMS_TO_TICKS(loop.end - ms));

        if (timer_over(&s1, &ms)) {
            ms = 1000 - ms;
            printf("rx5808 rssi reads %lu/%llums (%0.2f) millis:%llu\n",
                   read_cnt, ms, (float)read_cnt/ms, get_millis());
            read_cnt=0;
            timer_start(&s1, 1000, NULL, NULL);
        }
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


