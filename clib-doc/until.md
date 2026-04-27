---
aliases:
  - until
  - until.h
  - C 工具宏
tags:
  - c
  - clib
  - util
  - header
---

# until

`until.h` 提供一组轻量级 C 工具宏，主要用于数值处理、位操作、结构体成员定位、预处理器辅助和编译器属性适配。本文只说明 `until` 本身提供的接口，所有接口都使用独立标题，方便在 Obsidian 大纲中快速定位。

## 接口总览

| 类别 | 接口 | 基本功能 | 使用要点 |
|---|---|---|---|
| 数值处理 | `min(a, b)` | 返回两个值中的较小值 | 参数可能被求值多次 |
| 数值处理 | `max(a, b)` | 返回两个值中的较大值 | 参数可能被求值多次 |
| 数值处理 | `constrain(x, a, b)` | 将 `x` 限制在 `[a, b]` 内 | 调用者应保证 `a <= b` |
| 数值处理 | `linear_map(x, x1, x2, y1, y2)` | 将 `x` 从 `[x1, x2]` 映射到 `[y1, y2]` 并限幅 | 必须保证 `x1 != x2` |
| 位操作 | `BIT(n)` | 生成第 `n` 位为 1 的掩码 | `n` 需在有效位宽内 |
| 位操作 | `SET_BIT(x, n)` | 将 `x` 的第 `n` 位置 1 | 会修改 `x`，要求可修改左值 |
| 位操作 | `CLEAR_BIT(x, n)` | 将 `x` 的第 `n` 位清 0 | 会修改 `x`，要求可修改左值 |
| 位操作 | `TOGGLE_BIT(x, n)` | 翻转 `x` 的第 `n` 位 | 会修改 `x`，要求可修改左值 |
| 位操作 | `READ_BIT(x, n)` | 读取 `x` 的第 `n` 位 | 返回 `0` 或 `1`，不修改 `x` |
| 位操作 | `REG_SET(x, cmark, smark)` | 清除 `cmark` 指定位，再设置 `smark` 指定位 | 会修改 `x`，常用于寄存器字段 |
| 结构体成员定位 | `OFFSETOF(type, member)` | 获取成员相对结构体起始地址的偏移 | 语义接近标准 `offsetof` |
| 结构体成员定位 | `CONTAINER_OF(ptr, type, member)` | 由成员指针反推外层结构体指针 | `ptr` 必须指向 `type.member` |
| 预处理器辅助 | `STRINGIFY(x)` | 将宏参数展开后转为字符串 | 常用于版本号、提示文本、`_Pragma` |
| 预处理器辅助 | `CONCAT(a, b)` | 将两个标识符拼接成一个标识符 | 常用于生成变量名或函数名 |
| 编译器属性 | `WEAK` | 声明弱符号 | 可被同名强符号覆盖 |
| 编译器属性 | `UNUSED` | 标记对象可能未使用 | 用于减少未使用告警 |
| 编译器属性 | `PACKED` | 请求紧凑结构体布局 | 可能产生非对齐访问 |
| 编译器属性 | `INLINE` | 提供跨编译器内联声明辅助 | MSVC 分支不含 `static` |
| 编译器属性 | `NORETURN` | 标记函数不会返回调用点 | 用于致命错误、复位、退出路径 |
| 编译器属性 | `DEPRECATED` | 标记接口已弃用 | 调用时通常触发编译告警 |
| 编译器属性 | `ALIGNED(n)` | 指定对象或类型的对齐字节数 | `n` 通常应为 2 的幂 |
| 编译器属性 | `SECTION(s)` | 将对象或函数放入指定段 | 段名需匹配链接配置 |
| 分支预测 | `LIKELY(x)` | 提示表达式大概率为真 | GCC / Clang 下启用预测提示 |
| 分支预测 | `UNLIKELY(x)` | 提示表达式大概率为假 | 常用于错误路径和异常分支 |

## 数值处理宏

### `min(a, b)`

返回 `a` 和 `b` 中较小的值。

```c
int value = min(adc_value, threshold);
```

注意：这是宏，参数可能被求值多次，避免传入 `i++` 这类带副作用的表达式。

### `max(a, b)`

返回 `a` 和 `b` 中较大的值。

```c
int value = max(adc_value, threshold);
```

注意：这是宏，参数可能被求值多次。

### `constrain(x, a, b)`

将 `x` 限制在闭区间 `[a, b]` 内。

```c
int duty = constrain(raw_duty, 0, 100);
```

