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

`event` 是 `[[os]]` 目录下位于 `timer` 之上的事件调度模块。它负责把外部触发、延时 post、event timer、monitor 和 timer 到期统一成事件，再按 `EventId` 分发给用户注册的 handler。`event` 依赖 `timer`，但 `timer` 不知道 event。

本文描述 event 库的设计和实现约束。

## 依赖关系

规划代码路径：

- `clib-code/os/event/event.h`
- `clib-code/os/event/event.c`

`event.h` 应直接包含 `../timer/timer.h`，并通过 `TimerScheduler` 承载内部定时能力。`timer.h` 已经包含 `until.h`，因此 `event.h` 不需要也不应该再直接包含 `until.h`；`event` 对 `until` 的依赖来自 timer 的传递依赖。`event` 不依赖 `slco`，也不包含任何协程语义。

`EventId` 与 `TimerId` 属于不同命名空间。即使两者底层整数值相同，也不表示同一个对象；event 公开 API 不暴露 `TimerId`，也不把内部 timer id 当作事件 id 使用。

## 配置覆盖

`event` 保留自己的配置命名空间，同时允许上层通过 event 的配置影响内部 timer：

| 配置                        | 作用                                                                      |
| ------------------------- | ----------------------------------------------------------------------- |
| `EVENT_QUEUE_SIZE`        | pending post 池容量，同一个 `EventId` 最多占用一个槽位                                 |
| `EVENT_HANDLER_POOL_SIZE` | handler 注册池容量                                                           |
| `EVENT_MONITOR_POOL_SIZE` | monitor 注册池容量                                                           |
| `EVENT_TIMER_POOL_SIZE`   | event 内部 `TimerScheduler` 的 timer 池容量，用于延时 post、event timer 和周期 monitor |

配置覆盖规则：

- 用户直接定义的底层配置优先级最高，例如 `TIMER_POOL_SIZE` 高于 `EVENT_TIMER_POOL_SIZE`。
- 如果用户只包含 `os/event/event.h`，并定义了 `EVENT_TIMER_POOL_SIZE`，则 event 头文件应在包含 `timer.h` 前把该值转发给 `TIMER_POOL_SIZE`。
- 如果 `timer.h` 已经先被包含，`TIMER_POOL_SIZE` 已经确定，后续再通过 `EVENT_TIMER_POOL_SIZE` 覆盖不会生效。
- 文档只约定配置转发语义，不要求实现绑定到某段固定预处理代码。

## 接口总览

| 类别      | 接口                           | 功能                     |
| ------- | ---------------------------- | ---------------------- |
| 类型      | `EventId`                    | 调度器分配的事件 id，用于发布和匹配   |
| 类型      | `EventMonitorId`             | monitor 注册句柄           |
| 类型      | `EventTimerId`               | event timer 注册句柄       |
| 类型      | `EventSource`                | 事件来源枚举                 |
| 类型      | `Event`                      | handler 接收的事件载荷        |
| 类型      | `EventScheduler`             | 事件调度器实例                |
| 回调      | `event_handler_fn`           | 事件 handler             |
| 回调      | `event_monitor_fn`           | 通用 monitor 检查函数        |
| 初始化     | `event_scheduler_init`       | 初始化事件调度器和内部 timer      |
| 调度      | `event_scheduler_run_once`   | 非阻塞推进一次事件调度            |
| 调度      | `event_scheduler_next_delay` | 查询内部 timer 最近到期延迟      |
| 事件      | `event_new`                  | 创建事件并注册 handler        |
| 事件      | `event_delete`               | 删除事件及其关联资源            |
| 事件      | `event_post`                 | 按 `EventId` 投递或延时投递事件 |
| 事件      | `event_post_delay`           | `event_post` 的延时投递别名 |
| 事件      | `event_is_posted`            | 读取指定事件当前 pending post 的 value |
| 事件      | `event_trigger`              | 按 `EventId` 立即同步触发事件分发 |
| timer   | `event_timer_add`            | 为指定 `EventId` 添加周期投递任务 |
| timer   | `event_timer_remove`         | 按 `EventTimerId` 删除周期投递任务 |
| monitor | `event_monitor_add`          | 添加通用 monitor，并配置检查周期 |
| monitor | `event_monitor_remove`       | 移除 monitor             |

