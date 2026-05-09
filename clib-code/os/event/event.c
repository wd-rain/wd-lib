#include "event.h"

#define EVENT_POST_STATE_UNUSED 0U
#define EVENT_POST_STATE_READY 1U
#define EVENT_POST_STATE_DELAY 2U

static const TimerTick _event_tick_half_range = (TimerTick)(UINT32_MAX / 2U + 1U);

static EventHandler *_event_find_handler(EventScheduler *self, EventId id);
static void _event_post_timer_action(TimerScheduler *timer_scheduler, TimerId id, void *user_data);
static void _event_timer_action(TimerScheduler *timer_scheduler, TimerId id, void *user_data);

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

static void _event_assert_allocated_event(EventScheduler *self, EventId id)
{
    _event_assert_event_id(id);
    WD_ASSERT(_event_find_handler(self, id) != NULL);
}

static int _event_handler_is_allocated(const EventHandler *handler)
{
    return handler->id != EVENT_ID_INVALID;
}

static int _event_handler_is_free(const EventHandler *handler)
{
    return handler->id == EVENT_ID_INVALID;
}

static int _event_monitor_is_allocated(const EventMonitor *monitor)
{
    return monitor->id != EVENT_MONITOR_ID_INVALID;
}

static int _event_monitor_is_free(const EventMonitor *monitor)
{
    return monitor->id == EVENT_MONITOR_ID_INVALID;
}

static int _event_post_is_allocated(const EventPost *post)
{
    return post->state != EVENT_POST_STATE_UNUSED;
}

static int _event_post_is_free(const EventPost *post)
{
    return post->state == EVENT_POST_STATE_UNUSED;
}

static int _event_timer_is_allocated(const EventTimer *event_timer)
{
    return event_timer->id != EVENT_TIMER_ID_INVALID;
}

static int _event_timer_is_free(const EventTimer *event_timer)
{
    return event_timer->id == EVENT_TIMER_ID_INVALID;
}

static void _event_clear_handler(EventHandler *handler)
{
    handler->id = EVENT_ID_INVALID;
    handler->handler = NULL;
    handler->user_data = NULL;
}

