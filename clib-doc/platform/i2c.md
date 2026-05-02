---
aliases:
  - i2c
  - i2c.h
  - platform/i2c
  - I2C 平台层
depends:
  - "[[gpio]]"
tags:
  - c
  - clib
  - platform
  - i2c
---

# i2c

`i2c` 提供一个轻量级硬件 I2C platform 框架，用于把上层 I2C 语义和具体芯片 SDK、寄存器或板级驱动隔离开。上层代码操作 `I2c` 对象和 `I2cConfig` 配置，具体平台只需要实现常用硬件操作集合 `I2cOps`。

首版只覆盖 master blocking 场景，支持 7 位设备地址、8/16 位寄存器地址、设备 ready 探测和地址扫描。它不实现 GPIO 模拟 I2C、slave、10 位地址、异步、中断或 DMA。

本文只说明 `i2c` 本身提供的接口，所有接口都使用独立标题，方便在 Obsidian 大纲中快速定位。

## 依赖关系

`[[i2c]] -> [[gpio]] -> [[until]]`

`i2c` 位于 `[[platform]]` 层目录下，但源码不依赖 `platform.h`。它直接依赖 `[[gpio]]`，用于可选绑定 SCL/SDA 引脚生命周期；`[[until]]` 由 `gpio` 间接引入，其中的 `ASSERT` 用于空指针、缺失 ops、非法地址和非法配置检查。

和 `[[gpio]]` 不同，I2C 的 NACK、超时、总线忙等情况属于正常运行期结果，因此通过 `I2cStatus` 返回；空对象、空 `ops`、缺失函数指针、非法地址和非法枚举值仍属于编程错误，会触发 `ASSERT`。

## 接口总览

| 类别 | 接口 | 基本功能 | 使用要点 |
|---|---|---|---|
| 配置 | `I2C_DEFAULT_CLOCK_HZ` | 默认 I2C 时钟宏 | 默认 100 kHz |
| 配置 | `I2C_SCAN_ADDRESS_MIN` | 扫描起始地址 | 默认 `0x08` |
| 配置 | `I2C_SCAN_ADDRESS_MAX` | 扫描结束地址 | 默认 `0x77` |
| 配置 | `I2C_ADDRESS_MAX` | 最大 7 位地址 | 固定 `0x7F` |
| 类型 | `I2cStatus` | I2C 运行期结果 | 成功、错误、忙、超时、NACK 等 |
| 类型 | `I2cMemAddressSize` | 寄存器地址宽度 | 8 位或 16 位 |
| 类型 | `I2cConfig` | I2C 控制器配置 | 当前保存 `clock_hz` |
| 类型 | `I2cGpioConfig` | SCL/SDA GPIO 绑定配置 | 传给 `i2c_init`，可选 |
| Ops | `i2c_config_fn` | 平台配置函数类型 | 形参为 `bus` 和 `I2cConfig` |
| Ops | `i2c_write_fn` | 平台写函数类型 | 普通 I2C 写 |
| Ops | `i2c_read_fn` | 平台读函数类型 | 普通 I2C 读 |
| Ops | `i2c_mem_write_fn` | 平台寄存器写函数类型 | 支持 8/16 位寄存器地址 |
| Ops | `i2c_mem_read_fn` | 平台寄存器读函数类型 | repeated start 等细节由平台实现 |
| Ops | `i2c_deinit_fn` | 平台释放函数类型 | 关闭或释放 I2C 控制器 |
| Ops | `I2cOps` | 平台硬件操作集合 | 所有函数指针都必须实现 |
| 类型 | `I2c` | I2C 对象 | 保存 ops、bus、配置和可选 GPIO |
| 初始化 | `i2c_init(self, ops, bus, config, gpio_cfg)` | 初始化 I2C 对象 | 可同时初始化 SCL/SDA GPIO |
| 配置 | `i2c_config(self, config)` | 重新配置 I2C | 成功后更新缓存配置 |
| 读写 | `i2c_write(self, address, data, len, timeout_ms)` | 普通 I2C 写 | 地址是 7 位地址，不含 R/W bit |
| 读写 | `i2c_read(self, address, data, len, timeout_ms)` | 普通 I2C 读 | 地址是 7 位地址，不含 R/W bit |
| 读写 | `i2c_mem_write(self, address, mem_address, mem_address_size, data, len, timeout_ms)` | I2C 寄存器写 | 调用 `ops->mem_write` |
| 读写 | `i2c_mem_read(self, address, mem_address, mem_address_size, data, len, timeout_ms)` | I2C 寄存器读 | 调用 `ops->mem_read` |
| 探测 | `i2c_is_device_ready(self, address, trials, timeout_ms)` | 检查设备是否响应 | 通过零长度写事务探测 |
| 探测 | `i2c_scan(self, addresses, capacity, found_count, timeout_ms)` | 扫描常规 7 位地址 | NACK 表示未发现设备 |
| 释放 | `i2c_deinit(self)` | 释放 I2C 对象 | 先释放控制器，再释放绑定 GPIO |