## 类型

### `EventId`

```c
typedef uint32_t EventId;
```

`EventId` 是由 `EventScheduler` 分配的事件句柄。用户通过 `event_new` 创建事件并获得 id，随后用该 id 进行 `event_post`、`event_post_delay`、`event_trigger`、`event_timer_add`、`event_monitor_add` 和 slco 等待。

`EVENT_ID_INVALID` 作为非法事件 id，也作为 `event_new` 分配失败的返回值。公开发布、monitor 注册和定时触发接口都必须使用已经由调度器分配的 `EventId`。

### `EventMonitorId`

```c
typedef uint32_t EventMonitorId;
```

`EventMonitorId` 是 monitor 注册句柄，只用于移除 monitor。无效值规划为 `EVENT_MONITOR_ID_INVALID`。

### `EventTimerId`

```c
typedef uint32_t EventTimerId;
```

`EventTimerId` 是 event timer 注册句柄，只用于移除周期投递任务。无效值规划为 `EVENT_TIMER_ID_INVALID`。它与 `EventId`、`EventMonitorId` 和内部 `TimerId` 分属不同命名空间；用户不能把 `EventTimerId` 当作事件 id 发布，也不会接触 event 内部使用的 `TimerId`。

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
| `id` | 调度器分配的事件 id |
| `source` | 事件来源，只作元数据 |
| `value` | 来源提供的数值载荷 |
| `user_data` | 用户指针载荷，可为 `NULL` |

`Event` 是 handler 接收的统一载荷。用户发布事件时不需要手动构造 `Event`，而是使用 `event_new` 返回的 `EventId` 调用 `event_post` 或 `event_trigger`，再通过参数提供 `value` 和 `user_data`。

`user_data` 是可选载荷，允许为 `NULL`。handler 若需要长期保存数据，应自行保证该指针的生命周期。

### `event_handler_fn`

```c
typedef void (*event_handler_fn)(EventScheduler *scheduler, const Event *event, void *user_data);
```

handler 在事件被同步触发或从 pending post 分发时调用。回调参数中的 `user_data` 来自 handler 注册接口，不是 `Event.user_data`。

### `event_monitor_fn`

```c
typedef uint32_t (*event_monitor_fn)(EventScheduler *scheduler, void **event_user_data, void *user_data);
```

monitor 每次被检查时调用该函数。返回 `0U` 表示本次无事件；返回非 `0U` 表示触发绑定的 `EventId`，且该返回值会写入生成事件的 `Event.value`。

由于 `0U` 被保留为“不触发”，monitor 不能生成 `Event.value == 0U` 的 monitor 事件。如果业务值可能为 0，应在返回值中做非零编码，或把原始值放进 `user_data` 指向的上下文中。`event_user_data` 是事件指针载荷输出参数；不需要指针载荷时可把 `*event_user_data` 写为 `NULL`。`user_data` 是 monitor 自己的长期上下文，边沿检测、阈值、滞回、组合条件等状态都应保存在这里。

monitor 的检查时机由注册时的 `period_ticks` 决定：`0U` 表示每次 `event_scheduler_run_once` 都有机会检查；非 `0U` 表示附着到 event 内部 timer，按指定周期到期后才检查。

### `EventScheduler`

`EventScheduler` 规划持有以下状态：

- 内部 `TimerScheduler timer`。
- pending post 池，记录 `EventId`、来源、状态、载荷，以及延时 post 使用的内部 timer。
- handler 池，记录调度器分配的 `EventId`、回调和用户上下文。
- monitor 池，记录 `EventMonitorId`、目标 `EventId`、检查函数、检查周期、内部 timer 句柄和用户上下文。
- event timer 池，记录 `EventTimerId`、目标 `EventId`、内部 timer 句柄，以及周期到期时需要发布的 `value`、`user_data`。
- 分配 event id、monitor 句柄和 event timer 句柄所需的计数器或槽位状态。

