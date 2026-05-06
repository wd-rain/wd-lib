---
aliases:
  - gpio_isr
  - gpio_isr.h
  - platform/gpio_isr
  - GpioIsr
  - GPIO ISR
  - GPIO 中断
depends:
  - "[[gpio]]"
  - "[[isr]]"
tags:
  - c
  - clib
  - platform
  - gpio
  - isr
---

# gpio_isr

`gpio_isr` 提供一个轻量级 GPIO 中断扩展，用于把普通 `Gpio` 对象和通用 `Isr` 基类组合起来。普通 GPIO 配置、读、写仍由 `[[gpio]]` 负责；GPIO 中断触发沿、pending、ack 和 callback 由 `gpio_isr` 负责。

本文只说明 `gpio_isr` 本身提供的接口，所有接口都使用独立标题，方便在 Obsidian 大纲中快速定位。

## 依赖关系

`[[gpio_isr]] -> [[gpio]] -> [[until]]`

`[[gpio_isr]] -> [[isr]] -> [[until]]`

`gpio_isr` 位于 `[[platform]]` 层目录下，但源码不依赖 `platform.h`。它直接依赖 `gpio.h` 和 `isr.h`。

## 接口总览

| 类别 | 接口 | 基本功能 | 使用要点 |
|---|---|---|---|
| 类型 | `GpioIsrTrigger` | GPIO 中断触发沿语义 | 上升沿、下降沿、双边沿 |
| 类型 | `GpioIsrConfig` | GPIO 中断配置 | 当前只保存 `trigger` |
| 类型 | `GpioIsr` | GPIO ISR 对象 | 内嵌 `Isr`，绑定 `Gpio` |
| 类型 | `gpio_isr_callback_fn` | GPIO ISR callback 类型 | 在 ISR action 中调用 |
| Ops | `gpio_isr_config_fn` | 平台 GPIO 中断配置函数类型 | 配置触发沿和 GPIO 中断源 |
| Ops | `GpioIsrOps` | GPIO ISR 平台操作集合 | 包含 GPIO 专属 `config` 和通用 `IsrOps` |
| 初始化 | `gpio_isr_init(self, gpio, ops, config, callback)` | 初始化 GPIO ISR 对象 | 不自动使能中断 |
| 配置 | `gpio_isr_config(self, config)` | 重新配置 GPIO 中断触发 | 不缓存配置 |
| 转换 | `gpio_isr_base(self)` | 获取内嵌 `Isr*` | 供 `isr_handle` 使用 |
| 回调 | `gpio_isr_set_callback(self, callback)` | 修改应用 callback | 可在初始化后绑定 |
| 启停 | `gpio_isr_enable(self)` | 使能 GPIO 中断 | 委托 `isr_enable` |
| 启停 | `gpio_isr_disable(self)` | 关闭 GPIO 中断 | 委托 `isr_disable` |
| 释放 | `gpio_isr_deinit(self)` | 释放 GPIO ISR 对象 | 调用后对象失效 |

## 类型定义

### `GpioIsrTrigger`

GPIO 中断触发沿语义。

```c
typedef enum gpio_isr_trigger_t
{
    WD_GPIO_ISR_RISING = 0,
    WD_GPIO_ISR_FALLING,
    WD_GPIO_ISR_BOTH
} GpioIsrTrigger;
```

这些值是通用语义，不要求和芯片 SDK 枚举值一致。平台层需要显式映射。

### `GpioIsrConfig`

GPIO 中断配置。

```c
typedef struct gpio_isr_config_t
{
    GpioIsrTrigger trigger;
} GpioIsrConfig;
```

| 成员 | 说明 |
|---|---|
| `trigger` | GPIO 中断触发沿 |

`GpioIsrConfig` 是硬件配置输入。`gpio_isr_init` 和 `gpio_isr_config` 会把它传给平台 `ops->config`，但 `GpioIsr` 对象不会缓存该配置。

### `gpio_isr_callback_fn`

GPIO ISR callback 函数类型。

```c
typedef struct gpio_isr_t GpioIsr;
typedef void (*gpio_isr_callback_fn)(GpioIsr *self);
```

callback 在 ISR 上下文中执行。它决定后续动作，例如直接执行短动作、置 flag、投递 ISR-safe queue 或 event。

### `GpioIsr`

GPIO ISR 对象类型。

```c
struct gpio_isr_t
{
    Isr isr;
    Gpio *gpio;
    const GpioIsrOps *ops;
    gpio_isr_callback_fn callback;
};
```

| 成员 | 说明 |
|---|---|
| `isr` | 内嵌通用 `Isr` 基类 |
| `gpio` | 被绑定的普通 GPIO 对象 |
| `ops` | GPIO ISR 平台操作集合 |
| `callback` | 应用层中断回调 |

`GpioIsr` 不保存 `level`、`timestamp` 或 `config`。如果 callback 需要当前电平，应在 callback 中调用 `gpio_read(self->gpio)`。

## Ops接口

