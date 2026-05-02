---
aliases:
  - gpio
  - gpio.h
  - platform/gpio
  - GPIO 平台层
depends:
  - "[[until]]"
tags:
  - c
  - clib
  - platform
  - gpio
---

# gpio

`gpio` 提供一个轻量级通用 GPIO platform 框架，用于把上层 GPIO 语义和具体芯片 SDK、寄存器或板级驱动隔离开。上层代码操作 `Gpio` 对象和 `GpioConfig` 配置，具体平台只需要实现最小硬件操作集合 `GpioOps`。

本文只说明 `gpio` 本身提供的接口，所有接口都使用独立标题，方便在 Obsidian 大纲中快速定位。

## 依赖关系

`[[gpio]] -> [[until]]`

`gpio` 位于 `[[platform]]` 层目录下，但源码不依赖 `platform.h`。它直接依赖 `[[until]]` 中的 `ASSERT`，用于空指针、缺失 ops 和非法枚举值检查。

## 接口总览

| 类别 | 接口 | 基本功能 | 使用要点 |
|---|---|---|---|
| 配置 | `GPIO_DEFAULT_*` | 配置初始化和释放时的默认状态 | 默认是悬空输入 |
| 类型 | `GpioLevel` | GPIO 电平语义 | `GPIO_LEVEL_LOW` 或 `GPIO_LEVEL_HIGH` |
| 类型 | `GpioMode` | GPIO 模式语义 | 输入、输出、复用、模拟 |
| 类型 | `GpioPull` | 上下拉语义 | 无上下拉、上拉、下拉 |
| 类型 | `GpioSpeed` | 输出速度语义 | 由平台映射到实际速度档 |
| 类型 | `GpioOutputType` | 输出类型语义 | 推挽或开漏 |
| 类型 | `GpioAlternate` | 复用功能语义 | `GPIO_ALTERNATE_NONE` 或 `GPIO_ALTERNATE_0` 到 `GPIO_ALTERNATE_15` |
| 类型 | `GpioConfig` | GPIO 完整配置 | 保存模式、上下拉、速度、输出类型、复用和电平 |
| Ops | `gpio_config_fn` | 平台配置函数类型 | 形参为 `pin` 和 `GpioConfig` |
| Ops | `gpio_write_fn` | 平台写电平函数类型 | 形参为 `pin` 和 `level` |
| Ops | `gpio_read_fn` | 平台读电平函数类型 | 返回 `GpioLevel` |
| Ops | `GpioOps` | 平台硬件操作集合 | `config`、`write`、`read` 都必须实现 |
| 类型 | `Gpio` | GPIO 对象 | 保存 ops、pin 和当前配置缓存 |
| 初始化 | `gpio_init(self, ops, pin)` | 初始化 GPIO 对象 | 会配置为默认悬空输入 |
| 配置 | `gpio_config(self, config)` | 整体更新 GPIO 配置 | 调用 `ops->config` |
| 读写 | `gpio_write(self, level)` | 写 GPIO 电平 | 调用 `ops->write` 并更新缓存电平 |
| 读写 | `gpio_read(self)` | 读 GPIO 电平 | 调用 `ops->read` 并更新缓存电平 |
| 读写 | `gpio_toggle(self)` | 翻转 GPIO 电平 | 由 `gpio_read` 和 `gpio_write` 实现 |
| 释放 | `gpio_deinit(self)` | 恢复默认输入并清空对象 | 调用默认 `config` 后清空对象 |

## 配置宏

### `GPIO_DEFAULT_MODE`

配置 `gpio_init` 和 `gpio_deinit` 使用的默认模式。

```c
#ifndef GPIO_DEFAULT_MODE
#define GPIO_DEFAULT_MODE GPIO_MODE_INPUT
#endif
```

默认值为 `GPIO_MODE_INPUT`。

### `GPIO_DEFAULT_PULL`

配置默认上下拉。

```c
#ifndef GPIO_DEFAULT_PULL
#define GPIO_DEFAULT_PULL GPIO_PULL_NONE
#endif
```

默认值为 `GPIO_PULL_NONE`。

### `GPIO_DEFAULT_SPEED`

配置默认速度。

```c
#ifndef GPIO_DEFAULT_SPEED
#define GPIO_DEFAULT_SPEED GPIO_SPEED_LOW
#endif
```

默认值为 `GPIO_SPEED_LOW`。

### `GPIO_DEFAULT_OUTPUT_TYPE`

配置默认输出类型。

```c
#ifndef GPIO_DEFAULT_OUTPUT_TYPE
#define GPIO_DEFAULT_OUTPUT_TYPE GPIO_OUTPUT_PUSH_PULL
#endif
```