`EventScheduler` 应保持静态分配友好，不使用堆内存。`EventId` 由调度器分配，用户不应自行构造或猜测事件 id。pending post 以 `EVENT_QUEUE_SIZE` 为容量，但同一个 `EventId` 只会占用一个槽位，重复 post 会刷新该槽位内容。

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

1. 检查一个或一组 `period_ticks == 0U` 的 monitor，并把满足条件的 `EventId` 写入 pending post 池。
2. 调用内部 `timer_scheduler_run_once`，把到期的延时 post 切换为 ready；event timer 到期时同步 trigger 目标事件；周期 monitor 到期时检查并可能生成一次 pending post。
3. 从 pending post 池取出一个 ready 事件，并按 `event.id` 分发给匹配 handler。

每次调用至少推进一个小步，避免在 monitor 或 handler 很多时形成长时间阻塞。没有可处理事件时返回 `0`。

### `event_scheduler_next_delay`

```c
TimerTick event_scheduler_next_delay(EventScheduler *self);
```

返回内部 timer 距离最近到期延时 post、event timer 或周期 monitor 检查的延迟。没有运行中的内部 timer 时返回 `(TimerTick)-1`。该接口不反映已经 ready 的 pending post，也不反映 `period_ticks == 0U` 的 monitor。

### `event_new`

```c
EventId event_new(EventScheduler *self, event_handler_fn handler, void *user_data);
```

创建一个事件并注册它的 handler。`self` 和 `handler` 必须有效。handler 池满或无法分配新的 id 时返回 `EVENT_ID_INVALID`。

返回的 `EventId` 由调度器分配，用户保存该 id 后用于 post、trigger、event timer、monitor 绑定和上层 slco 等待。一个 `EventId` 对应一个 handler；如果多个事件复用同一个 handler 函数，可多次调用 `event_new`，handler 内通过 `event->id` 区分来源。

### `event_delete`

```c
int event_delete(EventScheduler *self, EventId id);
```

删除指定事件。`id` 不得为 `EVENT_ID_INVALID`；找不到 id 返回 `-1`。删除事件时，调度器同步移除该 `EventId` 关联的 event timer、monitor 和尚未分发的 pending post，避免 id 后续复用时收到旧载荷。

### `event_post`

```c
int event_post(EventScheduler *self, EventId event_id, TimerTick delay_ticks, uint32_t value, void *user_data);
```

将来源为 `WD_EVENT_SOURCE_EXTERNAL` 的事件写入 pending post 池，由后续 `event_scheduler_run_once` 分发。`self` 必须有效，`event_id` 必须是 `event_new` 返回的有效 id；`delay_ticks` 必须在 timer 半范围内。pending post 池满时返回 `-1`。

`delay_ticks == 0U` 表示立即 post：该 `EventId` 的 pending post 变为 ready，后续 `run_once` 会分发它。`delay_ticks != 0U` 表示延时 post：scheduler 为该 pending post 分配或复用内部 timer，到期后把它切换为 ready。

同一个 `EventId` 最多存在一条 pending post。重复调用 `event_post` 不增加新 post，而是刷新该 `EventId` 对应的 `delay_ticks`、`value` 和 `user_data`。如果已有延时 post，再调用 `event_post(..., 0U, ...)` 会取消并释放之前的内部 timer，然后用新内容覆盖为 ready post。

延时 post 到期后，内部 timer 会立即由调度器释放；此后只保留 ready post 内容等待分发。内部 `TimerId` 不暴露给用户，也不写入 `Event`。

### `event_post_delay`

```c
int event_post_delay(EventScheduler *self, EventId event_id, TimerTick delay_ticks, uint32_t value, void *user_data);
```

`event_post_delay` 是延时投递的便捷接口，等价于 `event_post(self, event_id, delay_ticks, value, user_data)`。重复调用会刷新同一 `EventId` 的 pending post 和内部 timer。

当 `delay_ticks == 0U` 时，它退化为立即 post。若希望 API 层禁止 0 延时，可在上层自行封装；event 库本身把 `0U` 解释为普通 `event_post`。

### `event_is_posted`

```c
uint32_t event_is_posted(EventScheduler *self, EventId event_id);
```

