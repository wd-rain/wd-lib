---
aliases:
  - slco
  - slco.h
  - 无栈协程
depends:
  - "[[event]]"
tags:
  - c
  - clib
  - os
  - slco
---

# slco

`slco` 是 `[[os]]` 目录下位于 `event` 之上的无栈协程模块。它通过 event 等待、timer 延时和协程调用链推进协程，不使用独立 C 栈，不依赖堆内存。

本文是设计文档，不表示当前仓库已经包含 `slco.h` 或 `slco.c` 的实现。

## 依赖关系

规划代码路径：

- `clib-code/os/slco/slco.h`
- `clib-code/os/slco/slco.c`

`slco.h` 应包含 `../event/event.h`。`slco` 可以推进 `EventScheduler`，但 `event` 不知道 slco。

## 配置覆盖

`slco` 保留自己的配置命名空间，同时允许通过 slco 的配置覆盖下层 event 和 timer：

| 配置 | 作用 |
|---|---|
| `SLCO_TASK_POOL_SIZE` | 协程池容量 |
| `SLCO_CALL_DEPTH` | 协程调用链最大嵌套深度 |
| `SLCO_EVENT_QUEUE_SIZE` | 转发为 event 队列容量 |
| `SLCO_EVENT_HANDLER_POOL_SIZE` | 转发为 event handler 池容量 |
| `SLCO_EVENT_MONITOR_POOL_SIZE` | 转发为 event monitor 池容量 |
| `SLCO_EVENT_TIMER_POOL_SIZE` | 转发为 event 内部 timer 池容量 |

配置覆盖规则：

- 用户直接定义的底层配置优先级最高，例如 `EVENT_QUEUE_SIZE` 高于 `SLCO_EVENT_QUEUE_SIZE`，`TIMER_POOL_SIZE` 高于 `SLCO_EVENT_TIMER_POOL_SIZE`。
- 如果用户只包含 `os/slco/slco.h`，并定义了 `SLCO_EVENT_*`，则 slco 头文件应在包含 `event.h` 前把这些值转发给对应的 `EVENT_*` 配置。
- `SLCO_EVENT_TIMER_POOL_SIZE` 通过 event 层继续影响内部 `TIMER_POOL_SIZE`，但不得破坏用户已直接定义的 `TIMER_POOL_SIZE`。
- 配置宏必须在首次包含 `timer.h`、`event.h` 或 `slco.h` 前定义；底层头文件已经展开后，后续覆盖不再生效。
- 文档只约定配置转发语义，不要求实现绑定到某段固定预处理代码。

## 接口总览

| 类别 | 接口 | 功能 |
|---|---|---|
| 类型 | `SlcoId` | 协程 id |
| 类型 | `SlcoState` | 协程运行状态 |
| 类型 | `Slco` | 协程对象 |
| 类型 | `SlcoScheduler` | 协程调度器 |
| 类型 | `SlcoCallFrame` | 协程调用栈帧 |
| 回调 | `slco_fn` | 协程体函数 |
| 初始化 | `slco_scheduler_init` | 初始化协程调度器 |
| 调度 | `slco_scheduler_run_once` | 推进 event 和一个 ready 协程 |
| 生命周期 | `slco_new` | 分配协程槽位 |
| 生命周期 | `slco_start` | 启动或重启协程 |
| 生命周期 | `slco_stop` | 停止协程 |
| 生命周期 | `slco_delete` | 删除协程 |
| 查询 | `slco_is_finished` | 查询协程是否结束 |
| 查询 | `slco_result` | 获取协程返回值 |
| 访问 | `slco_user_data` | 获取协程用户上下文 |
| 宏 | `SLCO_BEGIN` | 协程体开始 |
| 宏 | `SLCO_WAIT_EVENT` | 等待事件 |
| 宏 | `SLCO_DELAY` | 等待 timer 延时 |
| 宏 | `SLCO_CALL` | 调用子协程 |
| 宏 | `SLCO_RETURN` | 结束并返回长期有效指针 |
| 宏 | `SLCO_END` | 协程体结束 |

## 类型

### `SlcoId`

```c
typedef uint32_t SlcoId;
```

`SlcoId` 是外部引用协程的唯一标识。无效值规划为 `SLCO_ID_INVALID`。

### `SlcoState`

```c
typedef enum slco_state_t
{
    WD_SLCO_STATE_UNUSED = 0,
    WD_SLCO_STATE_READY,
    WD_SLCO_STATE_WAITING_EVENT,
    WD_SLCO_STATE_WAITING_DELAY,
    WD_SLCO_STATE_WAITING_CALL,
    WD_SLCO_STATE_FINISHED
} SlcoState;
```