默认值为 `GPIO_OUTPUT_PUSH_PULL`。

### `GPIO_DEFAULT_ALTERNATE`

配置默认复用功能。

```c
#ifndef GPIO_DEFAULT_ALTERNATE
#define GPIO_DEFAULT_ALTERNATE GPIO_ALTERNATE_NONE
#endif
```

默认值为 `GPIO_ALTERNATE_NONE`。

### `GPIO_DEFAULT_LEVEL`

配置默认缓存电平。

```c
#ifndef GPIO_DEFAULT_LEVEL
#define GPIO_DEFAULT_LEVEL GPIO_LEVEL_LOW
#endif
```

默认值为 `GPIO_LEVEL_LOW`。

如果需要调整默认状态，可以在包含 `gpio.h` 前定义这些宏：

```c
#define GPIO_DEFAULT_SPEED GPIO_SPEED_MEDIUM
#include "gpio.h"
```

## 类型定义

### `GpioLevel`

GPIO 电平语义。

```c
typedef enum gpio_level_t
{
    GPIO_LEVEL_LOW = 0,
    GPIO_LEVEL_HIGH
} GpioLevel;
```

平台 `read` 函数必须返回这两个值之一。`gpio_write` 也只接受这两个值。

### `GpioMode`

GPIO 模式语义。

```c
typedef enum gpio_mode_t
{
    GPIO_MODE_INPUT = 0,
    GPIO_MODE_OUTPUT,
    GPIO_MODE_ALTERNATE,
    GPIO_MODE_ANALOG
} GpioMode;
```

这些值表达通用语义，不要求和芯片 SDK 枚举值一致。平台层需要在 `config` 中显式映射。

### `GpioPull`

GPIO 上下拉语义。

```c
typedef enum gpio_pull_t
{
    GPIO_PULL_NONE = 0,
    GPIO_PULL_UP,
    GPIO_PULL_DOWN
} GpioPull;
```

如果目标芯片不支持某个上下拉配置，平台 `config` 应触发 `ASSERT`。

### `GpioSpeed`

GPIO 速度语义。

```c
typedef enum gpio_speed_t
{
    GPIO_SPEED_LOW = 0,
    GPIO_SPEED_MEDIUM,
    GPIO_SPEED_HIGH,
    GPIO_SPEED_VERY_HIGH
} GpioSpeed;
```

具体速度档位由平台映射到芯片支持的实际配置。

### `GpioOutputType`

GPIO 输出类型语义。

```c
typedef enum gpio_output_type_t
{
    GPIO_OUTPUT_PUSH_PULL = 0,
    GPIO_OUTPUT_OPEN_DRAIN
} GpioOutputType;
```

该字段通常只在输出模式或复用输出模式下有意义。

### `GpioAlternate`

GPIO 复用功能语义。

```c
typedef enum gpio_alternate_t
{
    GPIO_ALTERNATE_NONE = 0,
    GPIO_ALTERNATE_0,
    GPIO_ALTERNATE_1,
    /* ... */
    GPIO_ALTERNATE_15
} GpioAlternate;
```

`GPIO_ALTERNATE_NONE` 表示不使用复用功能。`GPIO_ALTERNATE_0` 到 `GPIO_ALTERNATE_15` 对应通用复用编号，平台层负责映射到具体芯片。

### `GpioConfig`

GPIO 完整配置。

```c
typedef struct gpio_config_t
{
    GpioMode mode;
    GpioPull pull;
    GpioSpeed speed;
    GpioOutputType output_type;
    GpioAlternate alternate;
    GpioLevel level;
} GpioConfig;
```

| 成员 | 说明 |
|---|---|
| `mode` | GPIO 模式 |
| `pull` | 上下拉配置 |
| `speed` | 输出速度 |
| `output_type` | 输出类型 |
| `alternate` | 复用功能 |
| `level` | 缓存电平 |

`gpio_config` 会复制传入的 `GpioConfig`，调用者不需要在函数返回后继续保存该结构体。

### `Gpio`

GPIO 对象类型。

```c
typedef struct gpio_t
{
    const GpioOps* ops;
    size_t pin;
    GpioConfig config;
} Gpio;
```

| 成员 | 说明 |
|---|---|
| `ops` | 指向平台硬件操作集合 |
| `pin` | 平台定义的引脚编号 |
| `config` | 当前缓存配置 |

`Gpio` 不分配堆内存，调用者负责提供对象存储。

## Ops接口

本节说明具体平台必须实现的硬件操作接口。当前 GPIO 模块要求 `GpioOps` 中的 `config`、`write` 和 `read` 全部实现；实现文件会在使用对应 ops 前执行 `ASSERT` 检查，函数指针为空会直接卡死在断言处。

