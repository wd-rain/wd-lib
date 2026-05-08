# AGENTS.md — clib-code 代理协作规范

本文件为 codex 等编码代理在 `clib-code/` 目录内进行代码生成、修改、重构时必须遵守的规范。完整背景与不一致审计见 [STYLE.md](STYLE.md)。

---

## 0. 核心原则（最高优先级）

1. **断言只在外部接口入口处和 `_module_assert_*` 辅助函数中出现**。除断言辅助函数外，任何私有静态函数（包括 `_module_default_*`、`_module_*_checked` 及其他内部逻辑）**不得**包含 `WD_ASSERT`、`if (x == NULL) return ...` 之类的参数/状态校验。所有前置条件必须在公开接口进入时一次性完成校验。
2. **不信任任何外部传入的参数**。每一个公开接口的传入参数都必须在**该函数自身入口处**完成校验，不得遗漏，也不得依赖被调用方间接校验。指针参数断言非空（NULL 是合法语义的可选参数除外，如 `void *user_data`、可选回调 `fn`），枚举/状态参数断言在合法范围内，结构体参数断言其不变式。校验方式按 §6.1 选择（必须成功用 `WD_ASSERT`，工具类用防御性 `if`），但**覆盖率**没有例外。
3. 已有模块**不做破坏性重命名**；新模块必须从一开始就保持函数前缀、struct tag、typedef 三者词法拆分一致。
4. 头文件**自包含**：`.h` 必须包含其公开接口所需的全部标准头与项目依赖。
5. 任何不确定的风格选择，参考 `platform/gpio/` 模块——该模块为整个仓库的风格基准。

---

## 1. 文件组织

### 1.1 目录结构

每个模块占一个目录，目录名 = 模块名，包含同名 `.h` 与 `.c`：

```
模块名/
├── 模块名.h
└── 模块名.c
```

### 1.2 头文件骨架

```c
#ifndef _MODULE_H_
#define _MODULE_H_

#include <stddef.h>            // 标准头文件

// 依赖
#include "../../until/until.h"

// 配置
#ifndef MODULE_CONFIG_VALUE
#define MODULE_CONFIG_VALUE 默认值
#endif

// 类型定义
typedef enum module_status_t { ... } ModuleStatus;
typedef struct module_config_t { ... } ModuleConfig;
typedef ReturnType (*module_action_fn)(...);
typedef struct module_ops_t { ... } ModuleOps;
typedef struct module_t { ... } Module;

// 接口
void module_init(Module *self, ...);
void module_deinit(Module *self);

#endif
```

### 1.3 源文件骨架

```c
#include "module.h"

// 静态辅助函数（按依赖顺序）
//   1. _module_assert_*    参数/状态校验（唯一允许写断言的私有函数）
//   2. _module_default_*   默认值构造（禁止断言）
//   3. _module_*_checked   ops 调用封装（禁止断言）
//   4. 其他内部逻辑       （禁止断言）

// 公开接口实现（顺序与 .h 中声明一致）
```

### 1.4 头文件保护

统一 `#ifndef _MODULENAME_H_` 风格。**禁止**使用 `#pragma once`。`#endif` 前不留空行，文件末尾保留恰好 1 个 `\n`。

### 1.5 Include 顺序

1. 自身头文件（仅 `.c`）
2. 标准库头文件（`<stddef.h>`、`<stdint.h>` 等）
3. 项目内部依赖（`until.h`、其他模块头文件）

项目内部依赖只包含**直接依赖**。如果模块 A 直接依赖模块 B，而模块 B 已经包含模块 C，则模块 A 不应为了使用 B 的公开接口而重复包含 C。典型例子：`event.h` 直接包含 `timer.h` 后，会通过 `timer.h` 间接获得 `until.h`，因此 `event.h` 不应再直接包含 `until.h`。

---

## 2. 命名规范

| 元素 | 风格 | 示例 |
|------|------|------|
| 文件名 | 小写，与模块同名 | `gpio.h`、`timer.c` |
| 公开函数 | `模块_动作` snake_case | `gpio_init`、`timer_start` |
| 静态函数 | `_模块_名称`（下划线前缀） | `_gpio_assert_ops`、`_timer_clear` |
| 局部变量 | snake_case | `timer_tick`、`active_head` |
| 宏常量 | UPPER_CASE | `TIMER_POOL_SIZE` |
| 函数式宏 | `WD_` 前缀 + UPPER_CASE | `WD_MIN()`、`WD_BIT_SET()` |
| 枚举值 | `WD_MODULE_PREFIX_NAME` | `WD_GPIO_MODE_INPUT`、`WD_I2C_STATUS_OK` |
| typedef 类型名 | PascalCase | `GpioConfig`、`I2cStatus` |
| struct/enum tag | `snake_case_t` | `gpio_config_t`、`i2c_status_t` |
| 函数指针 typedef | `模块_描述_fn` | `gpio_config_fn` |
| ops 结构体 | `模块_ops_t` / `ModuleOps` | `gpio_ops_t` / `GpioOps` |