## 配置宏

### `I2C_DEFAULT_CLOCK_HZ`

配置默认 I2C 时钟宏。该值会用于对象清空后的默认缓存配置，也可供调用者构造 `I2cConfig` 时使用。

```c
#ifndef I2C_DEFAULT_CLOCK_HZ
#define I2C_DEFAULT_CLOCK_HZ 100000U
#endif
```

默认值为 `100000U`，即 100 kHz。`i2c_init` 仍要求调用者显式传入有效的 `I2cConfig`。

### `I2C_SCAN_ADDRESS_MIN`

配置 `i2c_scan` 使用的起始地址。

```c
#ifndef I2C_SCAN_ADDRESS_MIN
#define I2C_SCAN_ADDRESS_MIN 0x08U
#endif
```

默认值为 `0x08U`。

### `I2C_SCAN_ADDRESS_MAX`

配置 `i2c_scan` 使用的结束地址。

```c
#ifndef I2C_SCAN_ADDRESS_MAX
#define I2C_SCAN_ADDRESS_MAX 0x77U
#endif
```

默认值为 `0x77U`。

### `I2C_ADDRESS_MAX`

定义 7 位 I2C 地址最大值。

```c
#define I2C_ADDRESS_MAX 0x7FU
```

普通读写接口接受 `0x00` 到 `0x7F` 的 7 位地址。`i2c_scan` 默认只扫描常规设备地址 `0x08` 到 `0x77`。

如果需要调整默认扫描范围，可以在包含 `i2c.h` 前定义这些宏：

```c
#define I2C_SCAN_ADDRESS_MIN 0x10U
#define I2C_SCAN_ADDRESS_MAX 0x70U
#include "i2c.h"
```

## 类型定义

### `I2cStatus`

I2C 运行期结果。

```c
typedef enum i2c_status_t
{
    I2C_STATUS_OK = 0,
    I2C_STATUS_ERROR,
    I2C_STATUS_BUSY,
    I2C_STATUS_TIMEOUT,
    I2C_STATUS_NACK,
    I2C_STATUS_ARBITRATION_LOST,
    I2C_STATUS_OVERFLOW
} I2cStatus;
```

| 值 | 说明 |
|---|---|
| `I2C_STATUS_OK` | 操作成功 |
| `I2C_STATUS_ERROR` | 通用错误 |
| `I2C_STATUS_BUSY` | 总线或控制器忙 |
| `I2C_STATUS_TIMEOUT` | 操作超时 |
| `I2C_STATUS_NACK` | 设备未应答 |
| `I2C_STATUS_ARBITRATION_LOST` | 仲裁丢失 |
| `I2C_STATUS_OVERFLOW` | 输出容量不足，例如扫描结果数组已满 |

平台 ops 返回值必须是这些值之一，否则 API 层会触发 `ASSERT`。

### `I2cMemAddressSize`

I2C 寄存器地址宽度。

```c
typedef enum i2c_mem_address_size_t
{
    I2C_MEM_ADDRESS_SIZE_8BIT = 0,
    I2C_MEM_ADDRESS_SIZE_16BIT
} I2cMemAddressSize;
```

`I2C_MEM_ADDRESS_SIZE_8BIT` 要求 `mem_address <= 0xFF`。`I2C_MEM_ADDRESS_SIZE_16BIT` 使用完整 `uint16_t` 寄存器地址。

### `I2cConfig`

I2C 控制器配置。

```c
typedef struct i2c_config_t
{
    uint32_t clock_hz;
} I2cConfig;
```

| 成员 | 说明 |
|---|---|
| `clock_hz` | I2C 时钟频率，必须大于 0 |

`i2c_config` 会复制传入的 `I2cConfig`。调用者不需要在函数返回后继续保存该结构体。

### `I2cGpioConfig`

I2C 可选 GPIO 绑定配置。

