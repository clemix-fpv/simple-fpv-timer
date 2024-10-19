#pragma once
#include <stdint.h>
#include <stdbool.h>

#define MILLIS_MAX  UINT64_MAX
typedef uint64_t millis_t;

#define TIMER_RESTART 0
#define TIMER_STOP    1
#define TIMER_BREAK_LOOP 2
typedef struct sft_timer_s {
    int64_t start;
    int64_t duration;
    int64_t end; /*  calculated end in milliseconds */
    int (*callback)(millis_t over_shoot, void *user_data);
    void * user_data;
} sft_timer_t;

#define min(a,b) \
({ __typeof__ (a) _a = (a); \
    __typeof__ (b) _b = (b); \
    _a < _b ? _a : _b; })

millis_t get_millis();

void timer_restart(sft_timer_t *t, millis_t over_shoot);
bool timer_over(sft_timer_t *t, millis_t *over_shoot);
void timer_start(sft_timer_t *t, millis_t duration,
                 int (*cb)(millis_t, void*), void *user_data);
millis_t process_timer(sft_timer_t *timers, int max_timer);
