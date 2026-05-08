#include "event.h"

#define EVENT_TIMER_MODE_UNUSED 0U
#define EVENT_TIMER_MODE_DELAY 1U
#define EVENT_TIMER_MODE_PERIOD 2U

static const TimerTick _event_tick_half_range = (TimerTick)(UINT32_MAX / 2U + 1U);

static void _event_assert_timer_ops(const TimerOps *ops)
{
    WD_ASSERT(ops != NULL);
    WD_ASSERT(ops->get_tick != NULL);
}

static void _event_assert_ready(const EventScheduler *self)
{
    WD_ASSERT(self != NULL);
    _event_assert_timer_ops(self->timer.ops);
}

static void _event_assert_event_id(EventId id)
{
    WD_ASSERT(id != EVENT_ID_INVALID);
}

static int _event_handler_is_allocated(const EventHandler *handler)
{
    return handler->id != EVENT_HANDLER_ID_INVALID;
}

static int _event_handler_is_free(const EventHandler *handler)
{
    return handler->id == EVENT_HANDLER_ID_INVALID;
}

static int _event_monitor_is_allocated(const EventMonitor *monitor)
{
    return monitor->id != EVENT_MONITOR_ID_INVALID;
}

static int _event_monitor_is_free(const EventMonitor *monitor)
{
    return monitor->id == EVENT_MONITOR_ID_INVALID;
}

static int _event_timer_plan_is_allocated(const EventTimerPlan *plan)
{
    return plan->mode != EVENT_TIMER_MODE_UNUSED;
}

static int _event_timer_plan_is_free(const EventTimerPlan *plan)
{
    return plan->mode == EVENT_TIMER_MODE_UNUSED;
}

static void _event_clear_handler(EventHandler *handler)
{
    handler->id = EVENT_HANDLER_ID_INVALID;
    handler->event_id = EVENT_ID_INVALID;
    handler->handler = NULL;
    handler->user_data = NULL;
}

static void _event_clear_monitor(EventMonitor *monitor)
{
    monitor->scheduler = NULL;
    monitor->id = EVENT_MONITOR_ID_INVALID;
    monitor->event_id = EVENT_ID_INVALID;
    monitor->monitor = NULL;
    monitor->timer_id = TIMER_ID_INVALID;
    monitor->period_ticks = 0U;
    monitor->user_data = NULL;
}

static void _event_clear_timer_plan(EventTimerPlan *plan)
{
    plan->scheduler = NULL;
    plan->event_id = EVENT_ID_INVALID;
    plan->timer_id = TIMER_ID_INVALID;
    plan->mode = EVENT_TIMER_MODE_UNUSED;
    plan->value = 0U;
    plan->user_data = NULL;
}

static size_t _event_next_queue_index(size_t index)
{
    ++index;
    if (index >= EVENT_QUEUE_SIZE)
    {
        index = 0U;
    }

    return index;
}

static size_t _event_next_monitor_index(size_t index)
{
    ++index;
    if (index >= EVENT_MONITOR_POOL_SIZE)
    {
        index = 0U;
    }

    return index;
}

static EventHandlerId _event_next_handler_candidate_id(EventHandlerId id)
{
    if (id >= (EventHandlerId)(EVENT_HANDLER_ID_INVALID - 1U))
    {
        return 0U;
    }

    return id + 1U;
}

static EventMonitorId _event_next_monitor_candidate_id(EventMonitorId id)
{
    if (id >= (EventMonitorId)(EVENT_MONITOR_ID_INVALID - 1U))
    {
        return 0U;
    }

    return id + 1U;
}

static EventHandler *_event_find_handler(EventScheduler *self, EventHandlerId id)
{
    size_t i;

    for (i = 0U; i < EVENT_HANDLER_POOL_SIZE; ++i)
    {
        if (self->handlers[i].id == id)
        {
            return &self->handlers[i];
        }
    }

    return NULL;
}

static EventHandler *_event_find_free_handler(EventScheduler *self)
{
    size_t i;

    for (i = 0U; i < EVENT_HANDLER_POOL_SIZE; ++i)
    {
        if (_event_handler_is_free(&self->handlers[i]))
        {
            return &self->handlers[i];
        }
    }

    return NULL;
}