static void _event_clear_post(EventPost *post)
{
    post->scheduler = NULL;
    post->event_id = EVENT_ID_INVALID;
    post->timer_id = TIMER_ID_INVALID;
    post->source = WD_EVENT_SOURCE_EXTERNAL;
    post->state = EVENT_POST_STATE_UNUSED;
    post->value = 0U;
    post->user_data = NULL;
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

static void _event_clear_event_timer(EventTimer *event_timer)
{
    event_timer->scheduler = NULL;
    event_timer->id = EVENT_TIMER_ID_INVALID;
    event_timer->event_id = EVENT_ID_INVALID;
    event_timer->timer_id = TIMER_ID_INVALID;
    event_timer->value = 0U;
    event_timer->user_data = NULL;
}

static size_t _event_next_post_index(size_t index)
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

static EventId _event_next_event_candidate_id(EventId id)
{
    if (id >= (EventId)(EVENT_ID_INVALID - 1U))
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

static EventTimerId _event_next_timer_candidate_id(EventTimerId id)
{
    if (id >= (EventTimerId)(EVENT_TIMER_ID_INVALID - 1U))
    {
        return 0U;
    }

    return id + 1U;
}

static EventHandler *_event_find_handler(EventScheduler *self, EventId id)
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

static EventPost *_event_find_post(EventScheduler *self, EventId event_id)
{
    size_t i;

    for (i = 0U; i < EVENT_QUEUE_SIZE; ++i)
    {
        if (_event_post_is_allocated(&self->posts[i]) &&
            self->posts[i].event_id == event_id)
        {
            return &self->posts[i];
        }
    }

    return NULL;
}

static EventPost *_event_find_free_post(EventScheduler *self)
{
    size_t i;

    for (i = 0U; i < EVENT_QUEUE_SIZE; ++i)
    {
        if (_event_post_is_free(&self->posts[i]))
        {
            return &self->posts[i];
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

static EventTimer *_event_find_event_timer(EventScheduler *self, EventTimerId id)
{
    size_t i;

    for (i = 0U; i < TIMER_POOL_SIZE; ++i)
    {
        if (self->event_timers[i].id == id)
        {
            return &self->event_timers[i];
        }
    }

    return NULL;
}

static EventTimer *_event_find_free_event_timer(EventScheduler *self)
{
    size_t i;

    for (i = 0U; i < TIMER_POOL_SIZE; ++i)
    {
        if (_event_timer_is_free(&self->event_timers[i]))
        {
            return &self->event_timers[i];
        }
    }

    return NULL;
}

static int _event_event_id_in_use(EventScheduler *self, EventId id)
{
    size_t i;

    if (_event_find_handler(self, id) != NULL ||
        _event_find_post(self, id) != NULL)
    {
        return 1;
    }

    for (i = 0U; i < TIMER_POOL_SIZE; ++i)
    {
        if (_event_timer_is_allocated(&self->event_timers[i]) &&
            self->event_timers[i].event_id == id)
        {
            return 1;
        }
    }

    for (i = 0U; i < EVENT_MONITOR_POOL_SIZE; ++i)
    {
        if (_event_monitor_is_allocated(&self->monitors[i]) &&
            self->monitors[i].event_id == id)
        {
            return 1;
        }
    }

    return 0;
}

static EventId _event_alloc_event_id(EventScheduler *self)
{
    EventId candidate;
    EventId start;

    candidate = self->next_event_id;
    start = candidate;

    do
    {
        if (candidate != EVENT_ID_INVALID &&
            !_event_event_id_in_use(self, candidate))
        {
            self->next_event_id = _event_next_event_candidate_id(candidate);
            return candidate;
        }

        candidate = _event_next_event_candidate_id(candidate);
    } while (candidate != start);

    return EVENT_ID_INVALID;
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

static EventTimerId _event_alloc_timer_id(EventScheduler *self)
{
    EventTimerId candidate;
    EventTimerId start;

    candidate = self->next_timer_id;
    start = candidate;

    do
    {
        if (candidate != EVENT_TIMER_ID_INVALID &&
            _event_find_event_timer(self, candidate) == NULL)
        {
            self->next_timer_id = _event_next_timer_candidate_id(candidate);
            return candidate;
        }

        candidate = _event_next_timer_candidate_id(candidate);
    } while (candidate != start);

    return EVENT_TIMER_ID_INVALID;
}

static int _event_delete_post_timer_checked(EventScheduler *self, EventPost *post)
{
    int result;

    result = 0;

    if (post->timer_id != TIMER_ID_INVALID)
    {
        result = timer_delete(&self->timer, post->timer_id);
        if (result == 0)
        {
            post->timer_id = TIMER_ID_INVALID;
        }
    }

    return result;
}

static int _event_delete_post_checked(EventScheduler *self, EventPost *post)
{
    int result;

    result = _event_delete_post_timer_checked(self, post);
    if (result == 0)
    {
        _event_clear_post(post);
    }

    return result;
}

static void _event_fill_post_checked(EventPost *post, EventScheduler *self, EventId event_id, EventSource source, uint8_t state, uint32_t value, void *user_data)
{
    post->scheduler = self;
    post->event_id = event_id;
    post->source = source;
    post->state = state;
    post->value = value;
    post->user_data = user_data;
}

static int _event_post_checked(EventScheduler *self, EventId event_id, EventSource source, TimerTick delay_ticks, uint32_t value, void *user_data)
{
    EventPost *post;
    TimerId timer_id;
    int allocated;
    int timer_allocated;
    int result;

    allocated = 0;
    timer_allocated = 0;
    result = -1;

    post = _event_find_post(self, event_id);
    if (post == NULL)
    {
        post = _event_find_free_post(self);
        if (post == NULL)
        {
            return -1;
        }
        _event_clear_post(post);
        allocated = 1;
    }

    if (delay_ticks == 0U)
    {
        // 立即 post 会取消同一 EventId 尚未到期的延时 post，并用新内容覆盖。
        if (_event_delete_post_timer_checked(self, post) != 0)
        {
            if (allocated)
            {
                _event_clear_post(post);
            }
            return -1;
        }

        _event_fill_post_checked(post, self, event_id, source, EVENT_POST_STATE_READY, value, user_data);
        return 0;
    }

    if (post->timer_id == TIMER_ID_INVALID)
    {
        timer_id = timer_new(&self->timer);
        if (timer_id == TIMER_ID_INVALID)
        {
            if (allocated)
            {
                _event_clear_post(post);
            }
            return -1;
        }
        timer_allocated = 1;
    }
    else
    {
        timer_id = post->timer_id;
    }

    result = timer_start(&self->timer, timer_id, delay_ticks, 0U, _event_post_timer_action, post);
    if (result != 0)
    {
        if (timer_allocated)
        {
            (void)timer_delete(&self->timer, timer_id);
        }
        if (allocated)
        {
            _event_clear_post(post);
        }
        return -1;
    }

    post->timer_id = timer_id;
    _event_fill_post_checked(post, self, event_id, source, EVENT_POST_STATE_DELAY, value, user_data);

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
        if (_event_handler_is_allocated(handler) && handler->id == event->id)
        {
            handler->handler(self, event, handler->user_data);
            ++count;
        }
    }

    return count;
}

static int _event_trigger_checked(EventScheduler *self, EventId event_id, EventSource source, uint32_t value, void *user_data)
{
    Event event;

    event.id = event_id;
    event.source = source;
    event.value = value;
    event.user_data = user_data;

    return _event_dispatch_checked(self, &event);
}

static int _event_dispatch_next_checked(EventScheduler *self)
{
    Event event;
    EventPost *post;
    size_t i;
    size_t index;
    int count;

    count = 0;

    for (i = 0U; i < EVENT_QUEUE_SIZE; ++i)
    {
        index = self->post_scan_index;
        self->post_scan_index = _event_next_post_index(self->post_scan_index);
        post = &self->posts[index];

        if (_event_post_is_allocated(post) && post->state == EVENT_POST_STATE_READY)
        {
            event.id = post->event_id;
            event.source = post->source;
            event.value = post->value;
            event.user_data = post->user_data;
            _event_clear_post(post);
            count = _event_dispatch_checked(self, &event);
            break;
        }
    }

    return count;
}

static int _event_check_monitor_checked(EventScheduler *self, EventMonitor *monitor)
{
    EventId event_id;
    event_monitor_fn monitor_fn;
    uint32_t value;
    void *event_user_data;
    void *monitor_user_data;
    int result;

    event_id = monitor->event_id;
    monitor_fn = monitor->monitor;
    event_user_data = NULL;
    monitor_user_data = monitor->user_data;
    value = monitor_fn(self, &event_user_data, monitor_user_data);
    result = 0;

    if (value != 0U)
    {
        result = _event_post_checked(self, event_id, WD_EVENT_SOURCE_MONITOR, 0U, value, event_user_data);
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

static void _event_post_timer_action(TimerScheduler *timer_scheduler, TimerId id, void *user_data)
{
    EventPost *post;
    EventScheduler *scheduler;

    (void)timer_scheduler;

    post = (EventPost *)user_data;
    scheduler = post->scheduler;

    // 延时 post 到期后只把已有 post 内容切换为待分发，并释放内部 timer 槽位。
    if (timer_delete(&scheduler->timer, id) != 0)
    {
        scheduler->internal_error = -1;
    }
    post->timer_id = TIMER_ID_INVALID;
    post->state = EVENT_POST_STATE_READY;
}

static void _event_timer_action(TimerScheduler *timer_scheduler, TimerId id, void *user_data)
{
    EventTimer *event_timer;
    EventScheduler *scheduler;

    (void)timer_scheduler;
    (void)id;

    event_timer = (EventTimer *)user_data;
    scheduler = event_timer->scheduler;

    // 周期任务到期时同步触发 handler，不占用 pending post 槽位。
    (void)_event_trigger_checked(scheduler, event_timer->event_id, WD_EVENT_SOURCE_TIMER, event_timer->value, event_timer->user_data);
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

static EventTimer *_event_alloc_event_timer_checked(EventScheduler *self, EventId event_id, EventTimerId id)
{
    EventTimer *event_timer;
    TimerId timer_id;

    event_timer = _event_find_free_event_timer(self);
    if (event_timer == NULL)
    {
        return NULL;
    }

    timer_id = timer_new(&self->timer);
    if (timer_id == TIMER_ID_INVALID)
    {
        return NULL;
    }

    event_timer->scheduler = self;
    event_timer->id = id;
    event_timer->event_id = event_id;
    event_timer->timer_id = timer_id;
    event_timer->value = 0U;
    event_timer->user_data = NULL;

    return event_timer;
}

static int _event_start_event_timer_checked(EventScheduler *self, EventTimer *event_timer, TimerTick period_ticks, uint32_t value, void *user_data)
{
    int result;

    result = timer_start(&self->timer, event_timer->timer_id, period_ticks, period_ticks, _event_timer_action, event_timer);
    if (result == 0)
    {
        event_timer->value = value;
        event_timer->user_data = user_data;
    }

    return result;
}

static int _event_delete_event_timer_checked(EventScheduler *self, EventTimer *event_timer)
{
    int result;

    result = timer_delete(&self->timer, event_timer->timer_id);
    _event_clear_event_timer(event_timer);

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

static int _event_delete_monitors_by_event_checked(EventScheduler *self, EventId event_id)
{
    size_t i;
    int result;

    result = 0;

    for (i = 0U; i < EVENT_MONITOR_POOL_SIZE; ++i)
    {
        if (_event_monitor_is_allocated(&self->monitors[i]) &&
            self->monitors[i].event_id == event_id &&
            _event_delete_monitor_checked(self, &self->monitors[i]) != 0)
        {
            result = -1;
        }
    }

    return result;
}

static int _event_delete_timers_by_event_checked(EventScheduler *self, EventId event_id)
{
    size_t i;
    int result;

    result = 0;

    for (i = 0U; i < TIMER_POOL_SIZE; ++i)
    {
        if (_event_timer_is_allocated(&self->event_timers[i]) &&
            self->event_timers[i].event_id == event_id &&
            _event_delete_event_timer_checked(self, &self->event_timers[i]) != 0)
        {
            result = -1;
        }
    }

    return result;
}

static int _event_delete_post_by_event_checked(EventScheduler *self, EventId event_id)
{
    EventPost *post;
    int result;

    result = 0;

    post = _event_find_post(self, event_id);
    if (post != NULL)
    {
        result = _event_delete_post_checked(self, post);
    }

    return result;
}

static int _event_delete_event_checked(EventScheduler *self, EventHandler *handler)
{
    EventId event_id;
    int result;

    event_id = handler->id;
    result = 0;

    if (_event_delete_timers_by_event_checked(self, event_id) != 0)
    {
        result = -1;
    }

    if (_event_delete_monitors_by_event_checked(self, event_id) != 0)
    {
        result = -1;
    }

    if (_event_delete_post_by_event_checked(self, event_id) != 0)
    {
        result = -1;
    }

    _event_clear_handler(handler);

    return result;
}

void event_scheduler_init(EventScheduler *self, const TimerOps *timer_ops, void *timer_user_data)
{
    size_t i;

    WD_ASSERT(self != NULL);
    _event_assert_timer_ops(timer_ops);

    timer_scheduler_init(&self->timer, timer_ops, timer_user_data);

    self->next_event_id = 0U;
    self->next_monitor_id = 0U;
    self->next_timer_id = 0U;
    self->post_scan_index = 0U;
    self->monitor_scan_index = 0U;
    self->internal_error = 0;

    // 初始化公开接口负责把内部 timer 和各个静态资源池全部恢复为空状态。
    // 资源池使用无效 ID 表示空槽，保持静态分配、无堆内存。
    for (i = 0U; i < EVENT_QUEUE_SIZE; ++i)
    {
        _event_clear_post(&self->posts[i]);
    }

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
        _event_clear_event_timer(&self->event_timers[i]);
    }
}

int event_scheduler_run_once(EventScheduler *self)
{
    int result;

    _event_assert_ready(self);

    result = 0;

    // 每轮只推进小步：检查一个 poll monitor、处理一个内部 timer、分发一个待投递事件。
    if (_event_check_next_poll_monitor_checked(self) != 0)
    {
        result = -1;
    }

    self->internal_error = 0;
    // timer 回调只能通过 internal_error 把 monitor/post 失败带回这个公开接口。
    (void)timer_scheduler_run_once(&self->timer);
    if (self->internal_error != 0)
    {
        result = -1;
    }

    // 待投递事件延迟到 run_once 末尾分发，handler 内 post 的新内容留到后续轮次。
    (void)_event_dispatch_next_checked(self);

    return result;
}

TimerTick event_scheduler_next_delay(EventScheduler *self)
{
    _event_assert_ready(self);

    return timer_scheduler_next_delay(&self->timer);
}

EventId event_new(EventScheduler *self, event_handler_fn handler, void *user_data)
{
    EventHandler *slot;
    EventId id;

    _event_assert_ready(self);
    WD_ASSERT(handler != NULL);

    slot = _event_find_free_handler(self);
    if (slot == NULL)
    {
        return EVENT_ID_INVALID;
    }

    id = _event_alloc_event_id(self);
    if (id == EVENT_ID_INVALID)
    {
        return EVENT_ID_INVALID;
    }

    // EventId 由调度器分配，用户保存该 id 后再用于 post、trigger、timer 和 monitor。
    slot->id = id;
    slot->handler = handler;
    slot->user_data = user_data;

    return id;
}

int event_delete(EventScheduler *self, EventId id)
{
    EventHandler *handler;
    int result;

    _event_assert_ready(self);
    _event_assert_event_id(id);

    result = -1;

    handler = _event_find_handler(self, id);
    if (handler != NULL)
    {
        // 删除事件时同步释放该 EventId 关联的 timer、monitor 和待投递 post，避免 id 复用后收到旧事件。
        result = _event_delete_event_checked(self, handler);
    }

    return result;
}

int event_post(EventScheduler *self, EventId event_id, TimerTick delay_ticks, uint32_t value, void *user_data)
{
    _event_assert_ready(self);
    _event_assert_allocated_event(self, event_id);
    WD_ASSERT(delay_ticks < _event_tick_half_range);

    // delay 为 0 时立即进入待分发状态，非 0 时刷新同一 EventId 的延时 post。
    return _event_post_checked(self, event_id, WD_EVENT_SOURCE_EXTERNAL, delay_ticks, value, user_data);
}

int event_post_delay(EventScheduler *self, EventId event_id, TimerTick delay_ticks, uint32_t value, void *user_data)
{
    _event_assert_ready(self);
    _event_assert_allocated_event(self, event_id);
    WD_ASSERT(delay_ticks < _event_tick_half_range);

    return event_post(self, event_id, delay_ticks, value, user_data);
}

uint32_t event_is_posted(EventScheduler *self, EventId event_id)
{
    EventPost *post;

    _event_assert_ready(self);
    _event_assert_allocated_event(self, event_id);

    post = _event_find_post(self, event_id);
    if (post == NULL)
    {
        return 0U;
    }

    return post->value;
}

int event_trigger(EventScheduler *self, EventId event_id, uint32_t value, void *user_data)
{
    _event_assert_ready(self);
    _event_assert_allocated_event(self, event_id);

    // trigger 是同步接口：绕过 pending post，当前调用栈内立即匹配并执行 handler。
    return _event_trigger_checked(self, event_id, WD_EVENT_SOURCE_EXTERNAL, value, user_data);
}

EventTimerId event_timer_add(EventScheduler *self, EventId event_id, TimerTick period_ticks, uint32_t value, void *user_data)
{
    EventTimer *event_timer;
    EventTimerId id;
    int result;

    _event_assert_ready(self);
    _event_assert_allocated_event(self, event_id);
    WD_ASSERT(period_ticks != 0U);
    WD_ASSERT(period_ticks < _event_tick_half_range);

    id = _event_alloc_timer_id(self);
    if (id == EVENT_TIMER_ID_INVALID)
    {
        return EVENT_TIMER_ID_INVALID;
    }

    // 每次添加都分配独立的周期任务句柄，同一个 EventId 可以被多个内部 timer 投递。
    event_timer = _event_alloc_event_timer_checked(self, event_id, id);
    if (event_timer == NULL)
    {
        return EVENT_TIMER_ID_INVALID;
    }

    result = _event_start_event_timer_checked(self, event_timer, period_ticks, value, user_data);
    if (result != 0)
    {
        // 启动失败时释放刚取得的内部 timer，避免留下不可删除的半初始化任务。
        (void)_event_delete_event_timer_checked(self, event_timer);
        return EVENT_TIMER_ID_INVALID;
    }

    return id;
}

int event_timer_remove(EventScheduler *self, EventTimerId id)
{
    EventTimer *event_timer;
    int result;

    _event_assert_ready(self);
    WD_ASSERT(id != EVENT_TIMER_ID_INVALID);

    result = -1;

    event_timer = _event_find_event_timer(self, id);
    if (event_timer != NULL)
    {
        // 删除周期任务时释放它独占的内部 timer，不影响同一 EventId 上的其他任务。
        result = _event_delete_event_timer_checked(self, event_timer);
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
    _event_assert_allocated_event(self, event_id);
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
        // 非零周期 monitor 由内部 timer 驱动；0 周期 monitor 则由 run_once 轮询。
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
        // 移除 monitor 时同时释放它可能持有的内部 timer。
        result = _event_delete_monitor_checked(self, monitor);
    }

    return result;
}
