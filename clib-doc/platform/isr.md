---
aliases:
  - isr
  - isr.h
  - platform/isr
  - Isr
  - ISR 基类
depends:
  - "[[until]]"
tags:
  - c
  - clib
  - platform
  - isr
---

# isr

`isr` 提供一个轻量级通用 ISR 基类，用于把真实 `IRQHandler` 之后的公共中断流程抽象出来。它不替代 MCU 向量表，也不负责外设业务，只固定 `pending -> ack -> action` 这一段最小流程。

本文只说明 `isr` 本身提供的接口，所有接口都使用独立标题，方便在 Obsidian 大纲中快速定位。

## 依赖关系

`[[isr]] -> [[until]]`

`isr` 位于 `[[platform]]` 层目录下，但源码不依赖 `platform.h`。它直接依赖 `[[until]]` 中的 `ASSERT`，用于空指针、缺失 ops 和非法调用检查。

## 接口总览

| 类别 | 接口 | 基本功能 | 使用要点 |
|---|---|---|---|
| 类型 | `Isr` | 通用 ISR 基类对象 | 保存 `ops/source/action` |
| 类型 | `isr_action_fn` | 派生对象 action 函数类型 | 由 `isr_handle` 在 pending 后调用 |
| Ops | `isr_enable_fn` | 平台使能中断源函数类型 | 形参为 `source` |
| Ops | `isr_disable_fn` | 平台关闭中断源函数类型 | 形参为 `source` |
| Ops | `isr_pending_fn` | 平台 pending 判断函数类型 | 返回 `bool` |
| Ops | `isr_ack_fn` | 平台清 pending 函数类型 | 可选 |
| Ops | `IsrOps` | 平台中断源操作集合 | `pending` 必选 |
| 初始化 | `isr_init(self, ops, source, action)` | 初始化 ISR 基类对象 | 不自动使能中断 |
| 启停 | `isr_enable(self)` | 使能中断源 | 调用 `ops->enable` |
| 启停 | `isr_disable(self)` | 关闭中断源 | 调用 `ops->disable` |
| 处理 | `isr_handle(self)` | 执行公共 ISR 流程 | `pending -> ack -> action` |
| 释放 | `isr_deinit(self)` | 关闭中断源并清空对象 | 调用后对象失效 |

## 类型定义

### `isr_action_fn`

ISR action 函数类型。

```c
typedef struct isr_t Isr;
typedef void (*isr_action_fn)(Isr *self);
```

`action` 由派生对象注册。`isr_handle()` 不知道具体外设类型，只在确认 pending 后调用 action。

### `Isr`

通用 ISR 基类对象。

```c
struct isr_t
{
    const IsrOps *ops;
    size_t source;
    isr_action_fn action;
};
```

| 成员 | 说明 |
|---|---|
| `ops` | 平台中断源操作集合 |
| `source` | 平台定义的中断源编号 |
| `action` | 派生对象注册的中断动作 |

`source` 的语义由平台适配层决定。GPIO 中断当前使用 pin 作为 source。

## Ops接口

本节说明具体平台必须实现的硬件中断源操作接口。`IsrOps` 只包含 enable、disable、pending、ack，不包含触发沿、payload、queue、event 或业务策略。

### `isr_enable_fn`

平台使能中断源函数类型。

```c
typedef void (*isr_enable_fn)(size_t source);
```

该函数由 `isr_enable` 调用。具体平台可以在这里打开中断 mask、使能 NVIC 或执行其他必要硬件动作。

### `isr_disable_fn`

平台关闭中断源函数类型。

```c
typedef void (*isr_disable_fn)(size_t source);
```

该函数由 `isr_disable` 和 `isr_deinit` 调用。具体平台应保证调用后该 source 不再继续触发对应 ISR 流程。

### `isr_pending_fn`

平台 pending 判断函数类型。

```c
typedef bool (*isr_pending_fn)(size_t source);
```

该函数是 `isr_handle` 的必选 ops。返回 `false` 时，`isr_handle` 直接返回，不会调用 `ack` 或 `action`。

### `isr_ack_fn`

平台清 pending 函数类型。

```c
typedef void (*isr_ack_fn)(size_t source);
```

该函数是可选 ops。如果 `ack == NULL`，`isr_handle` 会跳过清 pending 步骤。

### `IsrOps`

平台中断源操作集合。

```c
typedef struct isr_ops_t
{
    isr_enable_fn enable;
    isr_disable_fn disable;
    isr_pending_fn pending;
    isr_ack_fn ack;
} IsrOps;
```

