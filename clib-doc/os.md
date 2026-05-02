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
- `event`：未来事件模块，依赖 `timer`，负责把普通事件和 timer 到期统一分发。
- `slco`：未来无栈协程模块，依赖 `event`，通过事件等待和定时等待推进协程。

## 裁剪原则

这些模块按依赖方向裁剪：

- 只需要定时器时，只编译 `os/timer`。
- 需要事件时，编译 `timer + event`。
- 需要无栈协程时，编译 `timer + event + slco`。

底层模块不能反向包含上层模块。`timer` 不知道 event 和 slco；event 不知道 slco。

## 错误处理

`os` 层沿用当前库风格：

- 空对象、缺失必要 ops、非法 id 等编程错误使用 `ASSERT`。
- 池满、找不到对象、对象未运行等普通运行期失败返回 `-1` 或模块约定的失败值。

