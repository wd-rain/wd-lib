---
aliases:
  - event
  - event.h
  - OS 事件调度
depends:
  - "[[timer]]"
tags:
  - c
  - clib
  - os
  - event
---

# event

`event` 是 `[[os]]` 目录下位于 `timer` 之上的事件调度模块。它负责把外部触发、monitor 和 timer 到期统一成事件，再按 `EventId` 分发给用户注册的 handler。`event` 依赖 `timer`，但 `timer` 不知道 event。

本文是设计文档，不表示当前仓库已经包含 `event.h` 或 `event.c` 的实现。

## 依赖关系

规划代码路径：

- `clib-code/os/event/event.h`
- `clib-code/os/event/event.c`

`event.h` 应直接包含 `../timer/timer.h`，并通过 `TimerScheduler` 承载内部定时能力。`timer.h` 已经包含 `until.h`，因此 `event.h` 不需要也不应该再直接包含 `until.h`；`event` 对 `until` 的依赖来自 timer 的传递依赖。`event` 不依赖 `slco`，也不包含任何协程语义。

`EventId` 与 `TimerId` 属于不同命名空间。即使两者底层整数值相同，也不表示同一个对象；event 公开 API 不暴露 `TimerId`，也不把内部 timer id 当作事件 id 使用。

## 配置覆盖

`event` 保留自己的配置命名空间，同时允许上层通过 event 的配置影响内部 timer：

| 配置                        | 作用                                    |
| ------------------------- | ------------------------------------- |
| `EVENT_QUEUE_SIZE`        | 事件队列容量                                |
| `EVENT_HANDLER_POOL_SIZE` | handler 注册池容量                         |
| `EVENT_MONITOR_POOL_SIZE` | monitor 注册池容量                         |
| `EVENT_TIMER_POOL_SIZE`   | event 内部 `TimerScheduler` 的 timer 池容量，用于定时事件和周期 monitor |

配置覆盖规则：

- 用户直接定义的底层配置优先级最高，例如 `TIMER_POOL_SIZE` 高于 `EVENT_TIMER_POOL_SIZE`。
- 如果用户只包含 `os/event/event.h`，并定义了 `EVENT_TIMER_POOL_SIZE`，则 event 头文件应在包含 `timer.h` 前把该值转发给 `TIMER_POOL_SIZE`。
- 如果 `timer.h` 已经先被包含，`TIMER_POOL_SIZE` 已经确定，后续再通过 `EVENT_TIMER_POOL_SIZE` 覆盖不会生效。
- 文档只约定配置转发语义，不要求实现绑定到某段固定预处理代码。

## 接口总览

| 类别      | 接口                           | 功能                     |
| ------- | ---------------------------- | ---------------------- |
| 类型      | `EventId`                    | 业务事件 id，用于发布和匹配        |
| 类型      | `EventHandlerId`             | handler 注册句柄           |
| 类型      | `EventMonitorId`             | monitor 注册句柄           |
| 类型      | `EventSource`                | 事件来源枚举                 |
| 类型      | `Event`                      | handler 接收的事件载荷        |
| 类型      | `EventScheduler`             | 事件调度器实例                |
| 回调      | `event_handler_fn`           | 事件 handler             |
| 回调      | `event_monitor_fn`           | 通用 monitor 检查函数        |
| 初始化     | `event_scheduler_init`       | 初始化事件调度器和内部 timer      |
| 调度      | `event_scheduler_run_once`   | 非阻塞推进一次事件调度            |
| 调度      | `event_scheduler_next_delay` | 查询内部 timer 最近到期延迟      |
| handler | `event_handler_add`          | 注册事件 handler           |
| handler | `event_handler_remove`       | 移除事件 handler           |
| 事件      | `event_post`                 | 按 `EventId` 将事件加入队列    |
| 事件      | `event_trigger`              | 按 `EventId` 立即同步触发事件分发 |
| timer   | `event_schedule_delay`       | 为指定 `EventId` 安排一次性延时触发 |
| timer   | `event_schedule_period`      | 为指定 `EventId` 安排周期触发 |
| timer   | `event_unschedule`           | 取消指定 `EventId` 的内部定时触发 |
| monitor | `event_monitor_add`          | 添加通用 monitor，并配置检查周期 |
| monitor | `event_monitor_remove`       | 移除 monitor             |

