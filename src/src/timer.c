#include "timer.h"
#include <esp_timer.h>

millis_t get_millis()
{
    return esp_timer_get_time() / 1000;
}

void timer_restart(sft_timer_t *t, millis_t over_shoot)
{
    t->start = get_millis();
    if ((t->duration > over_shoot))
        t->end = t->start + t->duration - over_shoot;
    else
        t->end = t->start + 1;
}

bool timer_over(sft_timer_t *t, millis_t *over_shoot)
{
    if (!t->end)
        return true;

    millis_t ms = get_millis();
    if (t->end <= ms) {
        if (over_shoot)
            *over_shoot = ms - t->end;
        t->end = 0;
        return true;
    }
    return false;
}

void timer_start(sft_timer_t *t, millis_t duration,
                 int (*cb)(millis_t, void*), void * user_data)
{
    t->callback = cb;
    t->duration = duration;
    t->user_data = user_data;

    t->start = get_millis();
    t->end = t->start + t->duration;
}

millis_t process_timer(sft_timer_t *timers, int max_timer)
{
    sft_timer_t *t;
    millis_t over_shoot = 0;
    millis_t need_wait = MILLIS_MAX;

    for (int i = 0; i < max_timer; i++) {
        t = &timers[i];
        if (t->end == 0)
            continue;

        if (timer_over(t, &over_shoot)) {
            if (t->callback)
                switch (t->callback(over_shoot, t->user_data)){
                    case TIMER_RESTART:
                        timer_restart(t, over_shoot);
                        need_wait = min(need_wait, t->end - t->start);
                        break;
                    case TIMER_BREAK_LOOP:
                        return 0;
                    case TIMER_STOP:
                    default:
                        ;; /* Nothing to do */
                }
        } else {
            need_wait = min(need_wait, t->end - get_millis());
        }
    }
    return (need_wait == MILLIS_MAX)? MILLIS_MAX : need_wait;
}