static EventMonitor *_event_find_monitor(EventScheduler *self, EventMonitorId id)
{
    size_t i;

    for (i = 0U; i < EVENT_MONITOR_POOL_SIZE; ++i)
    {
        if (self->monitors[i].id == id)
        {
            return &self->monitors[i];
        }
    }

    return NULL;
}

static EventMonitor *_event_find_free_monitor(EventScheduler *self)
{
    size_t i;

    for (i = 0U; i < EVENT_MONITOR_POOL_SIZE; ++i)
    {
        if (_event_monitor_is_free(&self->monitors[i]))
        {
            return &self->monitors[i];
        }
    }

    return NULL;
}

static EventTimerPlan *_event_find_timer_plan(EventScheduler *self, EventId event_id)
{
    size_t i;

    for (i = 0U; i < TIMER_POOL_SIZE; ++i)
    {
        if (_event_timer_plan_is_allocated(&self->timer_plans[i]) &&
            self->timer_plans[i].event_id == event_id)
        {
            return &self->timer_plans[i];
        }
    }

    return NULL;
}

static EventTimerPlan *_event_find_free_timer_plan(EventScheduler *self)
{
    size_t i;

    for (i = 0U; i < TIMER_POOL_SIZE; ++i)
    {
        if (_event_timer_plan_is_free(&self->timer_plans[i]))
        {
            return &self->timer_plans[i];
        }
    }

    return NULL;
}

static EventHandlerId _event_alloc_handler_id(EventScheduler *self)
{
    EventHandlerId candidate;
    EventHandlerId start;

    candidate = self->next_handler_id;
    start = candidate;

    do
    {
        if (candidate != EVENT_HANDLER_ID_INVALID &&
            _event_find_handler(self, candidate) == NULL)
        {
            self->next_handler_id = _event_next_handler_candidate_id(candidate);
            return candidate;
        }

        candidate = _event_next_handler_candidate_id(candidate);
    } while (candidate != start);

    return EVENT_HANDLER_ID_INVALID;
}

static EventMonitorId _event_alloc_monitor_id(EventScheduler *self)
{
    EventMonitorId candidate;
    EventMonitorId start;

    candidate = self->next_monitor_id;
    start = candidate;

    do
    {
        if (candidate != EVENT_MONITOR_ID_INVALID &&
            _event_find_monitor(self, candidate) == NULL)
        {
            self->next_monitor_id = _event_next_monitor_candidate_id(candidate);
            return candidate;
        }

        candidate = _event_next_monitor_candidate_id(candidate);
    } while (candidate != start);

    return EVENT_MONITOR_ID_INVALID;
}

static int _event_post_checked(EventScheduler *self, EventId event_id, EventSource source, uint32_t value, void *user_data)
{
    Event *event;

    if (self->queue_count >= EVENT_QUEUE_SIZE)
    {
        return -1;
    }

    event = &self->queue[self->queue_tail];
    event->id = event_id;
    event->source = source;
    event->value = value;
    event->user_data = user_data;

    self->queue_tail = _event_next_queue_index(self->queue_tail);
    ++self->queue_count;

    return 0;
}

static int _event_dispatch_checked(EventScheduler *self, const Event *event)
{
    size_t i;
    int count;
    EventHandler *handler;

    count = 0;

    for (i = 0U; i < EVENT_HANDLER_POOL_SIZE; ++i)
    {
        handler = &self->handlers[i];
        if (_event_handler_is_allocated(handler) && handler->event_id == event->id)
        {
            handler->handler(self, event, handler->user_data);
            ++count;
        }
    }

    return count;
}

static int _event_dispatch_next_checked(EventScheduler *self)
{
    Event event;
    int count;

    if (self->queue_count == 0U)
    {
        return 0;
    }

    event = self->queue[self->queue_head];
    self->queue_head = _event_next_queue_index(self->queue_head);
    --self->queue_count;

    count = _event_dispatch_checked(self, &event);

    return count;
}