## 类型

### `EventId`

```c
typedef uint32_t EventId;
```

`EventId` 是用户或上层模块定义的业务事件 id。`event` 不解释具体业务含义，只按 id 匹配 handler、monitor 触发目标和协程等待条件。

`EVENT_ID_INVALID` 作为非法业务事件 id。公开发布、handler 注册、monitor 注册和定时触发接口都不得使用该值。

`EventId` 不是资源句柄，不由 handler 池、monitor 池或 timer 池分配。应用可以用宏或枚举集中定义自己的事件 id，例如 `EVENT_BUTTON`、`EVENT_TIMEOUT`。

### `EventHandlerId`

```c
typedef uint32_t EventHandlerId;
```

`EventHandlerId` 是 handler 注册句柄，只用于移除 handler。无效值规划为 `EVENT_HANDLER_ID_INVALID`，不得与 `EVENT_ID_INVALID` 混用语义。

### `EventMonitorId`

```c
typedef uint32_t EventMonitorId;
```

`EventMonitorId` 是 monitor 注册句柄，只用于移除 monitor。无效值规划为 `EVENT_MONITOR_ID_INVALID`。

### `EventSource`

```c
typedef enum event_source_t
{
    WD_EVENT_SOURCE_EXTERNAL = 0,
    WD_EVENT_SOURCE_TIMER,
    WD_EVENT_SOURCE_MONITOR
} EventSource;
```

`EventSource` 只表示事件从哪里产生，方便 handler 区分同一 `EventId` 下的不同来源。它是事件元数据，不参与 handler 匹配。

### `Event`

```c
typedef struct event_t
{
    EventId id;
    EventSource source;
    uint32_t value;
    void *user_data;
} Event;
```

字段含义：

| 字段 | 说明 |
|---|---|
| `id` | 业务事件 id |
| `source` | 事件来源，只作元数据 |
| `value` | 来源提供的数值载荷 |
| `user_data` | 用户指针载荷，可为 `NULL` |

`Event` 是 handler 接收的统一载荷。用户发布事件时不需要手动构造 `Event`，而是通过参数式 `event_post` 或 `event_trigger` 提供 `EventId`、`value` 和 `user_data`。

`user_data` 是可选载荷，允许为 `NULL`。handler 若需要长期保存数据，应自行保证该指针的生命周期。

### `event_handler_fn`

```c
typedef void (*event_handler_fn)(EventScheduler *scheduler, const Event *event, void *user_data);
```

handler 在事件被同步触发或从队列分发时调用。回调参数中的 `user_data` 来自 handler 注册接口，不是 `Event.user_data`。

### `event_monitor_fn`

```c
typedef int (*event_monitor_fn)(EventScheduler *scheduler, uint32_t *value, void **event_user_data, void *user_data);
```

monitor 每次被检查时调用该函数。返回 `0` 表示本次无事件；返回非 `0` 表示触发绑定的 `EventId`。

`value` 和 `event_user_data` 是事件载荷输出参数。触发事件时，回调应写入 `*value`；不需要指针载荷时可把 `*event_user_data` 写为 `NULL`。`user_data` 是 monitor 自己的长期上下文，边沿检测、阈值、滞回、组合条件等状态都应保存在这里。

monitor 的检查时机由注册时的 `period_ticks` 决定：`0U` 表示每次 `event_scheduler_run_once` 都有机会检查；非 `0U` 表示附着到 event 内部 timer，按指定周期到期后才检查。

### `EventScheduler`

`EventScheduler` 规划持有以下状态：

- 内部 `TimerScheduler timer`。
- 事件环形队列，元素为 `Event`。
- handler 池，记录 `EventHandlerId`、目标 `EventId`、回调和用户上下文。
- monitor 池，记录 `EventMonitorId`、目标 `EventId`、检查函数、检查周期、内部 timer 句柄和用户上下文。
- 内部定时计划表，维护 `EventId -> TimerId` 映射、一次性/周期模式，以及 timer 到期时需要发布的 `value`、`user_data`。
- 分配 handler 和 monitor 句柄所需的计数器或槽位状态。