读取指定 `EventId` 当前 pending post 保存的 `value`。ready post 和尚未到期的 delay post 都返回该 post 的 `value`；不存在 pending post 返回 `0U`。`self` 必须有效，`event_id` 必须是 `event_new` 返回的有效 id。

因此 `event_is_posted` 更适合把 `value` 设计成事件状态码：`0U` 表示“当前没有 pending post”，非零值表示当前待处理状态。如果业务确实需要 pending post 的 value 为 `0U`，就不能只靠该接口区分“存在且 value 为 0”和“不存在”，应使用非零编码或在业务上下文中另存状态。

### `event_trigger`

```c
int event_trigger(EventScheduler *self, EventId event_id, uint32_t value, void *user_data);
```

立即同步分发来源为 `WD_EVENT_SOURCE_EXTERNAL` 的事件，不经过 pending post 池，也不影响已有 `event_post_delay`。`self` 必须有效，`event_id` 必须是 `event_new` 返回的有效 id。有效事件正常会调用一个 handler 并返回 `1`。

### `event_timer_add`

```c
EventTimerId event_timer_add(EventScheduler *self, EventId event_id, TimerTick period_ticks, uint32_t value, void *user_data);
```

为指定 `EventId` 添加一个周期投递任务。`self` 必须有效，`event_id` 必须是 `event_new` 返回的有效 id；`period_ticks` 必须非 0，并且在 timer 半范围内。添加成功返回独立的 `EventTimerId`；event timer 池满、内部 timer 池满或启动内部 timer 失败时返回 `EVENT_TIMER_ID_INVALID`。

同一个 `EventId` 可以拥有多个 event timer。每次调用 `event_timer_add` 都创建一个新的周期任务，删除时必须保存并传回它返回的 `EventTimerId`。首次投递发生在 `period_ticks` 之后，之后按同一周期继续投递。

event timer 到期时使用 trigger 语义同步调用目标 `EventId` 的 handler，事件来源为 `WD_EVENT_SOURCE_TIMER`。它不写入 pending post 池，不影响 `event_is_posted` 的返回值，也不会刷新或取消同一 `EventId` 上已有的 `event_post_delay`。event timer 使用的内部 timer 由调度器管理，不暴露给用户。

### `event_timer_remove`

```c
int event_timer_remove(EventScheduler *self, EventTimerId id);
```

删除指定 event timer，并释放其内部 timer 槽位。`self` 必须有效，`id` 不得为 `EVENT_TIMER_ID_INVALID`。找不到 event timer 时返回 `-1`。删除一个 event timer 不影响同一 `EventId` 上的其他 event timer，也不清除该 `EventId` 已经存在的 pending post。

event timer handler 在 `event_scheduler_run_once` 推进内部 timer 时同步执行。handler 内如果调用 `event_post` 写入 pending post，该 post 仍然会按调度器规则等待后续分发；如果显式调用 `event_trigger`，则会立即嵌套同步分发。

### `event_monitor_add`

```c
EventMonitorId event_monitor_add(EventScheduler *self, EventId event_id, event_monitor_fn monitor, TimerTick period_ticks, void *user_data);
```

添加通用 monitor。`self` 和 `monitor` 必须有效，`event_id` 必须是 `event_new` 返回的有效 id。monitor 池满时返回 `EVENT_MONITOR_ID_INVALID`。

`monitor` 返回非 `0U` 时，scheduler 生成来源为 `WD_EVENT_SOURCE_MONITOR`、id 为 `event_id`、value 为返回值的事件。event 不内置条件保持、边沿判断或数值比较策略；这些状态由 monitor 的 `user_data` 自行维护。

`period_ticks` 控制检查周期：

- `period_ticks == 0U`：monitor 不占用内部 timer，每次 `event_scheduler_run_once` 都有机会被检查，适合非常轻量、需要低延迟的条件。
- `period_ticks != 0U`：monitor 占用一个内部 timer 槽位，timer 到期时才执行一次检查；周期越长，CPU 空转越少，但事件响应延迟越大。