static int _event_check_monitor_checked(EventScheduler *self, EventMonitor *monitor)
{
    EventId event_id;
    event_monitor_fn monitor_fn;
    uint32_t value;
    void *event_user_data;
    void *monitor_user_data;
    int fired;
    int result;

    event_id = monitor->event_id;
    monitor_fn = monitor->monitor;
    value = 0U;
    event_user_data = NULL;
    monitor_user_data = monitor->user_data;
    fired = monitor_fn(self, &value, &event_user_data, monitor_user_data);
    result = 0;

    if (fired)
    {
        result = _event_post_checked(self, event_id, WD_EVENT_SOURCE_MONITOR, value, event_user_data);
    }

    return result;
}

static int _event_check_next_poll_monitor_checked(EventScheduler *self)
{
    size_t i;
    size_t index;
    EventMonitor *monitor;
    int result;

    result = 0;

    for (i = 0U; i < EVENT_MONITOR_POOL_SIZE; ++i)
    {
        index = self->monitor_scan_index;
        self->monitor_scan_index = _event_next_monitor_index(self->monitor_scan_index);
        monitor = &self->monitors[index];

        if (_event_monitor_is_allocated(monitor) && monitor->period_ticks == 0U)
        {
            result = _event_check_monitor_checked(self, monitor);
            break;
        }
    }

    return result;
}

static void _event_timer_plan_action(TimerScheduler *timer_scheduler, TimerId id, void *user_data)
{
    EventTimerPlan *plan;
    EventScheduler *scheduler;
    uint8_t mode;
    EventId event_id;
    uint32_t value;
    void *event_user_data;

    (void)timer_scheduler;
    (void)id;

    plan = (EventTimerPlan *)user_data;
    scheduler = plan->scheduler;
    mode = plan->mode;
    event_id = plan->event_id;
    value = plan->value;
    event_user_data = plan->user_data;

    if (_event_post_checked(scheduler, event_id, WD_EVENT_SOURCE_TIMER, value, event_user_data) != 0)
    {
        scheduler->internal_error = -1;
    }

    if (mode == EVENT_TIMER_MODE_DELAY)
    {
        (void)timer_delete(&scheduler->timer, plan->timer_id);
        _event_clear_timer_plan(plan);
    }
}

static void _event_monitor_timer_action(TimerScheduler *timer_scheduler, TimerId id, void *user_data)
{
    EventMonitor *monitor;
    EventScheduler *scheduler;

    (void)timer_scheduler;
    (void)id;

    monitor = (EventMonitor *)user_data;
    scheduler = monitor->scheduler;

    if (_event_check_monitor_checked(scheduler, monitor) != 0)
    {
        scheduler->internal_error = -1;
    }
}

static EventTimerPlan *_event_alloc_timer_plan_checked(EventScheduler *self, EventId event_id)
{
    EventTimerPlan *plan;
    TimerId timer_id;

    plan = _event_find_free_timer_plan(self);
    if (plan == NULL)
    {
        return NULL;
    }

    timer_id = timer_new(&self->timer);
    if (timer_id == TIMER_ID_INVALID)
    {
        return NULL;
    }

    plan->scheduler = self;
    plan->event_id = event_id;
    plan->timer_id = timer_id;
    plan->mode = EVENT_TIMER_MODE_UNUSED;
    plan->value = 0U;
    plan->user_data = NULL;

    return plan;
}

static int _event_start_timer_plan_checked(EventScheduler *self, EventTimerPlan *plan, uint8_t mode, TimerTick delay_ticks, TimerTick period_ticks, uint32_t value, void *user_data)
{
    int result;

    result = timer_start(&self->timer, plan->timer_id, delay_ticks, period_ticks, _event_timer_plan_action, plan);
    if (result == 0)
    {
        plan->mode = mode;
        plan->value = value;
        plan->user_data = user_data;
    }

    return result;
}

static int _event_delete_timer_plan_checked(EventScheduler *self, EventTimerPlan *plan)
{
    int result;

    result = timer_delete(&self->timer, plan->timer_id);
    _event_clear_timer_plan(plan);

    return result;
}