```c
typedef struct i2c_gpio_config_t
{
    const GpioOps* gpio_ops;
    size_t scl_pin;
    size_t sda_pin;
    GpioConfig scl_config;
    GpioConfig sda_config;
} I2cGpioConfig;
```

| 成员 | 说明 |
|---|---|
| `gpio_ops` | SCL/SDA 使用的平台 GPIO ops |
| `scl_pin` | SCL 引脚编号 |
| `sda_pin` | SDA 引脚编号 |
| `scl_config` | SCL 的 GPIO 配置 |
| `sda_config` | SDA 的 GPIO 配置 |

`gpio_cfg == NULL` 表示不绑定 GPIO，只初始化 I2C 控制器。传入有效配置时，`i2c_init` 会先初始化并配置 SCL/SDA，再初始化 I2C 控制器；如果 I2C 控制器初始化失败，会回滚已经初始化的 SCL/SDA。

SCL/SDA 的复用编号、上下拉、开漏和速度由调用者通过 `GpioConfig` 明确传入，`i2c` 模块不会猜测平台默认值。

### `I2c`

I2C 对象类型。

```c
typedef struct i2c_t
{
    const I2cOps* ops;
    size_t bus;
    I2cConfig config;
    Gpio scl;
    Gpio sda;
} I2c;
```

| 成员 | 说明 |
|---|---|
| `ops` | 指向平台硬件操作集合 |
| `bus` | 平台定义的 I2C 控制器编号 |
| `config` | 当前缓存配置 |
| `scl` | 可选绑定的 SCL GPIO 对象 |
| `sda` | 可选绑定的 SDA GPIO 对象 |

`I2c` 不分配堆内存，调用者负责提供对象存储。未绑定 GPIO 时，`scl` 和 `sda` 不参与硬件配置。

## Ops接口

本节说明具体平台必须实现的硬件操作接口。当前 I2C 模块要求 `I2cOps` 中的 `config`、`write`、`read`、`mem_write`、`mem_read` 和 `deinit` 全部实现；实现文件会在初始化和使用对应 ops 前执行 `ASSERT` 检查，函数指针为空会直接卡死在断言处。

### `i2c_config_fn`

平台配置函数类型。

```c
typedef I2cStatus (*i2c_config_fn)(size_t bus, const I2cConfig* config);
```

该函数由具体平台实现，负责把 `I2cConfig` 映射到芯片 SDK 或寄存器配置。

该函数是必选 ops。`i2c_init` 和 `i2c_config` 都会使用它。

### `i2c_write_fn`

平台普通写函数类型。

```c
typedef I2cStatus (*i2c_write_fn)(size_t bus, uint8_t address, const uint8_t* data, size_t len, uint32_t timeout_ms);
```

`address` 是 7 位设备地址，不左移，不包含 R/W bit。`timeout_ms` 是本次操作的超时时间。

该函数是必选 ops。`i2c_write` 和 `i2c_is_device_ready` 会使用它。`i2c_is_device_ready` 当前通过零长度写事务探测地址，因此平台 `write` 必须支持 `data == NULL && len == 0`。

### `i2c_read_fn`

平台普通读函数类型。

```c
typedef I2cStatus (*i2c_read_fn)(size_t bus, uint8_t address, uint8_t* data, size_t len, uint32_t timeout_ms);
```

该函数是必选 ops。`i2c_read` 会使用它。

### `i2c_mem_write_fn`

平台寄存器写函数类型。

```c
typedef I2cStatus (*i2c_mem_write_fn)(size_t bus, uint8_t address, uint16_t mem_address, I2cMemAddressSize mem_address_size, const uint8_t* data, size_t len, uint32_t timeout_ms);
```

该函数是必选 ops。`mem_address_size` 表示寄存器地址宽度，平台适配层负责映射到芯片库需要的参数。

### `i2c_mem_read_fn`

平台寄存器读函数类型。

```c
typedef I2cStatus (*i2c_mem_read_fn)(size_t bus, uint8_t address, uint16_t mem_address, I2cMemAddressSize mem_address_size, uint8_t* data, size_t len, uint32_t timeout_ms);
```

该函数是必选 ops。寄存器读通常需要写寄存器地址后 repeated start 再读数据，具体时序由平台 `mem_read` 实现。

### `i2c_deinit_fn`

平台释放函数类型。

```c
typedef void (*i2c_deinit_fn)(size_t bus);
```

该函数由具体平台实现，负责关闭或释放 I2C 控制器。