周期 monitor 到期检查后，如果 monitor 返回 `0U`，只重新安排下一次检查，不生成事件；如果返回非 `0U`，先投递事件，再继续按周期检查。内部 `TimerId` 不暴露给用户。

### `event_monitor_remove`

```c
int event_monitor_remove(EventScheduler *self, EventMonitorId id);
```

移除 monitor。`id` 不得为 `EVENT_MONITOR_ID_INVALID`；找不到 id 返回 `-1`。如果该 monitor 附着了内部 timer，移除时必须同时释放内部 timer 槽位。

## 调度行为

- 外部立即触发通过 `event_trigger` 同步调用匹配 `EventId` 的 handler。
- 外部异步投递通过 `event_post(..., 0U, ...)` 写入 ready pending post，由 `event_scheduler_run_once` 分发。
- 外部延时投递通过 `event_post_delay` 或 `event_post(..., delay_ticks, ...)` 写入 delay pending post；同一 `EventId` 重复调用会刷新内容和到期时间。
- `event_post` 与 `event_post_delay` 共用同一 pending post 资源；立即 post 会清除该 `EventId` 之前 delay post 使用的内部 timer。
- `event_post_delay` 到期后，内部 timer action 只把 pending post 切换为 ready，并立即释放该 timer。
- `event_timer_add` 用非零周期内部 timer 周期性同步 trigger 来源为 `WD_EVENT_SOURCE_TIMER` 的事件；`period_ticks == 0U` 非法。
- 同一个 `EventId` 可以注册多个 event timer；每个 event timer 拥有独立 `EventTimerId`，并通过 `event_timer_remove` 删除。
- `period_ticks == 0U` 的 monitor 由 `event_scheduler_run_once` 主动检查，适合低延迟但会随主循环频率消耗 CPU。
- `period_ticks != 0U` 的 monitor 附着到内部 timer，只有到期时才检查；主循环可以结合 `event_scheduler_next_delay` 休眠到最近到期点。
- `EventSource` 只表示来源元数据，不参与 handler 匹配。
- handler 执行期间允许投递新事件；新 pending post 不会在当前 handler 调用栈中递归分发，除非用户显式调用 `event_trigger`。

## 错误处理

- 空 scheduler、空 handler、空 monitor 回调、非法或未分配的 `EventId`、非法 monitor id 使用 `WD_ASSERT` 暴露编程错误。
- pending post 池满、handler 池满、monitor 池满、timer 池满、event timer 不存在、周期 monitor 分配内部 timer 失败或内部 timer 操作失败返回失败值。
- `event_new` 分配失败返回 `EVENT_ID_INVALID`；`event_delete` 找不到指定 `EventId` 返回 `-1`。

## 单线程模型

`event` 沿用 timer 的单线程拥有模型。一个 `EventScheduler` 应由一个线程或一个主循环拥有，模块内部不提供锁或临界区。中断中若要触发事件，应由调用层保证 `event_post` 的并发安全，或先写入平台自己的 ISR-safe 缓冲区，再在主循环中投递到 event。

## 使用示例

### 外部立即触发和异步投递

```c
static void button_handler(EventScheduler *scheduler, const Event *event, void *user_data)
{
    (void)scheduler;
    (void)user_data;
    (void)event;
}

void example(EventScheduler *event_scheduler)
{
    EventId button_event;

    button_event = event_new(event_scheduler, button_handler, NULL);

    event_trigger(event_scheduler, button_event, 0U, NULL);
    event_post(event_scheduler, button_event, 0U, 1U, NULL);
}
```

### 延时 post 与 event timer