`EventScheduler` 应保持静态分配友好，不使用堆内存。它不分配业务 `EventId`，只保存并匹配用户提供的 `EventId`。

## 接口

### `event_scheduler_init`

```c
void event_scheduler_init(EventScheduler *self, const TimerOps *timer_ops, void *timer_user_data);
```

初始化事件调度器和内部 timer 调度器。`self`、`timer_ops`、`timer_ops->get_tick` 必须有效，否则触发 `WD_ASSERT`。

### `event_scheduler_run_once`

```c
int event_scheduler_run_once(EventScheduler *self);
```

非阻塞推进一次事件调度。推荐顺序：

1. 检查一个或一组 `period_ticks == 0U` 的 monitor，并把满足条件的 `EventId` 投递到队列。
2. 调用内部 `timer_scheduler_run_once`，把到期 timer 转换成对应 `EventId` 的事件，或触发周期 monitor 的一次检查。
3. 从事件队列取出一个事件，并按 `event.id` 分发给匹配 handler。

每次调用至少推进一个小步，避免在 monitor 或 handler 很多时形成长时间阻塞。没有可处理事件时返回 `0`。

### `event_scheduler_next_delay`

```c
TimerTick event_scheduler_next_delay(EventScheduler *self);
```

返回内部 timer 距离最近到期事件或周期 monitor 检查的延迟。没有运行中的内部定时计划和周期 monitor 时返回 `(TimerTick)-1`。该接口不反映队列中已经存在的事件，也不反映 `period_ticks == 0U` 的 monitor。

### `event_handler_add`

```c
EventHandlerId event_handler_add(EventScheduler *self, EventId event_id, event_handler_fn handler, void *user_data);
```

注册一个事件 handler。`self` 和 `handler` 必须有效，`event_id` 不得为 `EVENT_ID_INVALID`。池满时返回 `EVENT_HANDLER_ID_INVALID`。

同一 `event_id` 可以注册多个 handler，分发时按注册顺序调用。handler 可以在回调内投递新事件，但不应并发调用同一个 scheduler。

### `event_handler_remove`

```c
int event_handler_remove(EventScheduler *self, EventHandlerId id);
```

移除指定 handler。`id` 不得为 `EVENT_HANDLER_ID_INVALID`；找不到 id 返回 `-1`。

### `event_post`

```c
int event_post(EventScheduler *self, EventId event_id, uint32_t value, void *user_data);
```

将来源为 `WD_EVENT_SOURCE_EXTERNAL` 的事件加入队列，由后续 `event_scheduler_run_once` 分发。`self` 必须有效，`event_id` 不得为 `EVENT_ID_INVALID`。队列满时返回 `-1`。

### `event_trigger`

```c
int event_trigger(EventScheduler *self, EventId event_id, uint32_t value, void *user_data);
```

立即同步分发来源为 `WD_EVENT_SOURCE_EXTERNAL` 的事件，不经过队列。没有匹配 handler 时返回 `0`；至少调用一个 handler 时返回调用数量。`self` 必须有效，`event_id` 不得为 `EVENT_ID_INVALID`。

### `event_schedule_delay`

```c
int event_schedule_delay(EventScheduler *self, EventId event_id, TimerTick delay_ticks, uint32_t value, void *user_data);
```

为指定 `event_id` 安排一次性延时触发。`self` 必须有效，`event_id` 不得为 `EVENT_ID_INVALID`；`delay_ticks` 的合法范围沿用 timer 模块约束。

v1 约定同一个 `EventId` 最多对应一个内部定时计划。重复调用 `event_schedule_delay` 时，不创建新的用户可见对象，而是更新该 `EventId` 对应的内部 timer、模式、`value` 和 `user_data`。如果该 `EventId` 原本是周期计划，则切换为一次性延时计划。

timer 到期时生成来源为 `WD_EVENT_SOURCE_TIMER`、id 为 `event_id` 的事件。一次性延时计划触发后释放或标记空闲其内部 timer 槽位。内部使用的 `TimerId` 只保存在 `EventScheduler` 的映射表中，不传给用户，也不写入 `Event`。