该函数是必选 ops。`i2c_deinit` 会使用它。

### `I2cOps`

平台硬件操作集合。

```c
typedef struct i2c_ops_t
{
    i2c_config_fn config;
    i2c_write_fn write;
    i2c_read_fn read;
    i2c_mem_write_fn mem_write;
    i2c_mem_read_fn mem_read;
    i2c_deinit_fn deinit;
} I2cOps;
```

| 成员 | 说明 |
|---|---|
| `config` | 配置 I2C 控制器，必须实现 |
| `write` | 普通 I2C 写，必须实现 |
| `read` | 普通 I2C 读，必须实现 |
| `mem_write` | I2C 寄存器写，必须实现 |
| `mem_read` | I2C 寄存器读，必须实现 |
| `deinit` | 释放 I2C 控制器，必须实现 |

`I2cOps` 保留常用 I2C 硬件行为。地址扫描由 API 层通过 `i2c_is_device_ready` 组合实现，`i2c_is_device_ready` 又通过零长度 `write` 实现。

## 初始化接口

### `i2c_init(self, ops, bus, config, gpio_cfg)`

初始化 I2C 对象。

```c
I2cStatus i2c_init(I2c* self, const I2cOps* ops, size_t bus, const I2cConfig* config, const I2cGpioConfig* gpio_cfg);
```

示例：

```c
I2c i2c;
I2cConfig config;

config.clock_hz = I2C_DEFAULT_CLOCK_HZ;
i2c_init(&i2c, &board_i2c_ops, 0, &config, NULL);
```

该函数会：

- 清空 `I2c` 对象到默认状态。
- 保存 `ops` 和 `bus`。
- 如果 `gpio_cfg != NULL`，先初始化并配置 SCL/SDA。
- 调用 `ops->config(bus, config)` 初始化 I2C 控制器。
- 如果 I2C 控制器初始化成功，缓存 `config`。
- 如果 I2C 控制器初始化失败，回滚已经初始化的 SCL/SDA，并清空对象。

`self`、`ops`、`config` 和 `ops` 中所有函数指针必须有效，否则触发 `ASSERT`。`gpio_cfg == NULL` 表示不绑定 GPIO。

## 配置接口

### `i2c_config(self, config)`

重新配置 I2C 控制器。

```c
I2cStatus i2c_config(I2c* self, const I2cConfig* config);
```

该函数会调用平台 `ops->config(self->bus, config)`。只有平台返回 `I2C_STATUS_OK` 时，才会把 `*config` 复制到 `self->config`。

`self`、`self->ops`、`self->ops->config` 和 `config` 必须有效。`config->clock_hz` 必须大于 0。

## 读写接口

### `i2c_write(self, address, data, len, timeout_ms)`

普通 I2C 写。

```c
I2cStatus i2c_write(I2c* self, uint8_t address, const uint8_t* data, size_t len, uint32_t timeout_ms);
```

示例：

```c
uint8_t data[2] = {0x01, 0x02};

i2c_write(&i2c, 0x50, data, 2U, 100U);
```

`address` 是 7 位设备地址，不左移，不包含 R/W bit。`data` 必须非空，除非 `len == 0`。

### `i2c_read(self, address, data, len, timeout_ms)`

普通 I2C 读。

```c
I2cStatus i2c_read(I2c* self, uint8_t address, uint8_t* data, size_t len, uint32_t timeout_ms);
```

示例：

```c
uint8_t data[2];

i2c_read(&i2c, 0x50, data, 2U, 100U);
```

`data` 必须非空，除非 `len == 0`。

### `i2c_mem_write(self, address, mem_address, mem_address_size, data, len, timeout_ms)`

I2C 寄存器写。

```c
I2cStatus i2c_mem_write(I2c* self, uint8_t address, uint16_t mem_address, I2cMemAddressSize mem_address_size, const uint8_t* data, size_t len, uint32_t timeout_ms);
```

示例：

```c
uint8_t value = 0x80;

i2c_mem_write(&i2c, 0x50, 0x0010, I2C_MEM_ADDRESS_SIZE_16BIT, &value, 1U, 100U);
```

`mem_address_size` 必须是 `I2C_MEM_ADDRESS_SIZE_8BIT` 或 `I2C_MEM_ADDRESS_SIZE_16BIT`。当 `mem_address_size == I2C_MEM_ADDRESS_SIZE_8BIT` 时，`mem_address` 必须不大于 `0xFF`。