协程状态由调度器维护。用户不应直接修改状态字段。

### `slco_fn`

```c
typedef void (*slco_fn)(SlcoScheduler *scheduler, Slco *self, const Event *event);
```

协程体函数由调度器调用。`event` 在事件唤醒时指向触发事件；普通调度或首次启动时可为 `NULL`。

### `Slco`

`Slco` 规划保存以下状态：

- `SlcoId id`。
- `SlcoState state`。
- 宏式状态机恢复位置。
- `slco_fn fn`。
- 首次运行标记 `first_run`。
- 等待的事件类型。
- 延时等待使用的 event timer id。
- 子协程返回值指针。
- 用户上下文指针。

跨越 `SLCO_WAIT_EVENT`、`SLCO_DELAY`、`SLCO_CALL` 的业务局部状态不能依赖 C 栈局部变量，应保存到用户上下文结构体或 `Slco` 关联对象中。

### `SlcoCallFrame`

```c
typedef struct slco_call_frame_t
{
    SlcoId caller_id;
    SlcoId callee_id;
} SlcoCallFrame;
```

调用帧由 `SlcoScheduler` 的固定深度数组保存。`SLCO_CALL_DEPTH` 决定最多允许多少层协程调用协程。

### `SlcoScheduler`

`SlcoScheduler` 规划持有以下状态：

- `EventScheduler *event_scheduler`。
- 协程池。
- 固定深度调用帧数组。
- 当前调用深度。
- event handler id，用于接收 event 并唤醒等待协程。

`SlcoScheduler` 不拥有 `EventScheduler` 的存储，只保存指针。调用方负责保证 event scheduler 生命周期覆盖 slco scheduler。

## 接口

### `slco_scheduler_init`

```c
void slco_scheduler_init(SlcoScheduler *self, EventScheduler *event_scheduler);
```

初始化协程调度器，并在 event 中注册 slco 内部 handler。`self` 和 `event_scheduler` 必须有效。

### `slco_scheduler_run_once`

```c
int slco_scheduler_run_once(SlcoScheduler *self);
```

推进一次调度。推荐顺序：

1. 调用 `event_scheduler_run_once`，让外部事件、monitor 和 timer 先产生唤醒。
2. 从协程池中选择一个 ready 协程。
3. 调用该协程的 `slco_fn`。
4. 根据宏产生的状态更新协程。

没有 ready 协程时返回 `0`。成功推进协程也返回 `0`，普通调度失败使用 `-1`。

### `slco_new`

```c
SlcoId slco_new(SlcoScheduler *self);
```

从协程池分配一个槽位。池满返回 `SLCO_ID_INVALID`。

### `slco_start`

```c
int slco_start(SlcoScheduler *self, SlcoId id, slco_fn fn, void *user_data);
```

启动或重启协程。`fn` 必须有效。启动时状态设为 ready，恢复位置清零，`first_run` 设为真，旧返回值清空。

### `slco_stop`

```c
int slco_stop(SlcoScheduler *self, SlcoId id);
```

停止协程。若该协程正在等待子协程或被父协程等待，调度器应清理相关调用帧，避免父子关系悬空。找不到 id 返回 `-1`。

### `slco_delete`

```c
int slco_delete(SlcoScheduler *self, SlcoId id);
```

停止并释放协程槽位。找不到 id 返回 `-1`。

### `slco_is_finished`

```c
int slco_is_finished(SlcoScheduler *self, SlcoId id);
```

协程结束返回 `1`，未结束返回 `0`，找不到 id 返回 `-1`。

### `slco_result`

```c
void *slco_result(SlcoScheduler *self, SlcoId id);
```

返回协程最近一次 `SLCO_RETURN` 保存的指针。返回值必须由协程提供长期有效存储，调度器只保存指针，不复制数据。

### `slco_user_data`

```c
void *slco_user_data(Slco *self);
```

返回 `slco_start` 保存的用户上下文指针。`self` 必须有效。返回 `NULL` 是合法语义，表示该协程没有用户上下文。

## 宏语义

### `SLCO_BEGIN`

协程体开头必须调用。该宏恢复上次挂起位置，并允许调度器识别第一次进入协程。

### `SLCO_WAIT_EVENT`

等待指定事件类型。调用后当前协程进入 `WD_SLCO_STATE_WAITING_EVENT`，并从协程体返回。slco 内部 event handler 收到匹配事件后，将协程恢复为 ready，并在下一次调度时把事件传回协程体。

### `SLCO_DELAY`

