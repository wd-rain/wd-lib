---
aliases:
  - rb
  - rb.h
  - byte ring buffer
  - 字节环形缓冲
depends:
  - "[[until]]"
tags:
  - c
  - clib
  - tool
  - rb
  - buffer
---

# rb

`rb` 提供一个轻量级字节环形缓冲模块，用于串口、socket、pipe 或协议解析前后的连续字节流缓存。它不分配堆内存，底层存储由调用者提供；模块内部维护 `size` 字段区分空和满，因此初始化传入的 `capacity` 全部可用，不浪费一个槽位。

`rb` 只处理字节流，不保存消息边界。如果上层需要协议帧、命令或事件边界，应在 `rb` 之外通过协议解析器或消息队列维护。

## 依赖关系

代码路径：

- `clib-code/tool/rb/rb.h`
- `clib-code/tool/rb/rb.c`

`rb` 依赖 `clib-code/until/until.h`，使用其中的 `WD_ASSERT` 和 `WD_MIN`。实现文件额外使用标准头文件 `<string.h>` 完成连续内存拷贝。

## 实现约束

`rb` 的实现遵循 `AGENTS.md` 中的 clib-code 模块规范：

- 所有公开接口都会在入口处校验传入参数或对象状态。
- 断言只出现在公开接口入口和 `_rb_assert_*` 辅助函数中。
- `_rb_*_checked` 内部函数只执行已校验状态下的读写、拷贝或索引推进，不做额外参数防御。
- `Rb` 的公开字段虽然可见，但调用者不应直接修改；若字段被外部破坏，后续公开接口会通过 `_rb_assert_ready` 暴露不变式错误。

## 接口总览

| 类别 | 接口 | 基本功能 | 使用要点 |
|---|---|---|---|
| 类型 | `Rb` | 字节环形缓冲对象 | 保存外部存储指针、容量、读写位置和当前大小 |
| 生命周期 | `rb_init(self, buffer, capacity)` | 初始化缓冲对象 | 不清零底层数组，容量必须大于 `0U` |
| 生命周期 | `rb_deinit(self)` | 清空对象引用 | 不释放底层数组 |
| 写入 | `rb_write(self, data, size)` | 写入字节流 | 返回实际写入字节数，空间不足时部分写入 |
| 读取 | `rb_read(self, data, size)` | 读取并消费字节流 | 返回实际读取字节数，数据不足时部分读取 |
| 读取 | `rb_peek(self, data, size)` | 查看但不消费字节流 | 返回实际查看字节数，不改变读写位置和大小 |
| 读取 | `rb_drop(self, size)` | 丢弃已缓存字节 | 等价于无拷贝读取 |
| 状态 | `rb_clear(self)` | 清空逻辑内容 | 不擦除底层数组内容 |
| 状态 | `rb_size(self)` | 查询可读字节数 | 返回当前已缓存字节数 |
| 状态 | `rb_free_size(self)` | 查询可写字节数 | 与 `rb_size` 之和等于 `rb_capacity` |
| 状态 | `rb_capacity(self)` | 查询总容量 | 返回初始化传入的真实可用容量 |
| 状态 | `rb_is_empty(self)` | 判断是否为空 | 空返回 `1`，否则返回 `0` |
| 状态 | `rb_is_full(self)` | 判断是否已满 | 满返回 `1`，否则返回 `0` |

## 类型定义

### `Rb`

字节环形缓冲对象。

```c
typedef struct rb_t
{
    uint8_t *buffer;
    size_t capacity;
    size_t read_index;
    size_t write_index;
    size_t size;
} Rb;
```

| 成员 | 说明 |
|---|---|
| `buffer` | 调用者提供的底层字节数组 |
| `capacity` | 底层数组容量，也是 `rb` 的真实可用容量 |
| `read_index` | 下一次读取的起始位置 |
| `write_index` | 下一次写入的起始位置 |
| `size` | 当前已缓存的可读字节数 |

`Rb` 不分配堆内存，调用者负责提供对象本身和底层 `buffer` 的存储。结构体字段公开是为了便于静态分配和嵌入其他对象，但字段由 `rb` 模块管理，用户不应直接修改。

## 生命周期接口

### `rb_init(self, buffer, capacity)`