### `i2c_mem_read(self, address, mem_address, mem_address_size, data, len, timeout_ms)`

I2C 寄存器读。

```c
I2cStatus i2c_mem_read(I2c* self, uint8_t address, uint16_t mem_address, I2cMemAddressSize mem_address_size, uint8_t* data, size_t len, uint32_t timeout_ms);
```

示例：

```c
uint8_t value;

i2c_mem_read(&i2c, 0x50, 0x0010, I2C_MEM_ADDRESS_SIZE_16BIT, &value, 1U, 100U);
```

寄存器读需要 repeated start 时，由平台 `mem_read` 实现。通用 `i2c` API 不把 repeated start 细节暴露给上层调用者。

## 探测接口

### `i2c_is_device_ready(self, address, trials, timeout_ms)`

检查设备地址是否响应。

```c
I2cStatus i2c_is_device_ready(I2c* self, uint8_t address, size_t trials, uint32_t timeout_ms);
```

该函数会最多尝试 `trials` 次零长度写事务：

```c
i2c_write(self, address, NULL, 0U, timeout_ms);
```

如果任意一次返回 `I2C_STATUS_OK`，则 `i2c_is_device_ready` 返回 `I2C_STATUS_OK`。如果全部失败，则返回最后一次失败状态。

平台 `write` 必须支持 `data == NULL && len == 0` 的地址探测形式，否则 `i2c_is_device_ready` 和 `i2c_scan` 都无法使用。

### `i2c_scan(self, addresses, capacity, found_count, timeout_ms)`

扫描常规 7 位 I2C 地址。

```c
I2cStatus i2c_scan(I2c* self, uint8_t* addresses, size_t capacity, size_t* found_count, uint32_t timeout_ms);
```

该函数会从 `I2C_SCAN_ADDRESS_MIN` 扫描到 `I2C_SCAN_ADDRESS_MAX`。默认范围是 `0x08` 到 `0x77`。

扫描规则：

- `I2C_STATUS_OK`：记录地址。
- `I2C_STATUS_NACK`：认为该地址没有设备，继续扫描。
- 其他状态：立即中止扫描并返回该状态。
- `addresses` 容量不足：返回 `I2C_STATUS_OVERFLOW`。

`found_count` 必须非空。`addresses` 可以为 `NULL`，但此时 `capacity` 必须为 0；如果发现设备，会返回 `I2C_STATUS_OVERFLOW`。

## 释放接口

### `i2c_deinit(self)`

释放 I2C 对象。

```c
void i2c_deinit(I2c* self);
```

示例：

```c
i2c_deinit(&i2c);
```

该函数会：

- 调用 `ops->deinit(self->bus)` 关闭或释放 I2C 控制器。
- 如果绑定了 SCL/SDA GPIO，对两者调用 `gpio_deinit`。
- 清空 `ops`、`bus` 和 GPIO 绑定状态。

调用 `i2c_deinit` 后，不应继续使用该对象，除非重新执行 `i2c_init`。

## 平台映射规则

通用枚举和状态码不要求和芯片 SDK 枚举值一致。平台层应在 `I2cOps` 中显式翻译：

```c
static I2cStatus board_i2c_write(size_t bus, uint8_t address, const uint8_t* data, size_t len, uint32_t timeout_ms)
{
    ASSERT(address <= I2C_ADDRESS_MAX);
    ASSERT(data != NULL || len == 0U);

    if (len == 0U)
    {
        /* probe address without payload */
    }
    else
    {
        /* map to chip blocking transmit */
    }

    (void)bus;
    (void)timeout_ms;
    return I2C_STATUS_OK;
}
```

如果平台驱动返回芯片私有错误码，应在平台适配层转换为 `I2C_STATUS_*`。不要把芯片私有常量泄漏到上层 API。

## 完整示例