等待指定 tick 数。该宏通过 event timer 实现延时：为当前协程启动或复用一个 event timer，随后进入 `WD_SLCO_STATE_WAITING_DELAY`。timer 到期事件由 slco 内部 handler 转换为协程 ready。

### `SLCO_CALL`

调用子协程。该宏把当前协程作为 caller、目标协程作为 callee 推入 `SlcoCallFrame` 栈。caller 进入 `WD_SLCO_STATE_WAITING_CALL`，callee 进入 ready。

调用栈满时，宏应让调度器记录失败，并使当前协程以失败状态返回调度器；具体返回码在实现时通过调度器错误字段或接口返回值表达。

### `SLCO_RETURN`

结束当前协程并保存 `void *` 返回值。若当前协程是某个 caller 的 callee，调度器弹出调用帧，把返回值保存给 caller，并将 caller 置为 ready。

返回值必须是静态存储、全局对象、协程上下文字段或其他用户保证长期有效的地址。

### `SLCO_END`

协程体结尾必须调用。若流程自然到达结尾，则等价于返回 `NULL` 并进入 finished 状态。

## 调度行为

- `slco_scheduler_run_once` 先推进 event，再推进一个 ready 协程。
- event handler 只负责把等待事件的协程标记为 ready，不直接在 handler 调用栈内运行协程体。
- 一个事件可唤醒多个等待同类型事件的协程；调度器仍按 run_once 节奏逐个运行。
- 协程首次启动时 `first_run` 为真，协程可据此初始化上下文。
- 协程重启会清空恢复位置、等待条件、返回值和调用关系。
- 协程停止或删除时，必须释放其延时 timer 和相关调用帧。

## 错误处理

- 空 scheduler、空 event scheduler、空协程函数、非法 id 使用 `WD_ASSERT` 暴露编程错误。
- 协程池满、调用栈满、找不到 id、event timer 分配失败返回失败值。
- 子协程返回值为 `NULL` 是合法语义，表示无结果或空结果。

## 单线程模型

`slco` 沿用 event 和 timer 的单线程拥有模型。一个 `SlcoScheduler` 应由一个主循环拥有，模块内部不提供锁。跨线程或中断唤醒应先通过调用层转换成 event，再由主循环推进 slco。

## 使用示例

### 等待事件

```c
typedef struct app_task_t
{
    int ready_count;
} AppTask;

static void app_task(SlcoScheduler *scheduler, Slco *self, const Event *event)
{
    AppTask *task;

    task = (AppTask *)slco_user_data(self);

    SLCO_BEGIN(scheduler, self);

    while (1)
    {
        SLCO_WAIT_EVENT(scheduler, self, 1U);
        if (event != NULL)
        {
            ++task->ready_count;
        }
    }

    SLCO_END(scheduler, self);
}
```

### 延时等待

```c
static void blink_task(SlcoScheduler *scheduler, Slco *self, const Event *event)
{
    (void)event;

    SLCO_BEGIN(scheduler, self);

    while (1)
    {
        SLCO_DELAY(scheduler, self, 100U);
        /* 用户在这里切换 LED 状态。 */
    }

    SLCO_END(scheduler, self);
}
```

### 协程调用和返回值

```c
typedef struct calc_result_t
{
    uint32_t value;
} CalcResult;

static CalcResult result;

static void child_task(SlcoScheduler *scheduler, Slco *self, const Event *event)
{
    (void)event;

    SLCO_BEGIN(scheduler, self);

    result.value = 123U;
    SLCO_RETURN(scheduler, self, &result);

    SLCO_END(scheduler, self);
}

static void parent_task(SlcoScheduler *scheduler, Slco *self, const Event *event)
{
    void *child_result;

    (void)event;

    SLCO_BEGIN(scheduler, self);

    SLCO_CALL(scheduler, self, 2U);
    child_result = slco_result(scheduler, 2U);
    (void)child_result;

    SLCO_END(scheduler, self);
}
```

## 检查

实现该设计时需要检查：

- `slco.h` 自包含标准头和 `event.h`；`until.h` 由 `event -> timer` 传递提供，除非 slco 公开接口直接使用下层未公开的额外符号，否则不直接包含 `until.h`。
- `slco.h` 的配置转发发生在首次包含 `event.h` 前。
- `SLCO_EVENT_*` 不覆盖用户已经直接定义的 `EVENT_*` 或 `TIMER_*`。
- `SlcoScheduler` 可静态分配，不使用堆。
- 公开接口入口覆盖所有参数校验。
- 私有非 `_slco_assert_*` 函数不包含断言或防御性参数校验。
- `slco` 不反向修改 timer 或 event 的运行语义，只通过公开接口使用下层模块。