初始化一个 `Rb` 对象。

```c
void rb_init(Rb *self, uint8_t *buffer, size_t capacity);
```

示例：

```c
uint8_t storage[16U];
Rb rb;

rb_init(&rb, storage, 16U);
```

该函数只保存 `buffer` 指针并初始化索引和 `size`，不会清零底层数组。`self`、`buffer` 必须非空，`capacity` 必须大于 `0U`，否则触发 `WD_ASSERT`。

### `rb_deinit(self)`

清空一个 `Rb` 对象的引用和状态。

```c
void rb_deinit(Rb *self);
```

`rb_deinit` 会将 `buffer` 置为 `NULL`，将 `capacity`、`read_index`、`write_index` 和 `size` 置为 `0U`。它不会释放底层数组，因为底层数组不由 `rb` 分配。

调用 `rb_deinit` 前，`self` 必须是已经通过 `rb_init` 初始化且尚未释放的有效对象。调用 `rb_deinit` 后，该对象不应继续传给其他 `rb_*` 接口，除非重新执行 `rb_init`。

## 写入接口

### `rb_write(self, data, size)`

向缓冲中写入字节。

```c
size_t rb_write(Rb *self, const uint8_t *data, size_t size);
```

返回实际写入的字节数。若空闲空间不足，函数只写入当前能容纳的部分；若缓冲已满，返回 `0U`。

```c
uint8_t data[] = {1U, 2U, 3U, 4U};
size_t written;

written = rb_write(&rb, data, 4U);
```

`self` 必须已经完成初始化，`data` 必须非空。`size == 0U` 时不会写入数据，返回 `0U`。

## 读取接口

### `rb_read(self, data, size)`

从缓冲中读取并消费字节。

```c
size_t rb_read(Rb *self, uint8_t *data, size_t size);
```

返回实际读取的字节数。若可读数据不足，函数只读取已有数据；若缓冲为空，返回 `0U`。读取成功的字节会从缓冲逻辑内容中移除。

```c
uint8_t out[4U];
size_t read_size;

read_size = rb_read(&rb, out, 4U);
```

### `rb_peek(self, data, size)`

查看缓冲中的字节，但不消费。

```c
size_t rb_peek(const Rb *self, uint8_t *data, size_t size);
```

返回实际查看的字节数。`rb_peek` 不移动 `read_index`，不改变 `write_index` 和 `size`，适合协议解析前先检查帧头或长度字段。

### `rb_drop(self, size)`

丢弃缓冲中的字节。

```c
size_t rb_drop(Rb *self, size_t size);
```

返回实际丢弃的字节数。`rb_drop` 等价于无拷贝读取：它只推进读位置并减少 `size`，不会把数据复制到用户数组中。若请求丢弃的字节数超过当前可读字节数，只会丢弃已有数据。

## 状态接口

### `rb_clear(self)`

清空缓冲的逻辑内容。

```c
void rb_clear(Rb *self);
```

`rb_clear` 会将 `read_index`、`write_index` 和 `size` 置为 `0U`，但不会擦除底层数组中的旧字节。

### `rb_size(self)`

查询当前可读字节数。

```c
size_t rb_size(const Rb *self);
```

### `rb_free_size(self)`

查询当前可写字节数。

```c
size_t rb_free_size(const Rb *self);
```

对于有效的 `Rb` 对象，始终满足：

```text
rb_size(self) + rb_free_size(self) == rb_capacity(self)
```

### `rb_capacity(self)`

查询缓冲总容量。

```c
size_t rb_capacity(const Rb *self);
```

返回值等于 `rb_init` 传入的 `capacity`。由于 `rb` 使用 `size` 字段区分空和满，所以该容量全部可用于存储数据。

### `rb_is_empty(self)`

判断缓冲是否为空。

```c
int rb_is_empty(const Rb *self);
```

空返回 `1`，非空返回 `0`。

### `rb_is_full(self)`

判断缓冲是否已满。

```c
int rb_is_full(const Rb *self);
```

满返回 `1`，未满返回 `0`。

## 环形缓冲行为

