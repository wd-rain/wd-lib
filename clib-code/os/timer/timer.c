#include "timer.h"

static const TimerTick _timer_tick_half_range = (TimerTick)(UINT32_MAX / 2U + 1U);

static int _timer_tick_expired(TimerTick now, TimerTick deadline)
{
    /* 使用无符号差值处理 tick 回绕，比较范围限制在半个 uint32_t tick 内。 */
    return (TimerTick)(now - deadline) < _timer_tick_half_range;
}

static int _timer_deadline_before(TimerTick left, TimerTick right)
{
    return left != right && (TimerTick)(right - left) < _timer_tick_half_range;
}

static int _timer_is_firing(const Timer *timer)
{
    return timer->next == timer;
}

static int _timer_is_allocated(const Timer *timer)
{
    return timer->id != TIMER_ID_INVALID;
}

static int _timer_is_not_allocated(const Timer *timer)
{
    return timer->id == TIMER_ID_INVALID;
}

static TimerTick _timer_get_tick(const TimerScheduler *self)
{
    return self->ops->get_tick(self->ops_user_data);
}

static void _timer_assert_ops(const TimerOps *ops)
{
    WD_ASSERT(ops != NULL);
    WD_ASSERT(ops->get_tick != NULL);
}

static void _timer_assert_ready(const TimerScheduler *self)
{
    WD_ASSERT(self != NULL);
    _timer_assert_ops(self->ops);
}

static int _timer_is_active(const TimerScheduler *self, const Timer *timer)
{
    const Timer *current;

    /* 触发中的 timer 已离开活动链表，使用 next 自指作为内部运行标记。 */
    if (_timer_is_firing(timer))
    {
        return 1;
    }

    current = self->active_head;
    while (current != NULL)
    {
        if (current == timer)
        {
            return 1;
        }
        current = current->next;
    }

    return 0;
}

static TimerId _timer_next_candidate_id(TimerId id)
{
    if (id >= (TimerId)(TIMER_ID_INVALID - 1U))
    {
        return 0U;
    }

    return id + 1U;
}

static void _timer_clear(Timer *timer)
{
    timer->id = TIMER_ID_INVALID;
    timer->deadline = 0U;
    timer->period = 0U;
    timer->action = NULL;
    timer->user_data = NULL;
    timer->next = NULL;
}

static Timer *_timer_find(TimerScheduler *self, TimerId id)
{
    size_t i;
    for (i = 0U; i < TIMER_POOL_SIZE; ++i)
    {
        if (self->timers[i].id == id)
        {
            return &self->timers[i];
        }
    }
    return NULL;
}

static void _timer_assert_id(TimerId id)
{
    WD_ASSERT(id != TIMER_ID_INVALID);
}

static void _timer_assert_start_args(TimerId id, TimerTick delay_ticks, TimerTick period_ticks, timer_action_fn action)
{
    _timer_assert_id(id);
    WD_ASSERT(delay_ticks < _timer_tick_half_range);
    WD_ASSERT(period_ticks == 0U || period_ticks < _timer_tick_half_range);
    WD_ASSERT(action != NULL);
}

static Timer *_timer_find_free(TimerScheduler *self)
{
    size_t i;

    for (i = 0U; i < TIMER_POOL_SIZE; ++i)
    {
        if (_timer_is_not_allocated(&self->timers[i]))
        {
            return &self->timers[i];
        }
    }

    return NULL;
}

static TimerId _timer_alloc_id(TimerScheduler *self)
{
    TimerId candidate;
    candidate = self->next_id;
    do
    {
        self->next_id = _timer_next_candidate_id(self->next_id);
    } while (_timer_find(self, self->next_id));
    return candidate;
}

static void _timer_insert_active(TimerScheduler *self, Timer *timer)
{
    Timer *current;

    /* 活动链表按 deadline 从早到晚排序，run_once 只需要检查表头。 */
    if (self->active_head == NULL ||
        _timer_deadline_before(timer->deadline, self->active_head->deadline))
    {
        timer->next = self->active_head;
        self->active_head = timer;
        return;
    }

    current = self->active_head;
    while (current->next != NULL &&
           !_timer_deadline_before(timer->deadline, current->next->deadline))
    {
        current = current->next;
    }

    timer->next = current->next;
    current->next = timer;
}

static int _timer_stop_active(TimerScheduler *self, Timer *timer)
{
    Timer **current = &self->active_head;

    if (_timer_is_firing(timer))
    {
        /* callback 内 stop/delete 当前 timer 时清除触发中标记，阻止周期 timer 自动重插。 */
        timer->next = NULL;
        return 0;
    }

    while (*current)
    {
        if (*current == timer)
        {
            *current = timer->next;
            timer->next = NULL;
            return 0;
        }
        *current = (*current)->next;
    }
    return -1;
}

void timer_scheduler_init(TimerScheduler *self, const TimerOps *ops, void *ops_user_data)
{
    size_t i;

    WD_ASSERT(self != NULL);
    _timer_assert_ops(ops);

    self->ops = ops;
    self->ops_user_data = ops_user_data;
    self->active_head = NULL;
    self->next_id = 0U;

    for (i = 0U; i < TIMER_POOL_SIZE; ++i)
    {
        _timer_clear(&self->timers[i]);
    }
}

