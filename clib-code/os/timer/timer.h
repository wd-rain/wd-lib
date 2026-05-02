#ifndef _TIMER_H_
#define _TIMER_H_

#include <stddef.h>
#include <stdint.h>

// 依赖
#include "../../until/until.h"

// 配置
#ifndef TIMER_POOL_SIZE
#define TIMER_POOL_SIZE 16U
#endif

#if TIMER_POOL_SIZE == 0U
#error "TIMER_POOL_SIZE must be greater than 0"
#endif

#define TIMER_ID_INVALID UINT32_MAX

// 类型定义
typedef uint32_t TimerTick;
typedef uint32_t TimerId;

typedef struct timer_scheduler_t TimerScheduler;

typedef TimerTick (*timer_get_tick_fn)(void *user_data);
typedef void (*timer_action_fn)(TimerScheduler *scheduler, TimerId id, void *user_data);

typedef struct timer_ops_t
{
    timer_get_tick_fn get_tick;
} TimerOps;

typedef struct timer_t
{
    TimerId id;
    TimerTick deadline;
    TimerTick period;
    timer_action_fn action;
    void *user_data;
    struct timer_t *next;
} Timer;

struct timer_scheduler_t
{
    const TimerOps *ops;
    void *ops_user_data;
    Timer timers[TIMER_POOL_SIZE];
    Timer *active_head;
    TimerId next_id;
};

// 接口
void timer_scheduler_init(TimerScheduler *self, const TimerOps *ops, void *ops_user_data);
int timer_scheduler_run_once(TimerScheduler *self);
TimerTick timer_scheduler_now(TimerScheduler *self);
TimerTick timer_scheduler_next_delay(TimerScheduler *self);

TimerId timer_new(TimerScheduler *self);
int timer_delete(TimerScheduler *self, TimerId id);
int timer_start(TimerScheduler *self, TimerId id, TimerTick delay_ticks, TimerTick period_ticks, timer_action_fn action, void *user_data);
int timer_stop(TimerScheduler *self, TimerId id);
int timer_is_running(TimerScheduler *self, TimerId id);
int timer_remaining(TimerScheduler *self, TimerId id);

#endif
