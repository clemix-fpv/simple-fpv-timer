#include <stdint.h>
#include <string.h>
#include "esp_log.h"
#include <simple_fpv_timer.h>
#include <sys/types.h>
#include "esp_timer.h"
#include "config.h"
#include "led.h"
#include "timer.h"

#define LED_GPIO 2
static const char *TAG = "LED_TASK";

typedef struct task_led_s task_led_t;

typedef struct {
    color_t color;
    enum sft_led_command_type_e type;
    uint16_t offset;
    uint16_t num;
    millis_t duration;

    /* transition */
    uint16_t offset_end;
    uint16_t offset_current;
    uint16_t num_end;
    uint16_t num_current;
    millis_t start_time;
    uint16_t update_interval;

    void(*before_fn)(task_led_t*);
    void(*after_fn)(task_led_t*);
} led_command_t;


#define LED_TASK_CMD_STACK_MAX 32
struct task_led_s {
    led_t led;

    config_data_t cfg;

    uint16_t stack_sz;
    uint16_t stack_ptr;
    led_command_t stack[LED_TASK_CMD_STACK_MAX];

    esp_timer_handle_t timer;
};


static bool task_led_command_append(task_led_t *task, led_command_t * cmds, uint16_t num);

static void task_led_show_rssi(task_led_t *tskled, int rssi)
{
    uint32_t leds = 0;
    int ground = 500;

    if (!tskled->cfg.rssi[0].peak)
        return;

    if (rssi > ground) {
        rssi -= ground;
        int max = tskled->cfg.rssi[0].peak - ground;

        leds = (tskled->led.num_leds * ((rssi * 100)/ max)) / 100;
    }
    led_set(&tskled->led, 0, -1, 0);
    led_set(&tskled->led, 0, leds, COLOR_GREEN);
    led_refresh(&tskled->led);
}

void task_led_on_rssi_update(void* ctx, esp_event_base_t base, int32_t id, void* event_data)
{
    task_led_t *task = (task_led_t*) ctx;
    sft_event_rssi_update_t *ev = (sft_event_rssi_update_t*) event_data;
    int rssi = 0;

    if (task->cfg.game_mode != CFG_GAME_MODE_SPECTRUM)
        return;

    if (ev->cnt <= 0)
        return;

    for (int i = 0; i < ev->cnt; i++)
        rssi += ev->data[i].rssi;
    rssi /= ev->cnt;

    task_led_show_rssi(task, rssi);
}

void task_led_increase_num_leds(task_led_t *task)
{
    led_set_num_leds(&task->led, max(task->cfg.led_num, 256));
}

void task_led_num_leds_cfg(task_led_t *task)
{
    led_set_num_leds(&task->led, task->cfg.led_num);
}

void task_led_boot_start(task_led_t* task)
{
    led_command_t cmds[] =  {
        {.color = COLOR_RED, .num = 100, .duration = 1500, .before_fn = task_led_increase_num_leds},
        {.color = COLOR_GREEN, .num = 0, .duration = 1500,
            .num_end = 100, .type = SFT_LED_CMD_TYPE_PERCENT_TRANSITION, .update_interval = 100,
        },
        {.color = COLOR_GREEN, .num = 100, .duration = 1500},
        {.color = COLOR_BLUE, .num = 0, .duration = 1500,
            .num_end = 100, .type = SFT_LED_CMD_TYPE_PERCENT_TRANSITION, .update_interval = 100,
        },
        {.color = COLOR_BLUE, .num = 100, .duration = 1500},
        {.color = COLOR(252, 186, 3), .num = 0, .duration = 1500,
            .num_end = 100, .type = SFT_LED_CMD_TYPE_PERCENT_TRANSITION, .update_interval = 100,
        },
        {.color = COLOR(252, 186, 3), .num = 100, .duration = 1500},
        {.color = COLOR(207, 3,   252), .num = 0, .duration = 1500,
            .num_end = 100, .type = SFT_LED_CMD_TYPE_PERCENT_TRANSITION, .update_interval = 100,
        },
        {.color = COLOR(207, 3,   252), .num = 100, .duration = 1500},
        {.color = COLOR(3,   252, 252), .num = 0, .duration = 1500,
            .num_end = 100, .type = SFT_LED_CMD_TYPE_PERCENT_TRANSITION, .update_interval = 100,
        },
        {.color = COLOR(3,   252, 252), .num = 100, .duration = 1500},
        {.color = COLOR(252, 3,   119), .num = 0, .duration = 1500,
            .num_end = 100, .type = SFT_LED_CMD_TYPE_PERCENT_TRANSITION, .update_interval = 100,
        },
        {.color = COLOR(252, 3,   119), .num = 100, .duration = 1500},
        {.color = COLOR(252, 102, 3), .num = 0, .duration = 1500,
            .num_end = 100, .type = SFT_LED_CMD_TYPE_PERCENT_TRANSITION, .update_interval = 100,
        },
        {.color = COLOR(252, 102, 3), .num = 100, .duration = 1500, .after_fn = task_led_num_leds_cfg},
    };

    task_led_command_append(task, cmds, sizeof(cmds) / sizeof(cmds[0]));
}