池满、内部 timer 池满或启动内部 timer 失败时返回 `-1`。

### `event_schedule_period`

```c
int event_schedule_period(EventScheduler *self, EventId event_id, TimerTick period_ticks, uint32_t value, void *user_data);
```

为指定 `event_id` 安排周期触发。`self` 必须有效，`event_id` 不得为 `EVENT_ID_INVALID`；`period_ticks` 的合法范围沿用 timer 模块约束。

首次触发发生在 `period_ticks` 之后，随后每隔 `period_ticks` 生成一次来源为 `WD_EVENT_SOURCE_TIMER`、id 为 `event_id` 的事件。重复调用 `event_schedule_period` 会更新该 `EventId` 对应的内部 timer、周期、`value` 和 `user_data`。如果该 `EventId` 原本是一次性延时计划，则切换为周期计划。

池满、内部 timer 池满或启动内部 timer 失败时返回 `-1`。

### `event_unschedule`

```c
int event_unschedule(EventScheduler *self, EventId event_id);
```

取消指定 `EventId` 对应的内部定时计划，并释放内部 timer 槽位。`self` 必须有效，`event_id` 不得为 `EVENT_ID_INVALID`。找不到定时计划时返回 `-1`。

### `event_monitor_add`

```c
EventMonitorId event_monitor_add(EventScheduler *self, EventId event_id, event_monitor_fn monitor, TimerTick period_ticks, void *user_data);
```

添加通用 monitor。`self` 和 `monitor` 必须有效，`event_id` 不得为 `EVENT_ID_INVALID`。monitor 池满时返回 `EVENT_MONITOR_ID_INVALID`。

`monitor` 返回非 `0` 时，scheduler 生成来源为 `WD_EVENT_SOURCE_MONITOR`、id 为 `event_id` 的事件。event 不内置条件保持、边沿判断或数值比较策略；这些状态由 monitor 的 `user_data` 自行维护。

`period_ticks` 控制检查周期：

- `period_ticks == 0U`：monitor 不占用内部 timer，每次 `event_scheduler_run_once` 都有机会被检查，适合非常轻量、需要低延迟的条件。
- `period_ticks != 0U`：monitor 占用一个内部 timer 槽位，timer 到期时才执行一次检查；周期越长，CPU 空转越少，但事件响应延迟越大。

周期 monitor 到期检查后，如果 monitor 返回 `0`，只重新安排下一次检查，不生成事件；如果返回非 `0`，先投递事件，再继续按周期检查。内部 `TimerId` 不暴露给用户。

### `event_monitor_remove`

```c
int event_monitor_remove(EventScheduler *self, EventMonitorId id);
```

移除 monitor。`id` 不得为 `EVENT_MONITOR_ID_INVALID`；找不到 id 返回 `-1`。如果该 monitor 附着了内部 timer，移除时必须同时释放内部 timer 槽位。

## 调度行为

- 外部立即触发通过 `event_trigger` 同步调用匹配 `EventId` 的 handler。
- 外部异步投递通过 `event_post` 进入队列，由 `event_scheduler_run_once` 分发。
- timer 到期时不直接调用业务 handler，而是由内部 timer action 生成对应 `EventId` 的事件；生成事件失败时应丢弃该次事件并让 timer 继续遵循原 timer 行为。
- `period_ticks == 0U` 的 monitor 由 `event_scheduler_run_once` 主动检查，适合低延迟但会随主循环频率消耗 CPU。
- `period_ticks != 0U` 的 monitor 附着到内部 timer，只有到期时才检查；主循环可以结合 `event_scheduler_next_delay` 休眠到最近到期点。
- `EventSource` 只表示来源元数据，不参与 handler 匹配。
- handler 执行期间允许投递新事件；新事件不会在当前 handler 调用栈中递归分发，除非用户显式调用 `event_trigger`。

## 错误处理