本节说明具体平台必须实现的 GPIO 中断硬件操作接口。GPIO 的触发沿配置属于 `GpioIsrOps.config`；通用 enable、disable、pending、ack 属于内嵌 `IsrOps`。

### `gpio_isr_config_fn`

平台 GPIO 中断配置函数类型。

```c
typedef void (*gpio_isr_config_fn)(size_t pin, const GpioIsrConfig *config);
```

该函数负责把 `GpioIsrConfig` 映射到具体芯片的 GPIO 中断配置，例如选择端口 source、配置上升沿/下降沿和清除初始 pending。

### `GpioIsrOps`

GPIO ISR 平台操作集合。

```c
typedef struct gpio_isr_ops_t
{
    gpio_isr_config_fn config;
    IsrOps isr;
} GpioIsrOps;
```

| 成员 | 说明 |
|---|---|
| `config` | 配置 GPIO 中断触发和 GPIO 专属硬件 |
| `isr.enable` | 使能 GPIO 中断源 |
| `isr.disable` | 关闭 GPIO 中断源 |
| `isr.pending` | 判断 GPIO 中断源是否 pending |
| `isr.ack` | 清除 GPIO 中断 pending，可选 |

`config` 和 `isr.pending` 必须实现。`isr.ack` 可以为 `NULL`，但大多数边沿触发中断都应提供 ack。

## 初始化接口

### `gpio_isr_init(self, gpio, ops, config, callback)`

初始化 GPIO ISR 对象。

```c
void gpio_isr_init(GpioIsr *self, Gpio *gpio, const GpioIsrOps *ops, const GpioIsrConfig *config, gpio_isr_callback_fn callback);
```

示例：

```c
Gpio button_gpio;
GpioIsr button_isr;
GpioIsrConfig isr_config;

isr_config.trigger = WD_GPIO_ISR_FALLING;
gpio_isr_init(&button_isr, &button_gpio, &board_gpio_isr_ops, &isr_config, button_callback);
```

该函数会：

- 保存 `gpio`、`ops` 和 `callback`。
- 调用 `ops->config(gpio->pin, config)` 配置 GPIO 中断硬件。
- 调用 `isr_init(&self->isr, &ops->isr, gpio->pin, action)` 绑定通用 ISR 基类。

`self`、`gpio`、`ops`、`ops->config`、`ops->isr.pending` 和 `config` 必须有效。

## 配置接口

### `gpio_isr_config(self, config)`

重新配置 GPIO 中断触发。

```c
void gpio_isr_config(GpioIsr *self, const GpioIsrConfig *config);
```

该函数会调用 `self->ops->config(self->gpio->pin, config)`。`GpioIsr` 不缓存 `config`，因此调用者如需保存配置，应在自己的对象中保存。

## 转换接口

### `gpio_isr_base(self)`

获取内嵌 `Isr*`。

```c
Isr *gpio_isr_base(GpioIsr *self);
```

该函数用于真实 `IRQHandler` 中：

```c
void EXTI3_IRQHandler(void)
{
    isr_handle(gpio_isr_base(&button_isr));
}
```

## 回调接口

### `gpio_isr_set_callback(self, callback)`

修改 GPIO ISR callback。

```c
void gpio_isr_set_callback(GpioIsr *self, gpio_isr_callback_fn callback);
```

该函数可用于先初始化中断对象，再由应用层绑定具体动作。`callback == NULL` 表示中断触发后只执行 pending/ack，不执行应用回调。

callback 示例：

```c
static volatile bool button_changed;

static void button_callback(GpioIsr *self)
{
    (void)self;
    button_changed = true;
}
```

如果需要读取当前电平：

```c
static void button_callback(GpioIsr *self)
{
    GpioLevel level;

    level = gpio_read(self->gpio);
    (void)level;
}
```

## 启停接口

### `gpio_isr_enable(self)`

使能 GPIO 中断。

```c
void gpio_isr_enable(GpioIsr *self);
```

该函数委托 `isr_enable(&self->isr)` 实现。

### `gpio_isr_disable(self)`

关闭 GPIO 中断。

```c
void gpio_isr_disable(GpioIsr *self);
```

该函数委托 `isr_disable(&self->isr)` 实现。

## 释放接口

### `gpio_isr_deinit(self)`

释放 GPIO ISR 对象。

```c
void gpio_isr_deinit(GpioIsr *self);
```

该函数会先释放内嵌 `Isr`，再清空 `gpio`、`ops` 和 `callback`。调用后对象不再有效，除非重新执行 `gpio_isr_init`。

## 平台映射规则

平台层应把 `pin` 映射到具体芯片的 GPIO 中断机制。例如 EXTI 风格平台通常需要：

- 根据 pin 选择端口 source。
- 根据 `GpioIsrTrigger` 配置上升沿、下降沿或双边沿。
- 在 enable 前清 pending。
- 在 pending 中检查中断 mask 和 pending 标志。
- 在 ack 中清 pending 标志。

示例：