| 成员 | 说明 |
|---|---|
| `enable` | 使能中断源 |
| `disable` | 关闭中断源 |
| `pending` | 判断中断源是否挂起，必选 |
| `ack` | 清除挂起标志，可选 |

## 初始化接口

### `isr_init(self, ops, source, action)`

初始化 ISR 基类对象。

```c
void isr_init(Isr *self, const IsrOps *ops, size_t source, isr_action_fn action);
```

示例：

```c
Isr isr;

isr_init(&isr, &board_isr_ops, 3U, action);
```

该函数会保存 `ops`、`source` 和 `action`。它不会自动调用 `ops->enable`。

`self`、`ops` 和 `ops->pending` 必须有效，否则触发 `ASSERT`。

## 启停接口

### `isr_enable(self)`

使能中断源。

```c
void isr_enable(Isr *self);
```

该函数调用 `self->ops->enable(self->source)`。`self`、`ops`、`pending` 和 `enable` 必须有效。

### `isr_disable(self)`

关闭中断源。

```c
void isr_disable(Isr *self);
```

该函数调用 `self->ops->disable(self->source)`。`self`、`ops`、`pending` 和 `disable` 必须有效。

## 处理接口

### `isr_handle(self)`

执行 ISR 公共流程。

```c
void isr_handle(Isr *self);
```

流程固定为：

```text
pending -> ack(optional) -> action(optional)
```

示例：

```c
void EXTI3_IRQHandler(void)
{
    isr_handle(gpio_isr_base(&button_isr));
}
```

`isr_handle` 应放在真实 `IRQHandler` 中。它只处理公共中断源流程，不写业务逻辑。

## 释放接口

### `isr_deinit(self)`

关闭中断源并清空 ISR 基类对象。

```c
void isr_deinit(Isr *self);
```

该函数会先调用 `ops->disable(self->source)`，再清空 `ops`、`source` 和 `action`。调用后对象不再有效，除非重新执行 `isr_init`。

## 平台映射规则

平台层应把 `source` 映射到具体硬件中断源。例如 GPIO 平台可以把 `source` 解释为 pin；定时器平台可以把 `source` 解释为 timer id 或 channel id。

```c
static bool board_isr_pending(size_t source)
{
    /* map source to hardware pending flag */
    (void)source;
    return false;
}
```

不要把业务策略放进 `IsrOps`。`IsrOps` 只表达中断源的硬件状态和控制动作。

## 完整示例

```c
#include "clib-code/platform/isr/isr.h"

static bool pending_flag;
static bool action_flag;

static void mock_enable(size_t source)
{
    (void)source;
}

static void mock_disable(size_t source)
{
    (void)source;
}

static bool mock_pending(size_t source)
{
    (void)source;
    return pending_flag;
}

static void mock_ack(size_t source)
{
    (void)source;
    pending_flag = false;
}

static void mock_action(Isr *self)
{
    (void)self;
    action_flag = true;
}

static const IsrOps mock_ops = {
    mock_enable,
    mock_disable,
    mock_pending,
    mock_ack
};

int main(void)
{
    Isr isr;

    isr_init(&isr, &mock_ops, 0U, mock_action);
    isr_enable(&isr);

    pending_flag = true;
    isr_handle(&isr);

    isr_deinit(&isr);
    return action_flag ? 0 : 1;
}
```

编译：

```powershell
gcc -std=c99 -Wall -Wextra -g -O0 example.c clib-code\platform\isr\isr.c -Iclib-code -o example.exe
```

## 检查

可以先执行语法检查确认接口声明和实现一致：

```powershell
gcc -std=c99 -Wall -Wextra -pedantic -fsyntax-only clib-code\platform\isr\isr.c -Iclib-code
```

手动验证时，重点检查：

- `pending == false` 时，不调用 `ack` 和 `action`。
- `pending == true` 时，顺序为 `pending -> ack -> action`。
- `ack == NULL` 时，`pending == true` 仍会执行 `action`。
- `isr_enable` 和 `isr_disable` 会透传 `source`。
- `isr_deinit` 会先 disable，再清空对象。

## 通用注意事项

> [!warning]
> `isr` 使用 `ASSERT` 暴露编程错误，不返回状态码。空对象、空 `ops`、缺失 `pending`、缺失启停函数会直接卡死在断言处。

- `IRQHandler` 只调用 `isr_handle` 或其他外设 ISR 转发函数，不写业务逻辑。
- `action` 在 ISR 上下文中执行，必须短小、确定、不可阻塞。
- `IsrOps` 不保存触发沿、payload、queue、event 或业务策略。
- 需要队列或事件时，由具体 callback 使用 ISR-safe API 投递。