- 空 scheduler、空 handler、空 monitor 回调、非法 `EventId`、非法 handler id、非法 monitor id 使用 `WD_ASSERT` 暴露编程错误。
- 队列满、handler 池满、monitor 池满、timer 池满、定时计划不存在、周期 monitor 分配内部 timer 失败或内部 timer 操作失败返回失败值。
- `event_trigger` 没有匹配 handler 时返回 `0`，不视为错误。

## 单线程模型

`event` 沿用 timer 的单线程拥有模型。一个 `EventScheduler` 应由一个线程或一个主循环拥有，模块内部不提供锁或临界区。中断中若要触发事件，应由调用层保证 `event_post` 的并发安全，或先写入平台自己的 ISR-safe 缓冲区，再在主循环中投递到 event。

## 使用示例

### 外部立即触发和异步投递

```c
#define EVENT_BUTTON 1U

static void button_handler(EventScheduler *scheduler, const Event *event, void *user_data)
{
    (void)scheduler;
    (void)user_data;
    (void)event;
}

void example(EventScheduler *event_scheduler)
{
    event_handler_add(event_scheduler, EVENT_BUTTON, button_handler, NULL);

    event_trigger(event_scheduler, EVENT_BUTTON, 0U, NULL);
    event_post(event_scheduler, EVENT_BUTTON, 1U, NULL);
}
```

### timer 到期事件

```c
#define EVENT_TIMEOUT 2U
#define EVENT_HEARTBEAT 3U

void example_timer(EventScheduler *event_scheduler)
{
    event_schedule_delay(event_scheduler, EVENT_TIMEOUT, 100U, 0U, NULL);
    event_schedule_period(event_scheduler, EVENT_HEARTBEAT, 1000U, 0U, NULL);
    event_scheduler_run_once(event_scheduler);
}
```

### 通用 monitor

```c
#define EVENT_SENSOR_READY 4U
#define EVENT_LEVEL_CHANGE 5U

typedef struct level_monitor_t
{
    uint32_t last_value;
    uint32_t current_value;
} LevelMonitor;

static int sensor_ready(EventScheduler *scheduler, uint32_t *value, void **event_user_data, void *user_data)
{
    int *ready;

    (void)scheduler;

    ready = (int *)user_data;
    if (*ready == 0)
    {
        return 0;
    }

    *value = 1U;
    *event_user_data = NULL;
    return 1;
}

static int level_changed(EventScheduler *scheduler, uint32_t *value, void **event_user_data, void *user_data)
{
    LevelMonitor *monitor;
    uint32_t current;

    (void)scheduler;

    monitor = (LevelMonitor *)user_data;
    current = monitor->current_value;
    if (current == monitor->last_value)
    {
        return 0;
    }

    monitor->last_value = current;
    *value = current;
    *event_user_data = monitor;
    return 1;
}

void example_monitor(EventScheduler *event_scheduler)
{
    static int ready = 0;
    static LevelMonitor level_monitor = {0U, 0U};

    event_monitor_add(event_scheduler, EVENT_SENSOR_READY, sensor_ready, 0U, &ready);
    event_monitor_add(event_scheduler, EVENT_LEVEL_CHANGE, level_changed, 20U, &level_monitor);
}
```

## 检查

实现该设计时需要检查：

- `event.h` 自包含标准头和 `timer.h`；`until.h` 由 `timer.h` 传递包含，event 不重复直接包含。
- `event.h` 的配置转发发生在首次包含 `timer.h` 前。
- `EventId` 与 `TimerId` 不混用；event 公开 API 不暴露内部 `TimerId`。
- `event` 不提供用户管理 timer 生命周期的公开 API。
- 一次性定时事件使用 `event_schedule_delay`；周期定时事件使用 `event_schedule_period`，不使用组合参数表达两种模式。
- `period_ticks != 0U` 的 monitor 由内部 timer 驱动，`period_ticks == 0U` 的 monitor 才随 `run_once` 检查。
- `EventSource` 只作为元数据，不参与 handler 匹配。
- `EventScheduler` 可静态分配，不使用堆。
- 公开接口入口覆盖所有参数校验。
- 私有非 `_event_assert_*` 函数不包含断言或防御性参数校验。
- `event` 不包含 `slco.h`。