```c
#include "clib-code/platform/i2c/i2c.h"

static I2cStatus mock_i2c_config(size_t bus, const I2cConfig* config)
{
    (void)bus;
    ASSERT(config != NULL);
    return I2C_STATUS_OK;
}

static I2cStatus mock_i2c_write(size_t bus, uint8_t address, const uint8_t* data, size_t len, uint32_t timeout_ms)
{
    (void)bus;
    (void)address;
    (void)data;
    (void)len;
    (void)timeout_ms;
    return I2C_STATUS_OK;
}

static I2cStatus mock_i2c_read(size_t bus, uint8_t address, uint8_t* data, size_t len, uint32_t timeout_ms)
{
    (void)bus;
    (void)address;
    (void)data;
    (void)len;
    (void)timeout_ms;
    return I2C_STATUS_OK;
}

static I2cStatus mock_i2c_mem_write(size_t bus, uint8_t address, uint16_t mem_address, I2cMemAddressSize mem_address_size, const uint8_t* data, size_t len, uint32_t timeout_ms)
{
    (void)bus;
    (void)address;
    (void)mem_address;
    (void)mem_address_size;
    (void)data;
    (void)len;
    (void)timeout_ms;
    return I2C_STATUS_OK;
}

static I2cStatus mock_i2c_mem_read(size_t bus, uint8_t address, uint16_t mem_address, I2cMemAddressSize mem_address_size, uint8_t* data, size_t len, uint32_t timeout_ms)
{
    (void)bus;
    (void)address;
    (void)mem_address;
    (void)mem_address_size;
    (void)data;
    (void)len;
    (void)timeout_ms;
    return I2C_STATUS_OK;
}

static void mock_i2c_deinit(size_t bus)
{
    (void)bus;
}

static const I2cOps mock_i2c_ops = {
    mock_i2c_config,
    mock_i2c_write,
    mock_i2c_read,
    mock_i2c_mem_write,
    mock_i2c_mem_read,
    mock_i2c_deinit
};

int main(void)
{
    I2c i2c;
    I2cConfig config;
    uint8_t value;

    config.clock_hz = I2C_DEFAULT_CLOCK_HZ;

    if (i2c_init(&i2c, &mock_i2c_ops, 0, &config, NULL) == I2C_STATUS_OK)
    {
        i2c_mem_read(&i2c, 0x50, 0x0000, I2C_MEM_ADDRESS_SIZE_16BIT, &value, 1U, 100U);
        value = 0x5AU;
        i2c_mem_write(&i2c, 0x50, 0x0000, I2C_MEM_ADDRESS_SIZE_16BIT, &value, 1U, 100U);
        i2c_deinit(&i2c);
    }

    return 0;
}
```

编译：

```powershell
gcc -std=c99 -Wall -Wextra -g -O0 example.c clib-code\platform\i2c\i2c.c clib-code\platform\gpio\gpio.c -o example.exe
```

## 检查

可以先执行语法检查确认接口声明和实现一致：

```powershell
gcc -std=c99 -Wall -Wextra -pedantic -fsyntax-only clib-code\platform\i2c\i2c.c
```

如需手动验证调用顺序，可以使用 mock `I2cOps` 和 `GpioOps` 记录调用次数，重点检查：

- 不绑定 GPIO 时，`i2c_init` 只调用一次 `ops->config`。
- 绑定 GPIO 时，`i2c_init` 先初始化 SCL/SDA，再调用 `ops->config`。
- `ops->config` 返回失败时，已初始化的 SCL/SDA 会被回滚。
- `i2c_config` 只在返回 `I2C_STATUS_OK` 时更新缓存配置。
- `i2c_write`、`i2c_read`、`i2c_mem_write` 和 `i2c_mem_read` 会原样透传参数到对应 ops。
- `i2c_is_device_ready` 会通过零长度写事务重试。
- `i2c_scan` 会跳过 NACK，并在非 NACK 错误时中止扫描。

## 通用注意事项

> [!warning]
> `i2c` 使用 `ASSERT` 暴露编程错误，并使用 `I2cStatus` 表达运行期 I2C 结果。空指针、缺失 ops、非法地址、非法配置会直接卡死在断言处；NACK、超时、忙等结果由状态码返回。

- `i2c` 不分配堆内存，`I2c` 对象由调用者提供。
- `I2cOps` 中的 `config`、`write`、`read`、`mem_write`、`mem_read` 和 `deinit` 都是必选函数指针，不应留空。
- 所有设备地址都是 7 位地址，不左移，不包含 R/W bit。
- `i2c_is_device_ready` 依赖零长度 `write`，平台 `write` 应支持 `data == NULL && len == 0`。
- `i2c_mem_read` 的 repeated start 等细节由平台 `mem_read` 实现。
- `i2c_config` 只有在平台返回 `I2C_STATUS_OK` 时才更新缓存配置。
- `gpio_cfg == NULL` 表示不绑定 GPIO；绑定 GPIO 时，SCL/SDA 生命周期由 `i2c_init` 和 `i2c_deinit` 管理。
- `i2c_deinit` 后对象不再有效，继续使用会触发断言。
