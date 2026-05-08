#ifndef _EVENT_H_
#define _EVENT_H_

#include <stddef.h>
#include <stdint.h>

// 配置
#ifndef EVENT_QUEUE_SIZE
#define EVENT_QUEUE_SIZE 16U
#endif

#ifndef EVENT_HANDLER_POOL_SIZE
#define EVENT_HANDLER_POOL_SIZE 16U
#endif

#ifndef EVENT_MONITOR_POOL_SIZE
#define EVENT_MONITOR_POOL_SIZE 16U
#endif

#ifndef EVENT_TIMER_POOL_SIZE
#define EVENT_TIMER_POOL_SIZE 16U
#endif

#if EVENT_QUEUE_SIZE == 0U
#error "EVENT_QUEUE_SIZE must be greater than 0"
#endif

#if EVENT_HANDLER_POOL_SIZE == 0U
#error "EVENT_HANDLER_POOL_SIZE must be greater than 0"
#endif

#if EVENT_MONITOR_POOL_SIZE == 0U
#error "EVENT_MONITOR_POOL_SIZE must be greater than 0"
#endif

#if EVENT_TIMER_POOL_SIZE == 0U
#error "EVENT_TIMER_POOL_SIZE must be greater than 0"
#endif

#ifndef TIMER_POOL_SIZE
#define TIMER_POOL_SIZE EVENT_TIMER_POOL_SIZE
#endif

// 依赖
#include "../timer/timer.h"

#define EVENT_ID_INVALID UINT32_MAX
#define EVENT_HANDLER_ID_INVALID UINT32_MAX
#define EVENT_MONITOR_ID_INVALID UINT32_MAX

// 类型定义
typedef uint32_t EventId;
typedef uint32_t EventHandlerId;
typedef uint32_t EventMonitorId;

typedef struct event_scheduler_t EventScheduler;

typedef enum event_source_t
{
    WD_EVENT_SOURCE_EXTERNAL = 0,
    WD_EVENT_SOURCE_TIMER,
    WD_EVENT_SOURCE_MONITOR
} EventSource;

typedef struct event_t
{
    EventId id;
    EventSource source;
    uint32_t value;
    void *user_data;
} Event;

typedef void (*event_handler_fn)(EventScheduler *scheduler, const Event *event, void *user_data);
typedef int (*event_monitor_fn)(EventScheduler *scheduler, uint32_t *value, void **event_user_data, void *user_data);

typedef struct event_handler_t
{
    EventHandlerId id;
    EventId event_id;
    event_handler_fn handler;
    void *user_data;
} EventHandler;

typedef struct event_monitor_t
{
    EventScheduler *scheduler;
    EventMonitorId id;
    EventId event_id;
    event_monitor_fn monitor;
    TimerId timer_id;
    TimerTick period_ticks;
    void *user_data;
} EventMonitor;

typedef struct event_timer_plan_t
{
    EventScheduler *scheduler;
    EventId event_id;
    TimerId timer_id;
    uint8_t mode;
    uint32_t value;
    void *user_data;
} EventTimerPlan;

struct event_scheduler_t
{
    TimerScheduler timer;
    Event queue[EVENT_QUEUE_SIZE];
    size_t queue_head;
    size_t queue_tail;
    size_t queue_count;
    EventHandler handlers[EVENT_HANDLER_POOL_SIZE];
    EventMonitor monitors[EVENT_MONITOR_POOL_SIZE];
    EventTimerPlan timer_plans[TIMER_POOL_SIZE];
    EventHandlerId next_handler_id;
    EventMonitorId next_monitor_id;
    size_t monitor_scan_index;
    int internal_error;
};

// 接口
void event_scheduler_init(EventScheduler *self, const TimerOps *timer_ops, void *timer_user_data);
int event_scheduler_run_once(EventScheduler *self);
TimerTick event_scheduler_next_delay(EventScheduler *self);

EventHandlerId event_handler_add(EventScheduler *self, EventId event_id, event_handler_fn handler, void *user_data);
int event_handler_remove(EventScheduler *self, EventHandlerId id);

int event_post(EventScheduler *self, EventId event_id, uint32_t value, void *user_data);
int event_trigger(EventScheduler *self, EventId event_id, uint32_t value, void *user_data);

int event_schedule_delay(EventScheduler *self, EventId event_id, TimerTick delay_ticks, uint32_t value, void *user_data);
int event_schedule_period(EventScheduler *self, EventId event_id, TimerTick period_ticks, uint32_t value, void *user_data);
int event_unschedule(EventScheduler *self, EventId event_id);

EventMonitorId event_monitor_add(EventScheduler *self, EventId event_id, event_monitor_fn monitor, TimerTick period_ticks, void *user_data);
int event_monitor_remove(EventScheduler *self, EventMonitorId id);
#endif