**模块前缀一致性**：函数前缀、struct tag 前缀、typedef 前缀必须按相同方式拆词。例如函数命名为 `cmd_line_*`，则 tag 应为 `cmd_line_t`，typedef 应为 `CmdLine`，**不得**出现 `cmdline_t` 这种不一致拆分（cmdline 模块的现状是历史遗留，新代码不得效仿）。

---

## 3. 格式规范

- **缩进**：4 个空格，禁用 Tab。
- **花括号**：Allman 风格，`{` 独占一行（包括函数、struct、控制流）。
- **指针星号**：靠近变量名——`Gpio *self`、`const GpioOps *ops`、`Timer **current`。
- **typedef 闭合**：`}` 与类型名之间保留一个空格——`} Gpio;`，禁止 `}Gpio;`。
- **函数指针 typedef**：`(*` 与名称之间不加空格——`typedef void (*gpio_config_fn)(...)`。
- **空行**：函数之间空 1 行；同类静态函数之间空 1 行；`#endif` 前不留空行。
- **行宽**：无硬性限制，可读即可，参数过长不强制换行。
- **变量声明**：C89 风格——所有局部变量在函数/块开头集中声明，与逻辑代码用空行隔开。
- **无符号字面量**：整型字面量加 `U` 后缀——`0U`、`1U`、`UINT32_MAX`。

---

## 4. 预处理器

### 4.1 配置宏

每个模块通过可覆盖宏提供编译期配置，使用 `#ifndef` 包裹默认值：

```c
#ifndef TIMER_POOL_SIZE
#define TIMER_POOL_SIZE 16U
#endif
```

### 4.2 编译期校验

对非法配置使用 `#error`：

```c
#if TIMER_POOL_SIZE == 0U
#error "TIMER_POOL_SIZE must be greater than 0"
#endif
```

### 4.3 平台层条件编译

通过 `PLATFORM_USE_模块` 控制启用并检查依赖：

```c
#if PLATFORM_USE_I2C && !PLATFORM_USE_GPIO
#error "PLATFORM_USE_I2C requires PLATFORM_USE_GPIO"
#endif
```

### 4.4 编译器兼容宏

跨编译器属性统一通过 `until.h` 提供：`WD_WEAK`、`WD_UNUSED`、`WD_PACKED`、`WD_INLINE`、`WD_NORETURN`、`WD_DEPRECATED`、`WD_ALIGNED(n)`、`WD_SECTION(s)`、`WD_LIKELY(x)`、`WD_UNLIKELY(x)`。已包含 `until.h` 的模块**禁止**直接写 `static inline`，应使用 `WD_INLINE`。

---

## 5. 类型与结构体

### 5.1 typedef 模板

```c
// 简单别名
typedef uint32_t TimerTick;

// 枚举
typedef enum gpio_level_t
{
    WD_GPIO_LEVEL_LOW = 0,
    WD_GPIO_LEVEL_HIGH
} GpioLevel;

// 结构体
typedef struct gpio_config_t
{
    GpioMode mode;
    GpioPull pull;
    GpioSpeed speed;
} GpioConfig;

// 不透明类型前向声明
typedef struct timer_scheduler_t TimerScheduler;
```

### 5.2 Ops 结构体（策略模式）

硬件抽象、平台适配统一使用函数指针表：

```c
typedef void (*gpio_config_fn)(size_t pin, const GpioConfig *config);
typedef void (*gpio_write_fn)(size_t pin, GpioLevel level);
typedef GpioLevel (*gpio_read_fn)(size_t pin);

typedef struct gpio_ops_t
{
    gpio_config_fn config;
    gpio_write_fn write;
    gpio_read_fn read;
} GpioOps;
```

### 5.3 模块主结构体

包含 ops 指针（始终为 `const`）、标识、配置/状态：

```c
typedef struct gpio_t
{
    const GpioOps *ops;
    size_t pin;
    GpioConfig config;
} Gpio;
```

---

## 6. 错误处理与断言（含新规则）

### 6.1 校验方式选择

按函数自身定位选择，同一模块内允许混用：

- **必须成功的函数**（前置条件由调用者保证，违反即编程错误）→ 使用 `WD_ASSERT`。
- **允许容错的函数**（输入来源不确定、空调用应静默通过）→ 使用防御性 `if`：

