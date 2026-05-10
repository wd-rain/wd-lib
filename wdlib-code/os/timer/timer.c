#include "timer.h"

static const TimerTick _timer_tick_half_range = (TimerTick)(UINT32_MAX / 2U + 1U);

// 判断now是否已经到期
static int _timer_tick_expired(TimerTick now, TimerTick deadline)
{
    /* 使用无符号差值处理 tick 回绕，比较范围限制在半个 uint32_t tick 内。 */
    return (TimerTick)(now - deadline) < _timer_tick_half_range;
}

// 判断left是否比right更早不包括相等
static int _timer_deadline_before(TimerTick left, TimerTick right)
{
    /* 用 right-left 判断 left 是否更早，和 expired 一样依赖半范围约束消除回绕歧义。 */
    return left != right && (TimerTick)(right - left) < _timer_tick_half_range;
}

// 触发中标记：next 自指表示当前 timer 已经离开活动链表，正在执行回调。
static inline void _timer_latch(Timer *timer)
{
    timer->next = timer;
}

// 判断是否触发
static inline int _timer_is_latched(const Timer *timer)
{
    return timer->next == timer;
}

// timer 无效判断
static inline int _timer_is_not_allocated(const Timer *timer)
{
    return timer->id == TIMER_ID_INVALID;
}

// 调用ops获取当前 tick
static inline TimerTick _timer_get_tick(const TimerScheduler *self)
{
    return self->ops->get_tick(self->ops_user_data);
}

// ops有效断言
static void _timer_assert_ops(const TimerOps *ops)
{
    WD_ASSERT(ops != NULL);
    WD_ASSERT(ops->get_tick != NULL);
}

// 调度器有效断言
static void _timer_assert_ready(const TimerScheduler *self)
{
    WD_ASSERT(self != NULL);
    _timer_assert_ops(self->ops);
}

// 定时器是否在活动链表中，触发中也算在活动中
static int _timer_is_active(const TimerScheduler *self, const Timer *timer)
{
    const Timer *current;

    /* 触发中的 timer 已离开活动链表，使用 next 自指作为内部运行标记。 */
    if (_timer_is_latched(timer))
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

// 找到下一个候选 ID，跳过保留的无效 ID
static TimerId _timer_next_candidate_id(TimerId id)
{
    // 跳过保留的无效 ID，避免分配出外部用来表示失败的句柄。
    if (id >= (TimerId)(TIMER_ID_INVALID - 1U))
    {
        return 0U;
    }

    return id + 1U;
}

// 清空定时器
static void _timer_clear(Timer *timer)
{
    timer->id = TIMER_ID_INVALID;
    timer->next = NULL;
}

// 找到指定 ID 的定时器
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

// 定时器 ID 断言
static void _timer_assert_id(TimerId id)
{
    WD_ASSERT(id != TIMER_ID_INVALID);
}

// 定时器start有效断言
static void _timer_assert_start_args(TimerId id, TimerTick delay_ticks, TimerTick period_ticks, timer_action_fn action)
{
    _timer_assert_id(id);
    WD_ASSERT(delay_ticks < _timer_tick_half_range);
    WD_ASSERT(period_ticks == 0U || period_ticks < _timer_tick_half_range);
    WD_ASSERT(action != NULL);
}

// 找到一个空闲定时器槽位
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

// 提供空闲id，推进 next_id 到下一个可用候选，保证分配的 id 不会重复。
static TimerId _timer_alloc_id(TimerScheduler *self)
{
    TimerId candidate;
    candidate = self->next_id;

    // candidate 已由上次推进确认未占用；这里为下一次分配提前找到可用候选 ID。
    do
    {
        self->next_id = _timer_next_candidate_id(self->next_id);
    } while (_timer_find(self, self->next_id));
    return candidate;
}

// 插入定时器
static void _timer_insert_active(TimerScheduler *self, Timer *timer)
{
    Timer *current;

    /* 活动链表按 deadline 从早到晚排序，run_once 只需要检查表头。 */
    if (self->active_head == NULL ||
        _timer_deadline_before(timer->deadline, self->active_head->deadline))
    {
        // 新 timer 比当前表头更早到期时，直接成为下一次 run_once 的检查对象。
        timer->next = self->active_head;
        self->active_head = timer;
        return;
    }

    current = self->active_head;
    // 找到第一个更晚到期的节点前插入，保持链表始终按 deadline 排序。
    while (current->next != NULL &&
           !_timer_deadline_before(timer->deadline, current->next->deadline))
    {
        current = current->next;
    }

    timer->next = current->next;
    current->next = timer;
}

// 从活动链表中删除指定 timer，返回是否成功找到并删除
static int _timer_stop_active(TimerScheduler *self, Timer *timer)
{
    Timer **current = &self->active_head;

    if (_timer_is_latched(timer))
    {
        /* callback 内 stop/delete 当前 timer 时清除触发中标记，阻止周期 timer 自动重插。 */
        timer->next = NULL;
        return 0;
    }

    while (*current)
    {
        if (*current == timer)
        {
            // 使用指向指针的游标，删除表头和中间节点时都能更新前驱链接。
            *current = timer->next;
            timer->next = NULL;
            return 0;
        }
        current = &(*current)->next;
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

    // 固定池槽位以无效 ID 表示空闲，初始化时统一进入可分配状态。
    for (i = 0U; i < TIMER_POOL_SIZE; ++i)
    {
        _timer_clear(&self->timers[i]);
    }
}

int timer_scheduler_run_once(TimerScheduler *self)
{
    Timer *timer;
    TimerTick now;

    _timer_assert_ready(self);

    now = _timer_get_tick(self);
    timer = self->active_head;
    if (timer != NULL && _timer_tick_expired(now, timer->deadline))
    {
        /* 先摘下到期 timer 并保存回调数据，允许 action 内修改或删除该 timer。 */
        self->active_head = timer->next;
        // action 允许调用 stop/delete/restart，因此后续必须重新检查触发中标记。
        _timer_latch(timer);
    }
    else
    {
        goto exit;
    }

    if (timer->action != NULL)
    {
        
        timer->action(self, timer->id, timer->user_data);
    }

    if (_timer_is_latched(timer))
    {
        // 仍保持触发中表示 action 没有接管该 timer，可按保存的快照处理周期重插。
        timer->next = NULL;
        if (timer->period != 0U)
        {
            timer->deadline = timer->deadline + timer->period;
            _timer_insert_active(self, timer);
        }
    }
    exit:
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
        // 重启运行中的 timer 时先摘除旧节点，避免同一槽位在活动链表中出现两次。
        (void)_timer_stop_active(self, timer);
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

    _timer_assert_ready(self);
    _timer_assert_id(id);

    timer = _timer_find(self, id);
    if (timer != NULL)
    {
        return (_timer_is_latched(timer) | _timer_is_active(self, timer)) ? 1 : 0;
    }

    return -1;
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
