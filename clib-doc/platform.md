---
aliases:
  - platform
  - platform layer
  - 平台层
depends:
  - "[[gpio]]"
  - "[[i2c]]"
tags:
  - c
  - clib
  - platform
---

# platform

`platform` 是 clib 中面向硬件差异的适配层。它不直接绑定某一颗芯片，而是定义稳定的上层语义和最小硬件操作集合，由具体平台文件把这些语义映射到芯片 SDK、寄存器或板级驱动。

## 依赖关系

`clib-code/platform/platform.h` 是 platform 层的可裁剪聚合入口，默认包含 `gpio/gpio.h` 和 `i2c/i2c.h`。它本身不直接包含 `until.h`；`[[until]]` 由具体子模块按需要引入。

`[[i2c]]` 当前依赖 `[[gpio]]` 中的类型，用于可选绑定 SCL/SDA 引脚生命周期。因此启用 `PLATFORM_USE_I2C` 时必须同时启用 `PLATFORM_USE_GPIO`。

模块索引：

- `[[gpio]]`：通用 GPIO platform 框架。
- `[[i2c]]`：通用 I2C platform 框架。

## 配置宏

### `PLATFORM_USE_GPIO`

控制 `platform.h` 是否包含 GPIO 抽象层。

```c
#ifndef PLATFORM_USE_GPIO
#define PLATFORM_USE_GPIO 1
#endif
```

默认值为 `1`，表示启用 `[[gpio]]`。如果项目不需要 GPIO，可以在包含 `platform.h` 前定义为 `0`：

```c
#define PLATFORM_USE_GPIO 0
#define PLATFORM_USE_I2C 0
#include "platform.h"
```

该宏只能设置为 `0` 或 `1`，其他值会触发编译错误。

### `PLATFORM_USE_I2C`

控制 `platform.h` 是否包含 I2C 抽象层。

```c
#ifndef PLATFORM_USE_I2C
#define PLATFORM_USE_I2C 1
#endif
```

默认值为 `1`，表示启用 `[[i2c]]`。如果项目不需要 I2C，可以在包含 `platform.h` 前定义为 `0`：

```c
#define PLATFORM_USE_I2C 0
#include "platform.h"
```

该宏只能设置为 `0` 或 `1`。由于 `[[i2c]]` 依赖 `[[gpio]]`，不能设置 `PLATFORM_USE_I2C == 1` 且 `PLATFORM_USE_GPIO == 0`。

## 分层原则

platform 层只把真正硬件相关的动作放进 `ops`。凡是能通过已有 `ops` 组合出来的能力，都应作为模块 API 实现，而不是继续扩大 `ops`。

这种设计有两个目的：

- 上层 API 稳定，业务代码只依赖通用语义。
- 平台适配文件很薄，只负责把通用语义翻译成芯片实际配置。

## 最小 ops 原则

以 `[[gpio]]` 为例，硬件强相关原语只有配置、写电平和读电平。`toggle`、`set_pull`、`set_mode`、`deinit` 这类能力可以由配置、读、写组合实现，因此放在 GPIO API 层。

如果某个芯片不支持某项通用语义，例如不支持开漏输出或某个复用编号，平台 `config` 函数应在映射时触发 `ASSERT`，而不是让上层传芯片私有常量绕过框架。

## 断言策略

platform 层不使用状态码表达编程错误。空对象、空 `ops`、缺失函数指针、非法枚举值都属于调用者或适配层错误，应直接断言。

运行期硬件异常如果需要恢复机制，应由具体平台驱动自行处理；通用 platform 框架只定义接口边界和调用约束。像 `[[i2c]]` 这类天然存在 NACK、超时、总线忙的通信接口，可以用状态码表达这些正常运行期结果，但仍应使用 `ASSERT` 暴露空指针和非法配置。