该宏内部调用 `min` 和 `max`。调用者应保证 `a <= b`。

### `linear_map(x, x1, x2, y1, y2)`

将 `x` 从输入区间 `[x1, x2]` 线性映射到输出区间 `[y1, y2]`，然后使用 `constrain` 对结果限幅。

```c
int percent = linear_map(adc, 0, 4095, 0, 100);
```

调用者必须保证 `x1 != x2`，否则会发生除零。当前实现更适合 `x2 > x1` 且 `y2 > y1` 的递增映射。

## 位操作宏

### `BIT(n)`

生成第 `n` 位为 1 的无符号掩码。

```c
uint32_t mask = BIT(3);  // 0b1000
```

当前实现使用 `1U << n`，因此 `n` 不应超过 `unsigned int` 的有效位宽。

### `SET_BIT(x, n)`

将 `x` 的第 `n` 位置 1。

```c
uint32_t flags = 0;
SET_BIT(flags, 2);
```

`x` 必须是可修改左值，例如变量、寄存器或结构体成员。

### `CLEAR_BIT(x, n)`

将 `x` 的第 `n` 位清 0。

```c
CLEAR_BIT(flags, 2);
```

`x` 必须是可修改左值。

### `TOGGLE_BIT(x, n)`

翻转 `x` 的第 `n` 位。

```c
TOGGLE_BIT(flags, 0);
```

`x` 必须是可修改左值。

### `READ_BIT(x, n)`

读取 `x` 的第 `n` 位，返回 `0` 或 `1`。

```c
if (READ_BIT(flags, 0)) {
    // bit 0 is set
}
```

该宏不会修改 `x`。

### `REG_SET(x, cmark, smark)`

先清除 `cmark` 指定的位，再设置 `smark` 指定的位。

```c
REG_SET(reg, BIT(0) | BIT(1), BIT(1));
```

等价逻辑：

```c
x = (x & ~cmark) | smark;
```

常用于寄存器字段更新。`x` 必须是可修改左值。

## 结构体成员定位宏

### `OFFSETOF(type, member)`

返回 `member` 相对结构体 `type` 起始地址的字节偏移。

```c
unsigned long offset = OFFSETOF(struct message, payload);
```

这个宏用于结构体布局计算。若项目可以使用标准库，语义上接近 `<stddef.h>` 中的 `offsetof`。

### `CONTAINER_OF(ptr, type, member)`

由结构体成员指针 `ptr` 反推出包含该成员的外层结构体指针。

```c
struct node {
    struct list_head link;
    int value;
};

struct list_head *link = get_link();
struct node *node = CONTAINER_OF(link, struct node, link);
```

调用者需要保证 `ptr` 的确指向 `type.member`，否则得到的外层指针无效。

## 预处理器辅助宏

### `STRINGIFY(x)`

先展开宏参数，再将展开结果字符串化。

```c
#define VERSION_MAJOR 1
const char *text = STRINGIFY(VERSION_MAJOR);  // "1"
```

常用于生成版本号字符串、编译期提示文本或 `_Pragma` 参数。

### `CONCAT(a, b)`

先展开参数，再拼接两个标识符。

```c
#define ID 1
int CONCAT(sensor_, ID);  // sensor_1
```

常用于生成带编号的变量名、函数名或静态对象名。

## 编译器属性宏

### `WEAK`

声明弱符号。弱符号可以被其他同名强符号覆盖，常用于提供可重写的默认函数实现。

| 编译器 | 展开 |
|---|---|
| GCC / Clang | `__attribute__((weak))` |
| ARMCC | `__attribute__((weak))` |
| IAR | `__weak` |
| MSVC | `__declspec(selectany)` |
| 未识别编译器 | 空 |

```c
WEAK void board_init(void)
{
}
```

### `UNUSED`

标记变量、函数或参数可能未使用，用于减少编译器告警。

| 编译器 | 展开 |
|---|---|
| GCC / Clang | `__attribute__((unused))` |
| ARMCC | `__attribute__((unused))` |
| IAR | 空 |
| MSVC | 空 |
| 未识别编译器 | 空 |

```c
static UNUSED int debug_counter;
```

### `PACKED`

请求编译器使用紧凑布局，减少结构体成员之间的填充字节。常用于通信帧、文件格式和寄存器映射结构体。

| 编译器 | 展开 |
|---|---|
| GCC / Clang | `__attribute__((packed))` |
| ARMCC | `__attribute__((packed))` |
| IAR | `__packed` |
| MSVC | 空 |
| 未识别编译器 | 空 |