```c
static void timer_handler(EventScheduler *scheduler, const Event *event, void *user_data)
{
    (void)scheduler;
    (void)event;
    (void)user_data;
}

void example_timer(EventScheduler *event_scheduler)
{
    EventId timeout_event;
    EventId heartbeat_event;
    EventTimerId heartbeat_timer;
    EventTimerId fast_heartbeat_timer;
    uint32_t pending_value;

    timeout_event = event_new(event_scheduler, timer_handler, NULL);
    heartbeat_event = event_new(event_scheduler, timer_handler, NULL);

    event_post_delay(event_scheduler, timeout_event, 100U, 1U, NULL);
    event_post_delay(event_scheduler, timeout_event, 200U, 2U, NULL);
    heartbeat_timer = event_timer_add(event_scheduler, heartbeat_event, 1000U, 1U, NULL);
    fast_heartbeat_timer = event_timer_add(event_scheduler, heartbeat_event, 250U, 2U, NULL);

    pending_value = event_is_posted(event_scheduler, timeout_event);
    if (pending_value == 2U)
    {
        event_post(event_scheduler, timeout_event, 0U, 3U, NULL);
    }

    event_scheduler_run_once(event_scheduler);
    event_timer_remove(event_scheduler, fast_heartbeat_timer);
    event_timer_remove(event_scheduler, heartbeat_timer);
}
```

### 通用 monitor

下面的 `level_changed` 示例用 `current + 1U` 编码变化值，因为 monitor 返回 `0U` 被保留为“不触发”。如果原始值可能覆盖完整 `uint32_t` 范围，应返回固定的非零事件码，并通过 `event->user_data` 指向的上下文读取原始值。

```c
typedef struct level_monitor_t
{
    uint32_t last_value;
    uint32_t current_value;
} LevelMonitor;

static uint32_t sensor_ready(EventScheduler *scheduler, void **event_user_data, void *user_data)
{
    int *ready;

    (void)scheduler;

    ready = (int *)user_data;
    if (*ready == 0)
    {
        return 0U;
    }

    *event_user_data = NULL;
    return 1U;
}

static uint32_t level_changed(EventScheduler *scheduler, void **event_user_data, void *user_data)
{
    LevelMonitor *monitor;
    uint32_t current;

    (void)scheduler;

    monitor = (LevelMonitor *)user_data;
    current = monitor->current_value;
    if (current == monitor->last_value)
    {
        return 0U;
    }

    monitor->last_value = current;
    *event_user_data = monitor;
    return current + 1U;
}

static void monitor_handler(EventScheduler *scheduler, const Event *event, void *user_data)
{
    (void)scheduler;
    (void)event;
    (void)user_data;
}

void example_monitor(EventScheduler *event_scheduler)
{
    static int ready = 0;
    static LevelMonitor level_monitor = {0U, 0U};
    EventId sensor_ready_event;
    EventId level_change_event;

    sensor_ready_event = event_new(event_scheduler, monitor_handler, NULL);
    level_change_event = event_new(event_scheduler, monitor_handler, NULL);

    event_monitor_add(event_scheduler, sensor_ready_event, sensor_ready, 0U, &ready);
    event_monitor_add(event_scheduler, level_change_event, level_changed, 20U, &level_monitor);
}
```

## 检查

实现该设计时需要检查：

- `event.h` 自包含标准头和 `timer.h`；`until.h` 由 `timer.h` 传递包含，event 不重复直接包含。
- `event.h` 的配置转发发生在首次包含 `timer.h` 前。
- `EventId` 由 `event_new` 分配，不由用户手写宏值；`EventId` 与 `TimerId` 不混用，event 公开 API 不暴露内部 `TimerId`。
- `event` 不暴露内部 `TimerId`，用户只通过 `EventTimerId` 管理 event timer 任务。
- `event_post` 与 `event_post_delay` 共用 pending post 资源，同一 `EventId` 重复调用只刷新内容和时间。
- `event_post_delay` 到期后必须释放内部 timer，只保留 ready post 等待分发。
- 周期投递使用 `event_timer_add`，且 `period_ticks` 不允许为 `0U`。
- 同一个 `EventId` 允许拥有多个 event timer，删除时使用各自的 `EventTimerId`。
- `period_ticks != 0U` 的 monitor 由内部 timer 驱动，`period_ticks == 0U` 的 monitor 才随 `run_once` 检查。
- `EventSource` 只作为元数据，不参与 handler 匹配。
- `EventScheduler` 可静态分配，不使用堆。
- 公开接口入口覆盖所有参数校验。
- 私有非 `_event_assert_*` 函数不包含断言或防御性参数校验。
- `event` 不包含 `slco.h`。