```c
void cmd_line_init(CmdLine *self, ...)
{
    if (self == NULL)
    {
        return;
    }
    // ...
}
```

### 6.2 **断言放置规则（核心约束）**

**所有参数与状态断言必须出现在公开接口（外部接口）的入口处，或集中在 `_module_assert_*` 辅助函数中。**

私有静态函数中**禁止**出现断言或防御性校验，包括但不限于：

- `_module_default_*`：默认值构造，假定调用方已校验，直接构造返回。
- `_module_*_checked`：ops 调用封装，假定 self/参数已合法，直接转发调用。
- 其他任意非 `_assert_` 前缀的内部辅助函数。

> **理由**：避免在调用链上重复校验同一条件，使断言具有唯一权威入口；同时使私有函数职责单一、易于内联与优化。`_checked` 后缀名指代"被外部接口校验过的状态下执行"，而非"在函数内部再次校验"。

#### 反例（禁止）

```c
// ✗ 私有函数内出现断言
static GpioLevel _gpio_read_checked(Gpio *self)
{
    WD_ASSERT(self != NULL);          // ✗ 不应在此校验
    WD_ASSERT(self->ops != NULL);     // ✗ 不应在此校验
    return self->ops->read(self->pin);
}

// ✗ 私有函数内出现状态断言
static I2cStatus _i2c_write_checked(I2c *self, ...)
{
    I2cStatus status = self->ops->write(...);
    _i2c_assert_status(status);       // ✗ 状态断言也属于断言，不应在此
    return status;
}
```

#### 正例（推荐）

```c
// ✓ 断言集中在 _assert_ 辅助函数
static void _gpio_assert_ops(const GpioOps *ops)
{
    WD_ASSERT(ops != NULL);
    WD_ASSERT(ops->config != NULL);
    WD_ASSERT(ops->write != NULL);
    WD_ASSERT(ops->read != NULL);
}

static void _gpio_assert_ready(const Gpio *self)
{
    WD_ASSERT(self != NULL);
    _gpio_assert_ops(self->ops);
}

// ✓ _checked 函数不再断言，只做转发
static GpioLevel _gpio_read_checked(Gpio *self)
{
    return self->ops->read(self->pin);
}

// ✓ 公开接口在入口处一次性完成全部断言
GpioLevel gpio_read(Gpio *self)
{
    _gpio_assert_ready(self);
    return _gpio_read_checked(self);
}
```

### 6.3 返回值策略分层

| 场景 | 返回类型 | 示例 |
|------|----------|------|
| 不会运行时失败的操作 | `void` | `gpio_init`、`gpio_write` |
| 多种运行时失败模式的 IO | 专用状态枚举 | `I2cStatus`：OK/ERROR/BUSY/TIMEOUT/NACK |
| 简单成功/失败的资源管理 | `int`（0 成功，-1 失败） | `timer_start`、`timer_stop` |
| 工具类可选操作 | `int`（0 表示未匹配/无操作） | `cmd_line_exe` |

### 6.4 禁止过度检测内部不变量

公开接口**只断言外部传入参数**（`self != NULL`、`data != NULL` 等）。由库内部维护的不变量（如环形缓冲区的 `read_index < capacity`、`write_index` 一致性）**不得**在每次 API 调用时重新校验——这些条件由实现本身保证恒成立，重复检查属于无意义的运行时开销。

`_module_assert_ready` 类辅助函数**不得**包含对内部不变量的反向推导校验（例如重新计算 `write_index` 再与自身比较）。对于不含 ops 的工具类模块（如 `rb`），通常无需 `_assert_ready`，直接在公开接口入口断言 `self != NULL` 即可。

#### 反例（禁止）

```c
// ✗ 每次 API 调用都重检库内部维护的不变量
static void _rb_assert_ready(const Rb *self)
{
    WD_ASSERT(self != NULL);
    WD_ASSERT(self->buffer != NULL);           // ✗ init 后不变，不应重检
    WD_ASSERT(self->read_index < self->capacity); // ✗ 内部维护
    WD_ASSERT(self->size <= self->capacity);      // ✗ 内部维护

    size_t write_index = self->read_index + self->size;
    if (write_index >= self->capacity) write_index -= self->capacity;
    WD_ASSERT(write_index == self->write_index);  // ✗ 反向推导纯属浪费
}
```

#### 正例（推荐）

```c
// ✓ 工具类模块公开接口只断言外部入参
size_t rb_write(Rb *self, const uint8_t *data, size_t size)
{
    WD_ASSERT(self != NULL);
    WD_ASSERT(data != NULL);
    // 内部不变量由实现保证，不再校验
    ...
}
```