### `gpio_config_fn`

平台配置函数类型。

```c
typedef void (*gpio_config_fn)(size_t pin, const GpioConfig* config);
```

该函数由具体平台实现，负责把 `GpioConfig` 映射到芯片 SDK 或寄存器配置。

该函数是必选 ops。`gpio_init`、`gpio_config`、`gpio_set_*` 和 `gpio_deinit` 都会使用它。

### `gpio_write_fn`

平台写电平函数类型。

```c
typedef void (*gpio_write_fn)(size_t pin, GpioLevel level);
```

该函数只负责写硬件电平，不负责修改 `Gpio` 对象缓存。

该函数是必选 ops。`gpio_write` 和 `gpio_toggle` 会使用它。

### `gpio_read_fn`

平台读电平函数类型。

```c
typedef GpioLevel (*gpio_read_fn)(size_t pin);
```

该函数必须返回 `GPIO_LEVEL_LOW` 或 `GPIO_LEVEL_HIGH`。

该函数是必选 ops。`gpio_read` 和 `gpio_toggle` 会使用它。

### `GpioOps`

平台硬件操作集合。

```c
typedef struct gpio_ops_t
{
    gpio_config_fn config;
    gpio_write_fn write;
    gpio_read_fn read;
} GpioOps;
```

| 成员 | 说明 |
|---|---|
| `config` | 配置 GPIO 硬件，必须实现 |
| `write` | 写 GPIO 电平，必须实现 |
| `read` | 读 GPIO 电平，必须实现 |

`GpioOps` 只保留硬件强相关原语。`toggle`、`set_pull`、`set_mode`、`set_speed`、`set_output_type`、`set_alternate` 和 `deinit` 都由 API 层组合实现。

当前 GPIO 模块认为 `GpioOps` 中的所有函数指针都是必选项。实现文件中会对实际使用到的 ops 做 `ASSERT` 检查：初始化、配置和释放要求 `config` 非空，写电平要求 `write` 非空，读电平和翻转要求 `read` 非空。平台适配时不要把未支持的能力留空；如果某颗芯片不支持某种配置，应在对应 ops 内部触发 `ASSERT` 或做平台侧约束。

## 初始化接口

### `gpio_init(self, ops, pin)`

初始化 GPIO 对象，并将引脚配置为默认状态。

```c
void gpio_init(Gpio* self, const GpioOps* ops, size_t pin);
```

示例：

```c
Gpio led;

gpio_init(&led, &led_ops, 13);
```

该函数会：

- 保存 `ops` 和 `pin`。
- 生成默认 `GpioConfig`。
- 调用 `ops->config(pin, &self->config)`。

`self`、`ops` 和 `ops->config` 必须非空，否则触发 `ASSERT`。

虽然 `gpio_init` 只会立即调用 `ops->config`，但传入的 `GpioOps` 仍应完整实现 `config`、`write` 和 `read`，否则后续读写或翻转会在断言处卡死。

## 配置接口

### `gpio_config(self, config)`

整体更新 GPIO 配置。

```c
void gpio_config(Gpio* self, const GpioConfig* config);
```

示例：

```c
GpioConfig config;

config.mode = GPIO_MODE_OUTPUT;
config.pull = GPIO_PULL_NONE;
config.speed = GPIO_SPEED_LOW;
config.output_type = GPIO_OUTPUT_PUSH_PULL;
config.alternate = GPIO_ALTERNATE_NONE;
config.level = GPIO_LEVEL_LOW;

gpio_config(&led, &config);
```

该函数会复制 `*config` 到 `self->config`，然后调用平台 `ops->config`。

`self`、`self->ops`、`self->ops->config` 和 `config` 必须有效。`config` 中的枚举值必须属于 `gpio.h` 定义的范围，否则触发 `ASSERT`。


## 读写接口

### `gpio_write(self, level)`

写 GPIO 电平。

```c
void gpio_write(Gpio* self, GpioLevel level);
```

示例：

```c
gpio_write(&led, GPIO_LEVEL_HIGH);
```

该函数会更新 `self->config.level`，然后调用 `ops->write(self->pin, level)`。

`self->ops->write` 必须非空，`level` 必须为合法 `GpioLevel`。

### `gpio_read(self)`

读 GPIO 电平。

```c
GpioLevel gpio_read(Gpio* self);
```

示例：

```c
GpioLevel level = gpio_read(&button);
```

该函数会调用 `ops->read(self->pin)`，并把读到的电平缓存到 `self->config.level`。

返回值必须是合法 `GpioLevel`，否则触发 `ASSERT`。