- `rb_write`、`rb_read`、`rb_peek` 和 `rb_drop` 都返回实际处理的字节数。
- 写入空间不足时，`rb_write` 会部分写入，不会覆盖旧数据。
- 读取数据不足时，`rb_read` 和 `rb_peek` 会部分读取。
- `rb_peek` 不消费数据，适合只观察当前缓存内容。
- `rb_drop` 消费数据但不拷贝，适合丢弃已解析的帧头或无效字节。
- `rb_clear` 只清空逻辑状态，不擦除底层数组。
- 读写位置到达数组末尾后会回绕到数组起点，调用者不需要处理两段拷贝。
- `capacity` 是真实可用容量。例如容量为 `4U` 时，最多可以缓存 4 个字节。

## 线程模型

`rb` v1 不提供锁、临界区回调或线程归属检查。同一个 `Rb` 对象不应被多个线程或中断上下文并发访问。

如果在中断与主循环、多个线程或 DMA 回调中共享同一个 `Rb` 对象，调用层需要自行保护临界区，确保一次 `rb_*` 调用期间对象状态不会被其他上下文修改。

## 完整示例

```c
#include "clib-code/tool/rb/rb.h"

#include <stdint.h>

int main(void)
{
    uint8_t storage[16U];
    uint8_t input[] = {1U, 2U, 3U, 4U};
    uint8_t view[2U];
    uint8_t output[4U];
    Rb rb;
    size_t count;

    rb_init(&rb, storage, 16U);

    count = rb_write(&rb, input, 4U);
    (void)count;

    count = rb_peek(&rb, view, 2U);
    (void)count;

    count = rb_drop(&rb, 1U);
    (void)count;

    count = rb_read(&rb, output, 4U);
    (void)count;

    rb_clear(&rb);
    rb_deinit(&rb);

    return 0;
}
```

编译：

```powershell
gcc -std=c99 -Wall -Wextra -g -O0 example.c clib-code\tool\rb\rb.c -o example.exe
```

## 检查

可以先执行语法检查，确认头文件声明和实现一致：

```powershell
gcc -std=c99 -Wall -Wextra -pedantic -fsyntax-only clib-code\tool\rb\rb.c
```

建议手动或用临时测试程序覆盖以下行为：

- 初始化后 `rb_size` 为 `0U`，`rb_free_size` 等于 `rb_capacity`。
- 容量为 `4U` 时可以写入 4 个字节，随后 `rb_is_full` 返回 `1`。
- 满时继续写入返回 `0U`。
- 空间不足时 `rb_write` 返回剩余空间大小。
- 数据不足时 `rb_read`、`rb_peek` 和 `rb_drop` 返回已有数据大小。
- 回绕后读写顺序仍保持先进先出。
- `rb_peek` 后 `rb_size` 不变。
- `rb_drop` 后 `rb_size` 减少。
- `rb_clear` 后对象恢复为空，但底层数组内容不保证被清零。

`clib-demo` 中的 `app_init` 会运行一组 `rb` 调试自检。通过 Keil Debug 观察：

- `app_rb_test_result == APP_RB_TEST_RESULT_PASS` 表示测试通过。
- `app_rb_test_result == APP_RB_TEST_RESULT_FAIL` 表示测试失败，此时查看 `app_rb_test_failed_step` 定位失败步骤。
- `app_rb_test_size`、`app_rb_test_free_size`、`app_rb_test_capacity`、`app_rb_test_is_empty`、`app_rb_test_is_full` 保存最近一次状态快照。
- `app_rb_test_output` 保存最近一次读取或窥视到的数据，回绕测试通过时可看到 `{3U, 4U, 5U, 6U}` 的窥视结果。

## 通用注意事项

> [!warning]
> `rb` 使用 `WD_ASSERT` 暴露编程错误。空指针、未初始化对象和 `capacity == 0U` 都会触发断言；运行时容量不足不是错误，而是通过实际处理字节数返回给调用者。

- `rb` 不分配也不释放堆内存，调用者必须保证底层 `buffer` 在 `Rb` 使用期间持续有效。
- `rb` 是字节流缓存，不保存消息边界。
- `rb_write` 不覆盖旧数据，空间不足时只写入可容纳的部分。
- `rb_read` 会消费数据，`rb_peek` 不会消费数据。
- `rb_clear` 不擦除底层数组，不适合用作敏感数据清除。
- `rb` 无锁且非并发安全，共享访问时由调用层加临界区。
