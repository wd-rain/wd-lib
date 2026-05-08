---
aliases:
  - os
  - OS 抽象层
depends:
  - "[[until]]"
tags:
  - c
  - clib
  - os
---

# os

`os` 是 clib 中面向调度、事件、协程等运行期工具的抽象层目录。这里的 `os` 表示模块归属，不要求公共符号使用 `os_` 前缀；各子模块仍使用自己的稳定前缀，例如 `timer_`、`event_` 和 `slco_`。

## 分层关系

当前规划的运行期工具分为三层：

```text
timer -> event -> slco
```

- `[[timer]]`：独立定时器模块，只依赖 `[[until]]`。
- `[[event]]`：事件调度模块，依赖 `timer`，负责把外部触发、monitor 和 timer 到期统一分发。
- `[[slco]]`：无栈协程模块，依赖 `event`，通过事件等待、定时等待和协程调用链推进协程。

底层模块不能反向包含上层模块。`timer` 不知道 event 和 slco；event 不知道 slco。

头文件包含关系应保持直接依赖最小化：`event.h` 直接包含 `timer.h`，不重复直接包含 `until.h`；`until.h` 由 `timer.h` 传递提供。后续 `slco.h` 同理只直接包含 `event.h`，除非它需要使用下层未公开的额外接口。

## 裁剪原则

这些模块按依赖方向裁剪：

- 只需要定时器时，只编译 `os/timer`。
- 需要事件时，编译 `timer + event`。
- 需要无栈协程时，编译 `timer + event + slco`。

上层模块可以包含下层模块并驱动下层 scheduler：

- `event` 内部包含并驱动 `TimerScheduler`。
- `slco` 调用并推进 `EventScheduler`。

## 配置覆盖

`os` 层配置按模块命名空间分层：

- `timer` 使用 `TIMER_*` 配置。
- `event` 使用 `EVENT_*` 配置，并可通过 `EVENT_TIMER_POOL_SIZE` 影响其内部 timer 池容量。
- `slco` 使用 `SLCO_*` 配置，并可通过 `SLCO_EVENT_*` 影响下层 event 配置，通过 `SLCO_EVENT_TIMER_POOL_SIZE` 间接影响 event 内部 timer 池容量。

配置覆盖优先级从高到低为：

1. 用户直接定义的底层配置，例如 `TIMER_POOL_SIZE` 或 `EVENT_QUEUE_SIZE`。
2. 上层模块转发配置，例如 `EVENT_TIMER_POOL_SIZE` 或 `SLCO_EVENT_QUEUE_SIZE`。
3. 模块默认值。

配置宏必须在首次包含 `timer.h`、`event.h` 或 `slco.h` 前定义。若底层头文件已经被包含，头文件保护会让后续转发配置不再影响已经确定的下层配置。

配置文档只约定覆盖语义和优先级，不要求实现绑定到某段固定预处理代码。

## 错误处理

`os` 层沿用当前库风格：

- 空对象、缺失必要 ops、非法 id 等编程错误使用 `WD_ASSERT`。
- 池满、找不到对象、对象未运行等普通运行期失败返回 `-1` 或模块约定的失败值。