### 6.5 静态辅助函数三层职责

| 层次 | 命名 | 职责 | 允许断言？ |
|------|------|------|-----------|
| 校验层 | `_module_assert_*` | 参数/状态的 `WD_ASSERT` 校验 | **是（唯一允许）** |
| 默认值层 | `_module_default_*` | 构造默认配置 | 否 |
| 操作层 | `_module_*_checked` | 封装 ops 调用 | 否 |

---

### 6.6 静态函数封装原则

内部静态函数是否封装**以可读性为准**，不为封装而封装：

- **被多处调用**（≥ 2）的内部逻辑 → 提取为静态函数，避免重复代码。
- **仅被 1 处调用**的内部逻辑 → 优先内联到调用方，除非提取后显著提升可读性（如调用方函数过长、逻辑分层清晰）。
- **仅包裹 1-2 行断言**的 `_assert_data`、`_assert_storage` 类函数 → 不封装，直接在调用处写 `WD_ASSERT`。

> **理由**：单次调用的函数封装增加调用层级却不减少重复，反而让阅读者多跳一层理解逻辑；过薄的 assert 包装函数同理。

---

## 7. 注释

- **语言**：中文。
- **头文件区域注释**：固定使用 `// 依赖`、`// 配置`、`// 类型定义`、`// 接口` 分隔（**禁止**使用 `// 类定义`）。
- **行内注释**：用于关键算法或非显而易见的设计意图。
- **块注释**：用 `/* */` 解释 **为什么**，不解释 **是什么**。
- **不写注释的情况**：
  - 函数名/变量名已经足够说明意图；
  - 解释代码做了什么；
  - 引用 issue/PR 号或调用者。

---

## 8. 模块结构模式

### 8.1 self 参数

所有公开函数第一个参数为 `Module *self`。纯查询且不修改 self 状态的函数使用 `const Module *self`。

### 8.2 init/deinit 生命周期

- `init`：校验参数 → 设置默认值 → 应用配置
- `deinit`：恢复默认状态 → 释放资源 → 清空指针

```c
void gpio_init(Gpio *self, const GpioOps *ops, size_t pin)
{
    WD_ASSERT(self != NULL);
    _gpio_assert_ops(ops);

    self->ops = ops;
    self->pin = pin;
    self->config = _gpio_default_config();
    _gpio_apply_config_checked(self);
}

void gpio_deinit(Gpio *self)
{
    _gpio_assert_ready(self);

    self->config = _gpio_default_config();
    _gpio_apply_config_checked(self);

    self->ops = NULL;
    self->pin = 0U;
}
```

### 8.3 公开接口三段式函数体

```c
ReturnType module_action(Module *self, ...)
{
    // 1. 局部变量声明（C89 风格）
    SomeType local_var;

    // 2. 前置条件断言（唯一的断言位置）
    _module_assert_ready(self);
    _module_assert_xxx(参数);

    // 3. 核心逻辑（调用不含断言的 _checked / _default 辅助）
    return _module_action_checked(self, ...);
}
```

---

## 9. 提交前检查清单

代理在提交修改前应自查：

- [ ] 头文件使用 `#ifndef _MODULE_H_` 而非 `#pragma once`。
- [ ] `}` 与 typedef 名之间有空格（`} Gpio;`）。
- [ ] 指针星号靠近变量名（`Gpio *self`）。
- [ ] 函数指针 typedef 中 `(*name)` 无多余空格。
- [ ] 头文件区域注释使用 `// 类型定义`，不是 `// 类定义`。
- [ ] `#endif` 前无空行；文件末尾恰好 1 个换行符。
- [ ] 头文件自包含其公开接口所需的全部标准头。
- [ ] 项目内部头文件只包含直接依赖，不重复包含由直接依赖传递提供的头文件。
- [ ] 模块前缀（函数 / tag / typedef）词法拆分一致。
- [ ] **断言只出现在公开接口入口和 `_module_assert_*` 函数中**。
- [ ] 公开接口的每个传入参数都在入口处完成校验，无遗漏（NULL 为合法语义的可选参数除外，如 `void *user_data`、可选回调 `fn`）。
- [ ] `_module_*_checked` 与 `_module_default_*` 函数体内**无任何**断言或防御性 `if`。
- [ ] 公开接口不对库内部维护的不变量做冗余重检（§6.4）。
- [ ] 仅被 1 处调用的内部函数已评估是否应内联（§6.6）。
- [ ] 公开函数遵循"声明 → 断言 → 核心逻辑"三段式。
- [ ] 已包含 `until.h` 的模块用 `WD_INLINE` 而非 `static inline`。
- [ ] 整型字面量带 `U` 后缀。