static void task_led_command_process(task_led_t* task)
{
    uint32_t idx_start = 0;
    uint32_t num_leds = 0;
    uint64_t duration;
    led_command_t *cmd;

    if (task->stack_ptr >= task->stack_sz)
        return;
    cmd = &task->stack[task->stack_ptr];

    if (cmd->before_fn)
        cmd->before_fn(task);

    switch(cmd->type) {
        case SFT_LED_CMD_TYPE_PERCENT:
            idx_start = (cmd->offset * task->led.num_leds) / 100;
            num_leds = (cmd->num * task->led.num_leds) / 100;
            duration = cmd->duration > 0 ? cmd->duration * 1000 : 1;
            task->stack_ptr++;
            break;
        default:
        case SFT_LED_CMD_TYPE_NUM:
            idx_start = cmd->offset;
            num_leds = cmd->num;
            duration = cmd->duration > 0 ? cmd->duration * 1000 : 1;
            task->stack_ptr++;
            break;

        case SFT_LED_CMD_TYPE_PERCENT_TRANSITION:
        case SFT_LED_CMD_TYPE_NUM_TRANSITION:
            if (!cmd->start_time)
                cmd->start_time = get_millis();

            millis_t elapsed = get_millis() - cmd->start_time;
            uint16_t persent;

            if (elapsed >= cmd->duration){
                task->stack_ptr++;
                persent = 100;
                duration = 1;
            } else {
                persent = (elapsed * 100) / cmd->duration;
                duration = (cmd->update_interval?: 500) * 1000;
            }

            uint16_t led_offset = cmd->offset;
            uint16_t led_offset_end = cmd->offset_end;
            uint16_t led_num = cmd->num;
            uint16_t led_num_end = cmd->num_end;
            if (cmd->type == SFT_LED_CMD_TYPE_PERCENT_TRANSITION) {
                led_offset = (cmd->offset * task->led.num_leds) / 100;
                led_offset_end = (cmd->offset_end * task->led.num_leds) / 100;
                led_num = (cmd->num * task->led.num_leds) / 100;
                led_num_end = (cmd->num_end * task->led.num_leds) / 100;
            }

            uint16_t add_offset = ((led_offset_end - led_offset) * persent) / 100;
            uint16_t add_num = ((led_num_end - led_num) * persent) / 100;

            idx_start = led_offset + add_offset;
            num_leds = led_num + add_num;
            break;
    }

    led_set(&task->led, idx_start, num_leds, cmd->color);
    led_refresh(&task->led);

    if (cmd->after_fn)
        cmd->after_fn(task);

    esp_timer_start_once(task->timer, duration);
}

static void task_led_on_cmd_timer(void* arg)
{
    task_led_t *task = (task_led_t*) arg;
    task_led_command_process(task);
}

static bool task_led_command_append(task_led_t *task, led_command_t * cmds, uint16_t num)
{
    if (task->stack_ptr > 0) {
        if (task->stack_ptr > task->stack_sz) {
            task->stack_sz = 0;
        } else {
            memmove(task->stack, &task->stack[task->stack_ptr],
                    (task->stack_sz - task->stack_ptr) * sizeof(task->stack[0]));
            task->stack_sz -= task->stack_ptr;
            task->stack_ptr = 0;
        }
    }
    if (num + task->stack_sz > LED_TASK_CMD_STACK_MAX) {
        ESP_LOGE(TAG, "Failed to add led command stack! needed:%"PRIu16" used:%"PRIu16" max:%"PRIu16,
                 num, task->stack_sz, LED_TASK_CMD_STACK_MAX);
        return false;
    }

    memcpy(&task->stack[task->stack_sz], cmds, num * sizeof(cmds[0]));
    task->stack_sz += num;

    task_led_command_process(task);
    return true;
}

void task_led_on_start_race(void* ctx, esp_event_base_t base, int32_t id, void* event_data)
{
    task_led_t *task = (task_led_t*) ctx;
    sft_event_start_race_t *ev = (sft_event_start_race_t*) event_data;


    led_command_t cmds[] =  {
        {.color = COLOR_BLUE, .num = 100, .duration = 0, .before_fn = task_led_increase_num_leds},
        {.color = 0, .type = SFT_LED_CMD_TYPE_PERCENT_TRANSITION,
            .offset = 100, .offset_end = 0,
            .num = 0, .duration = ev->offset,
            .num_end = 100
        },
        {.color = COLOR_RED,   .num = 33,  .duration = 1000 },
        {.color = COLOR_RED,   .num = 66,  .duration = 1000 },
        {.color = COLOR_RED,   .num = 100, .duration = 1000 },
        {.color = COLOR_GREEN, .num = 100
        },
    };

    task_led_command_append(task, cmds, sizeof(cmds) / sizeof(cmds[0]));
}

void task_led_on_cfg_change(void* ctx, esp_event_base_t base, int32_t id, void* event_data)
{
    task_led_t *task = (task_led_t*) ctx;
    sft_event_cfg_changed_t *ev = (sft_event_cfg_changed_t*) event_data;

    task->cfg = ev->cfg;

    task_led_boot_start(task);
}

void task_led_init(const ctx_t *ctx)
{
    static task_led_t led = {0};

    led.cfg = ctx->cfg.eeprom;

    led_init(&led.led, LED_GPIO, ctx->cfg.eeprom.led_num);

    const esp_timer_create_args_t timer_args = {
        .callback = &task_led_on_cmd_timer,
        .arg = (void*) &led,
        .name = "led-command-stack"
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &led.timer));

    esp_event_handler_register(SFT_EVENT, SFT_EVENT_RSSI_UPDATE, task_led_on_rssi_update, &led);
    esp_event_handler_register(SFT_EVENT, SFT_EVENT_START_RACE, task_led_on_start_race, &led);
    esp_event_handler_register(SFT_EVENT, SFT_EVENT_CFG_CHANGED, task_led_on_cfg_change, &led);

    task_led_boot_start(&led);
}