static int _event_delete_monitor_checked(EventScheduler *self, EventMonitor *monitor)
{
    int result;

    result = 0;

    if (monitor->timer_id != TIMER_ID_INVALID)
    {
        result = timer_delete(&self->timer, monitor->timer_id);
    }

    _event_clear_monitor(monitor);

    return result;
}

void event_scheduler_init(EventScheduler *self, const TimerOps *timer_ops, void *timer_user_data)
{
    size_t i;

    WD_ASSERT(self != NULL);
    _event_assert_timer_ops(timer_ops);

    timer_scheduler_init(&self->timer, timer_ops, timer_user_data);

    self->queue_head = 0U;
    self->queue_tail = 0U;
    self->queue_count = 0U;
    self->next_handler_id = 0U;
    self->next_monitor_id = 0U;
    self->monitor_scan_index = 0U;
    self->internal_error = 0;

    for (i = 0U; i < EVENT_HANDLER_POOL_SIZE; ++i)
    {
        _event_clear_handler(&self->handlers[i]);
    }

    for (i = 0U; i < EVENT_MONITOR_POOL_SIZE; ++i)
    {
        _event_clear_monitor(&self->monitors[i]);
    }

    for (i = 0U; i < TIMER_POOL_SIZE; ++i)
    {
        _event_clear_timer_plan(&self->timer_plans[i]);
    }
}

int event_scheduler_run_once(EventScheduler *self)
{
    int result;

    _event_assert_ready(self);

    result = 0;

    if (_event_check_next_poll_monitor_checked(self) != 0)
    {
        result = -1;
    }

    self->internal_error = 0;
    (void)timer_scheduler_run_once(&self->timer);
    if (self->internal_error != 0)
    {
        result = -1;
    }

    (void)_event_dispatch_next_checked(self);

    return result;
}

TimerTick event_scheduler_next_delay(EventScheduler *self)
{
    _event_assert_ready(self);

    return timer_scheduler_next_delay(&self->timer);
}

EventHandlerId event_handler_add(EventScheduler *self, EventId event_id, event_handler_fn handler, void *user_data)
{
    EventHandler *slot;
    EventHandlerId id;

    _event_assert_ready(self);
    _event_assert_event_id(event_id);
    WD_ASSERT(handler != NULL);

    slot = _event_find_free_handler(self);
    if (slot == NULL)
    {
        return EVENT_HANDLER_ID_INVALID;
    }

    id = _event_alloc_handler_id(self);
    if (id == EVENT_HANDLER_ID_INVALID)
    {
        return EVENT_HANDLER_ID_INVALID;
    }

    slot->id = id;
    slot->event_id = event_id;
    slot->handler = handler;
    slot->user_data = user_data;

    return id;
}

int event_handler_remove(EventScheduler *self, EventHandlerId id)
{
    EventHandler *handler;
    int result;

    _event_assert_ready(self);
    WD_ASSERT(id != EVENT_HANDLER_ID_INVALID);

    result = -1;

    handler = _event_find_handler(self, id);
    if (handler != NULL)
    {
        _event_clear_handler(handler);
        result = 0;
    }

    return result;
}

int event_post(EventScheduler *self, EventId event_id, uint32_t value, void *user_data)
{
    _event_assert_ready(self);
    _event_assert_event_id(event_id);

    return _event_post_checked(self, event_id, WD_EVENT_SOURCE_EXTERNAL, value, user_data);
}

int event_trigger(EventScheduler *self, EventId event_id, uint32_t value, void *user_data)
{
    Event event;

    _event_assert_ready(self);
    _event_assert_event_id(event_id);

    event.id = event_id;
    event.source = WD_EVENT_SOURCE_EXTERNAL;
    event.value = value;
    event.user_data = user_data;

    return _event_dispatch_checked(self, &event);
}