```c
PACKED struct frame {
    uint8_t head;
    uint16_t length;
};
```

注意：紧凑结构体可能导致非对齐访问，部分 MCU 上需要额外小心。

### `INLINE`

提供跨编译器的内联声明辅助。

| 编译器 | 展开 |
|---|---|
| GCC / Clang | `static inline __attribute__((always_inline))` |
| ARMCC | `static __forceinline` |
| IAR | `static inline` |
| MSVC | `__forceinline` |
| 未识别编译器 | `static inline` |

```c
INLINE int add_one(int value)
{
    return value + 1;
}
```

注意：MSVC 分支当前只展开为 `__forceinline`，不包含 `static`。

### `NORETURN`

标记函数不会返回调用点，常用于死循环、复位、异常退出或致命错误处理函数。

| 编译器 | 展开 |
|---|---|
| GCC / Clang | `__attribute__((noreturn))` |
| ARMCC | `__attribute__((noreturn))` |
| IAR | `__noreturn` |
| MSVC | `__declspec(noreturn)` |
| 未识别编译器 | 空 |

```c
NORETURN void fatal_error(void);
```

### `DEPRECATED`

标记接口已弃用。调用被标记的接口时，编译器通常会给出告警。

| 编译器 | 展开 |
|---|---|
| GCC / Clang | `__attribute__((deprecated))` |
| ARMCC | `__attribute__((deprecated))` |
| IAR | 空 |
| MSVC | `__declspec(deprecated)` |
| 未识别编译器 | 空 |

```c
DEPRECATED void old_api(void);
```

### `ALIGNED(n)`

指定变量、类型或对象的对齐字节数。

| 编译器 | 展开 |
|---|---|
| GCC / Clang | `__attribute__((aligned(n)))` |
| ARMCC | `__attribute__((aligned(n)))` |
| IAR | `_Pragma(STRINGIFY(data_alignment=n))` |
| MSVC | `__declspec(align(n))` |
| 未识别编译器 | 空 |

```c
ALIGNED(4) uint8_t rx_buffer[128];
```

`n` 通常应为 2 的幂，并满足目标平台和编译器的对齐要求。

### `SECTION(s)`

将函数或对象放入指定段中，常用于 bootloader、链接脚本分区、特殊 RAM 区域或只读表。

| 编译器 | 展开 |
|---|---|
| GCC / Clang | `__attribute__((section(s)))` |
| ARMCC | `__attribute__((section(s)))` |
| IAR | 空 |
| MSVC | `__declspec(allocate(s))` |
| 未识别编译器 | 空 |

```c
SECTION(".fast_code") void time_critical_task(void);
```

段名 `s` 必须与链接脚本或工程配置匹配。

## 分支预测宏

### `LIKELY(x)`

提示编译器表达式 `x` 大概率为真。

| 编译器 | 展开 |
|---|---|
| GCC / Clang | `__builtin_expect(!!(x), 1)` |
| ARMCC | `(x)` |
| IAR | `(x)` |
| MSVC | `(x)` |
| 未识别编译器 | `(x)` |

```c
if (LIKELY(status == 0)) {
    return;
}
```

该宏主要用于热路径优化，普通业务判断不必刻意使用。

### `UNLIKELY(x)`

提示编译器表达式 `x` 大概率为假。

| 编译器 | 展开 |
|---|---|
| GCC / Clang | `__builtin_expect(!!(x), 0)` |
| ARMCC | `(x)` |
| IAR | `(x)` |
| MSVC | `(x)` |
| 未识别编译器 | `(x)` |

```c
if (UNLIKELY(error != 0)) {
    handle_error(error);
}
```

常用于错误路径、边界条件和异常状态判断。

## 通用注意事项

> [!warning]
> 当前接口主要是函数式宏，不是 `static inline` 函数。传入带副作用的表达式可能被求值多次，例如 `min(i++, limit)` 会产生不直观的结果。优先传入普通变量或无副作用表达式。

- 带赋值效果的宏要求参数是可修改左值，例如 `SET_BIT`、`CLEAR_BIT`、`TOGGLE_BIT` 和 `REG_SET`。
- 位移宏 `BIT(n)` 要求 `n` 位于底层无符号整数类型的有效位宽内。
- `linear_map` 内部存在除法，必须保证 `x1 != x2`。
- 属性宏的具体行为仍取决于编译器、目标平台和链接脚本配置。