```c
static void board_gpio_isr_config(size_t pin, const GpioIsrConfig *config)
{
    ASSERT(config != NULL);

    switch (config->trigger)
    {
    case WD_GPIO_ISR_RISING:
        /* map to rising edge */
        break;
    case WD_GPIO_ISR_FALLING:
        /* map to falling edge */
        break;
    case WD_GPIO_ISR_BOTH:
        /* map to both edges */
        break;
    default:
        ASSERT(0);
        break;
    }

    (void)pin;
}
```

## 完整示例

```c
#include <stdbool.h>

#include "clib-code/platform/gpio/gpio.h"
#include "clib-code/platform/gpio/gpio_isr.h"

static bool pending_flag;
static volatile bool button_changed;
static Gpio button_gpio;
static GpioIsr button_isr;

static void mock_gpio_config(size_t pin, const GpioConfig *config)
{
    (void)pin;
    ASSERT(config != NULL);
}

static void mock_gpio_write(size_t pin, GpioLevel level)
{
    (void)pin;
    (void)level;
}

static GpioLevel mock_gpio_read(size_t pin)
{
    (void)pin;
    return WD_GPIO_LEVEL_HIGH;
}

static void mock_gpio_isr_config(size_t pin, const GpioIsrConfig *config)
{
    (void)pin;
    ASSERT(config != NULL);
}

static void mock_gpio_isr_enable(size_t pin)
{
    (void)pin;
}

static void mock_gpio_isr_disable(size_t pin)
{
    (void)pin;
}

static bool mock_gpio_isr_pending(size_t pin)
{
    (void)pin;
    return pending_flag;
}

static void mock_gpio_isr_ack(size_t pin)
{
    (void)pin;
    pending_flag = false;
}

static void button_callback(GpioIsr *self)
{
    (void)self;
    button_changed = true;
}

static const GpioOps mock_gpio_ops = {
    mock_gpio_config,
    mock_gpio_write,
    mock_gpio_read
};

static const GpioIsrOps mock_gpio_isr_ops = {
    mock_gpio_isr_config,
    {
        mock_gpio_isr_enable,
        mock_gpio_isr_disable,
        mock_gpio_isr_pending,
        mock_gpio_isr_ack
    }
};

void EXTI3_IRQHandler(void)
{
    isr_handle(gpio_isr_base(&button_isr));
}

int main(void)
{
    GpioConfig pin_config;
    GpioIsrConfig isr_config;

    pin_config.mode = WD_GPIO_MODE_INPUT;
    pin_config.pull = WD_GPIO_PULL_UP;
    pin_config.speed = WD_GPIO_SPEED_LOW;
    pin_config.output_type = WD_GPIO_OUTPUT_PUSH_PULL;
    pin_config.alternate = WD_GPIO_ALTERNATE_NONE;
    pin_config.level = WD_GPIO_LEVEL_HIGH;

    gpio_init(&button_gpio, &mock_gpio_ops, 3U);
    gpio_config(&button_gpio, &pin_config);

    isr_config.trigger = WD_GPIO_ISR_FALLING;
    gpio_isr_init(&button_isr, &button_gpio, &mock_gpio_isr_ops, &isr_config, button_callback);
    gpio_isr_enable(&button_isr);

    pending_flag = true;
    EXTI3_IRQHandler();

    return button_changed ? 0 : 1;
}
```

编译：

```powershell
gcc -std=c99 -Wall -Wextra -g -O0 example.c clib-code\platform\isr\isr.c clib-code\platform\gpio\gpio.c clib-code\platform\gpio\gpio_isr.c -Iclib-code -o example.exe
```

## 检查

可以先执行语法检查确认接口声明和实现一致：

```powershell
gcc -std=c99 -Wall -Wextra -pedantic -fsyntax-only clib-code\platform\isr\isr.c clib-code\platform\gpio\gpio.c clib-code\platform\gpio\gpio_isr.c -Iclib-code
```

手动验证时，重点检查：

- `pending == false` 时，不调用 `ack` 和 `callback`。
- `pending == true` 时，顺序为 `pending -> ack -> callback`。
- `callback == NULL` 时，仍能完成 pending 和 ack。
- `gpio_isr_config` 会调用平台 `config`，但不会缓存配置。
- `gpio_isr_base` 返回内嵌 `Isr*`。

## 通用注意事项

> [!warning]
> `gpio_isr` 使用 `ASSERT` 暴露编程错误，不返回状态码。空对象、空 `gpio`、空 `ops`、缺失 `config`、缺失 `pending` 或非法 trigger 会直接卡死在断言处。

- GPIO 中断能力不进入 `GpioOps`，应使用独立 `GpioIsrOps`。
- `GpioIsr` 不缓存 `level`、`timestamp` 或 `config`。
- callback 在 ISR 上下文中执行，必须短小、确定、不可阻塞。
- 需要电平时，在 callback 中调用 `gpio_read(self->gpio)`。
- 需要队列或事件时，由 callback 使用 ISR-safe API 投递。