int event_schedule_delay(EventScheduler *self, EventId event_id, TimerTick delay_ticks, uint32_t value, void *user_data)
{
    EventTimerPlan *plan;
    int allocated;
    int result;

    _event_assert_ready(self);
    _event_assert_event_id(event_id);
    WD_ASSERT(delay_ticks < _event_tick_half_range);

    allocated = 0;
    result = -1;

    plan = _event_find_timer_plan(self, event_id);
    if (plan == NULL)
    {
        plan = _event_alloc_timer_plan_checked(self, event_id);
        allocated = 1;
    }

    if (plan != NULL)
    {
        result = _event_start_timer_plan_checked(self, plan, EVENT_TIMER_MODE_DELAY, delay_ticks, 0U, value, user_data);
        if (result != 0 && allocated)
        {
            (void)_event_delete_timer_plan_checked(self, plan);
        }
    }

    return result;
}

int event_schedule_period(EventScheduler *self, EventId event_id, TimerTick period_ticks, uint32_t value, void *user_data)
{
    EventTimerPlan *plan;
    int allocated;
    int result;

    _event_assert_ready(self);
    _event_assert_event_id(event_id);
    WD_ASSERT(period_ticks != 0U);
    WD_ASSERT(period_ticks < _event_tick_half_range);

    allocated = 0;
    result = -1;

    plan = _event_find_timer_plan(self, event_id);
    if (plan == NULL)
    {
        plan = _event_alloc_timer_plan_checked(self, event_id);
        allocated = 1;
    }

    if (plan != NULL)
    {
        result = _event_start_timer_plan_checked(self, plan, EVENT_TIMER_MODE_PERIOD, period_ticks, period_ticks, value, user_data);
        if (result != 0 && allocated)
        {
            (void)_event_delete_timer_plan_checked(self, plan);
        }
    }

    return result;
}

int event_unschedule(EventScheduler *self, EventId event_id)
{
    EventTimerPlan *plan;
    int result;

    _event_assert_ready(self);
    _event_assert_event_id(event_id);

    result = -1;

    plan = _event_find_timer_plan(self, event_id);
    if (plan != NULL)
    {
        result = _event_delete_timer_plan_checked(self, plan);
    }

    return result;
}

EventMonitorId event_monitor_add(EventScheduler *self, EventId event_id, event_monitor_fn monitor, TimerTick period_ticks, void *user_data)
{
    EventMonitor *slot;
    EventMonitorId id;
    TimerId timer_id;
    int result;

    _event_assert_ready(self);
    _event_assert_event_id(event_id);
    WD_ASSERT(monitor != NULL);
    WD_ASSERT(period_ticks == 0U || period_ticks < _event_tick_half_range);

    slot = _event_find_free_monitor(self);
    if (slot == NULL)
    {
        return EVENT_MONITOR_ID_INVALID;
    }

    id = _event_alloc_monitor_id(self);
    if (id == EVENT_MONITOR_ID_INVALID)
    {
        return EVENT_MONITOR_ID_INVALID;
    }

    slot->scheduler = self;
    slot->id = id;
    slot->event_id = event_id;
    slot->monitor = monitor;
    slot->timer_id = TIMER_ID_INVALID;
    slot->period_ticks = period_ticks;
    slot->user_data = user_data;

    if (period_ticks != 0U)
    {
        timer_id = timer_new(&self->timer);
        if (timer_id == TIMER_ID_INVALID)
        {
            _event_clear_monitor(slot);
            return EVENT_MONITOR_ID_INVALID;
        }

        slot->timer_id = timer_id;
        result = timer_start(&self->timer, timer_id, period_ticks, period_ticks, _event_monitor_timer_action, slot);
        if (result != 0)
        {
            (void)timer_delete(&self->timer, timer_id);
            _event_clear_monitor(slot);
            return EVENT_MONITOR_ID_INVALID;
        }
    }

    return id;
}

int event_monitor_remove(EventScheduler *self, EventMonitorId id)
{
    EventMonitor *monitor;
    int result;

    _event_assert_ready(self);
    WD_ASSERT(id != EVENT_MONITOR_ID_INVALID);

    result = -1;

    monitor = _event_find_monitor(self, id);
    if (monitor != NULL)
    {
        result = _event_delete_monitor_checked(self, monitor);
    }

    return result;
}