int timer_scheduler_run_once(TimerScheduler *self)
{
    Timer *timer;
    TimerId id = TIMER_ID_INVALID;
    TimerTick deadline = 0U;
    TimerTick now;
    TimerTick period = 0U;
    timer_action_fn action = NULL;
    void *user_data = NULL;
    int fired = 0;

    _timer_assert_ready(self);

    now = _timer_get_tick(self);
    timer = self->active_head;
    if (timer != NULL && _timer_tick_expired(now, timer->deadline))
    {
        /* 先摘下到期 timer 并保存回调数据，允许 action 内修改或删除该 timer。 */
        self->active_head = timer->next;
        id = timer->id;
        deadline = timer->deadline;
        period = timer->period;
        action = timer->action;
        user_data = timer->user_data;
        timer->next = timer;
        fired = 1;
    }

    if (!fired)
    {
        return 0;
    }

    if (action != NULL)
    {
        action(self, id, user_data);
    }

    if (_timer_is_firing(timer))
    {
        timer->next = NULL;
        if (_timer_is_allocated(timer) && timer->id == id && period != 0U)
        {
            /* action 未改变当前 timer 时，周期 timer 按原 deadline 推进一个周期。 */
            timer->deadline = deadline + period;
            _timer_insert_active(self, timer);
        }
    }
    return 0;
}

TimerTick timer_scheduler_now(TimerScheduler *self)
{
    _timer_assert_ready(self);
    return _timer_get_tick(self);
}

TimerTick timer_scheduler_next_delay(TimerScheduler *self)
{
    TimerTick now;
    TimerTick result;

    _timer_assert_ready(self);

    if (self->active_head == NULL)
    {
        result = (TimerTick)-1;
    }
    else
    {
        now = _timer_get_tick(self);
        if (_timer_tick_expired(now, self->active_head->deadline))
        {
            result = 0U;
        }
        else
        {
            result = self->active_head->deadline - now;
        }
    }

    return result;
}

TimerId timer_new(TimerScheduler *self)
{
    Timer *timer;
    TimerId id;

    _timer_assert_ready(self);

    timer = _timer_find_free(self);
    if (timer == NULL)
    {
        return TIMER_ID_INVALID;
    }

    id = _timer_alloc_id(self);
    if (id == TIMER_ID_INVALID)
    {
        return TIMER_ID_INVALID;
    }

    timer->id = id;
    timer->deadline = 0U;
    timer->period = 0U;
    timer->action = NULL;
    timer->user_data = NULL;
    timer->next = NULL;

    return id;
}

int timer_delete(TimerScheduler *self, TimerId id)
{
    Timer *timer;
    int result;

    _timer_assert_ready(self);
    _timer_assert_id(id);

    result = -1;

    timer = _timer_find(self, id);
    if (timer != NULL)
    {
        (void)_timer_stop_active(self, timer);
        _timer_clear(timer);
        result = 0;
    }

    return result;
}

int timer_start(TimerScheduler *self, TimerId id, TimerTick delay_ticks, TimerTick period_ticks, timer_action_fn action, void *user_data)
{
    Timer *timer;
    TimerTick now;

    _timer_assert_ready(self);
    _timer_assert_start_args(id, delay_ticks, period_ticks, action);

    now = _timer_get_tick(self);
    timer = _timer_find(self, id);
    if (timer == NULL)
    {
        return -1;
    }

    if (_timer_is_active(self, timer))
    {
        _timer_stop_active(self, timer);
    }

    timer->deadline = now + delay_ticks;
    timer->period = period_ticks;
    timer->action = action;
    timer->user_data = user_data;

    _timer_insert_active(self, timer);

    return 0;
}

int timer_stop(TimerScheduler *self, TimerId id)
{
    Timer *timer;
    int result;

    _timer_assert_ready(self);
    _timer_assert_id(id);

    result = -1;

    timer = _timer_find(self, id);
    if (timer != NULL)
    {
        result = _timer_stop_active(self, timer);
    }

    return result;
}

int timer_is_running(TimerScheduler *self, TimerId id)
{
    Timer *timer;
    int result;

    _timer_assert_ready(self);
    _timer_assert_id(id);

    result = -1;

    timer = _timer_find(self, id);
    if (timer != NULL)
    {
        result = _timer_is_active(self, timer) ? 1 : 0;
    }

    return result;
}

int timer_remaining(TimerScheduler *self, TimerId id)
{
    Timer *timer;
    TimerTick now;
    int result;

    _timer_assert_ready(self);
    _timer_assert_id(id);

    result = -1;

    timer = _timer_find(self, id);
    if (timer != NULL)
    {
        if (_timer_is_active(self, timer))
        {
            now = _timer_get_tick(self);
            if (_timer_tick_expired(now, timer->deadline))
            {
                result = 0;
            }
            else
            {
                result = (int)(timer->deadline - now);
            }
        }
    }
    return result;
}