### `gpio_toggle(self)`

翻转 GPIO 电平。

```c
void gpio_toggle(Gpio* self);
```

该函数通过 `gpio_read` 读取当前电平，再通过 `gpio_write` 写入相反电平。`GpioOps` 不需要提供单独的 `toggle`。

由于 `gpio_toggle` 会同时调用 `read` 和 `write`，平台必须实现这两个 ops。

## 释放接口

### `gpio_deinit(self)`

释放 GPIO 对象，并将引脚恢复为默认状态。

```c
void gpio_deinit(Gpio* self);
```

示例：

```c
gpio_deinit(&led);
```

该函数会：

- 将 `self->config` 设置为默认 `GpioConfig`。
- 调用 `ops->config` 把引脚恢复为默认状态。
- 清空 `ops` 和 `pin`。
- 将缓存配置重置为默认值。

调用 `gpio_deinit` 后，不应继续使用该对象，除非重新执行 `gpio_init`。

## 平台映射规则

通用枚举不要求和芯片 SDK 枚举值一致。平台层应在 `gpio_config_fn` 中显式翻译：

```c
static void board_gpio_config(size_t pin, const GpioConfig* config)
{
    ASSERT(config != NULL);

    switch (config->mode)
    {
    case GPIO_MODE_INPUT:
        /* map to chip input mode */
        break;
    case GPIO_MODE_OUTPUT:
        /* map to chip output mode */
        break;
    case GPIO_MODE_ALTERNATE:
        /* map to chip alternate mode */
        break;
    case GPIO_MODE_ANALOG:
        /* map to chip analog mode */
        break;
    default:
        ASSERT(0);
        break;
    }

    (void)pin;
}
```

如果芯片不支持某个通用配置，平台映射函数应触发 `ASSERT`，不要把芯片私有常量泄漏到上层 API。

## 完整示例

```c
#include "clib-code/platform/gpio/gpio.h"

static GpioLevel led_level;

static void led_config(size_t pin, const GpioConfig* config)
{
    (void)pin;
    ASSERT(config != NULL);
}

static void led_write(size_t pin, GpioLevel level)
{
    (void)pin;
    led_level = level;
}

static GpioLevel led_read(size_t pin)
{
    (void)pin;
    return led_level;
}

static const GpioOps led_ops = {
    led_config,
    led_write,
    led_read
};

int main(void)
{
    Gpio led;
    GpioConfig config;

    gpio_init(&led, &led_ops, 13);

    config.mode = GPIO_MODE_OUTPUT;
    config.pull = GPIO_PULL_NONE;
    config.speed = GPIO_SPEED_LOW;
    config.output_type = GPIO_OUTPUT_PUSH_PULL;
    config.alternate = GPIO_ALTERNATE_NONE;
    config.level = GPIO_LEVEL_LOW;
    gpio_config(&led, &config);

    gpio_write(&led, GPIO_LEVEL_HIGH);
    gpio_toggle(&led);
    gpio_deinit(&led);

    return 0;
}
```

编译：

```powershell
gcc -std=c99 -Wall -Wextra -g -O0 example.c clib-code\platform\gpio\gpio.c -o example.exe
```

## 检查

当前仓库未保留独立测试文件。可以先执行语法检查确认接口声明和实现一致：

```powershell
gcc -std=c99 -Wall -Wextra -pedantic -fsyntax-only clib-code\platform\gpio\gpio.c
```

如需手动验证调用顺序，可以使用 mock `GpioOps` 记录 `config`、`write`、`read` 的调用次数，重点检查：

- `gpio_init` 会调用一次默认 `config`。
- `gpio_config` 会复制传入配置并调用 `ops->config`。
- `gpio_set_*` 会保留其他字段，只修改目标字段。
- `gpio_toggle` 只通过 `read` 和 `write` 实现。
- `gpio_deinit` 会先恢复默认配置，再清空对象。

## 通用注意事项

> [!warning]
> `gpio` 使用 `ASSERT` 暴露编程错误，不返回状态码。空指针、缺失 ops、非法枚举值会直接卡死在断言处。

- `gpio` 不分配堆内存，`Gpio` 对象由调用者提供。
- `GpioConfig` 使用通用语义枚举，平台层负责映射到具体芯片。
- `GpioOps` 只放硬件强相关原语，不放可由 API 组合实现的便捷函数。
- `GpioOps` 中的 `config`、`write`、`read` 都是必选函数指针，不应留空。
- `gpio_write` 会更新缓存电平，但不会重新调用 `ops->config`。
- `gpio_read` 会把平台返回的电平写回 `self->config.level`。
- `gpio_deinit` 后对象不再有效，继续使用会触发断言。
