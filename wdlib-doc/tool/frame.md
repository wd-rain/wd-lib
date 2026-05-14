---
aliases:
  - frame
  - frame.h
  - 自定义帧协议
depends:
  - "[[until]]"
tags:
  - c
  - clib
  - tool
  - frame
  - protocol
---

# frame

`frame` 提供一个轻量级自定义帧协议编码和解码器，适合串口、socket、pipe 等按字节到达的数据流。解码器内部使用状态机，调用者可以每收到 1 个字节就调用一次 `frame_decoder_decode_byte`。当完整帧通过 LEN 反码、累加校验和尾帧检查后，库会先更新最近帧视图，再自动触发用户回调。

帧格式如下：

```text
HEAD(1) + LEN(1) + LEN_INV(1) + DATA(0-255) + CHECKSUM(1) + TAIL(1)
```

默认 `HEAD` 为 `0xAAU`，`TAIL` 为 `0x55U`。`LEN_INV` 等于 `~LEN` 的低 8 位，用于尽早发现长度字节错误。`CHECKSUM` 为 `LEN + DATA` 的 8 位累加回绕值。

## 依赖关系

代码路径：

- `wdlib-code/tool/frame/frame.h`
- `wdlib-code/tool/frame/frame.c`

`frame` 依赖 `wdlib-code/until/until.h`，使用其中的 `WD_ASSERT`。实现文件额外使用标准头文件 `<string.h>` 完成 payload 拷贝。

## 集成与编译

`frame` 不需要安装步骤，也不分配堆内存。将以下文件加入工程即可：

- 头文件搜索路径包含 `wdlib-code`
- 编译源文件加入 `wdlib-code/tool/frame/frame.c`
- 使用处包含 `#include "tool/frame/frame.h"`

单独语法检查示例：

```powershell
gcc -std=c99 -Wall -Wextra -pedantic -I wdlib-code -fsyntax-only wdlib-code\tool\frame\frame.c
```

独立测试示例：

```powershell
powershell -ExecutionPolicy Bypass -File .\clib-tests\frame\build.ps1
.\clib-tests\frame\build\frame_test.exe
```

## 实现约束

`frame` 的实现遵循 `AGENTS.md` 中的 wdlib-code 模块规范：

- 所有公开接口都会在入口处校验传入参数。
- 断言只出现在公开接口入口处。
- `_frame_*_checked` 内部函数只处理已校验状态下的状态清理，不做额外参数防御。
- `FrameDecoder` 不拥有 payload 缓存，缓存由调用者提供并保证生命周期。
- `callback == NULL` 合法，表示只通过 `frame_decoder_view` 获取最近完整帧。
- 错误恢复采用重置到等待帧头；触发错误的当前字节不会重新作为新帧头处理。

## 接口总览

| 类别 | 接口 | 基本功能 | 使用要点 |
|---|---|---|---|
| 配置 | `FRAME_HEAD` | 配置帧头字节 | 默认 `0xAAU`，必须不大于 `0xFFU` |
| 配置 | `FRAME_TAIL` | 配置帧尾字节 | 默认 `0x55U`，必须不大于 `0xFFU` |
| 类型 | `FrameStatus` | 表示编码或解码结果 | 错误状态会使解码器回到等待帧头 |
| 类型 | `FrameView` | 最近完整帧的 payload 视图 | 由解码器维护，下一次解码输入前有效 |
| 类型 | `FrameDecoder` | 状态机解码器对象 | 保存外部缓存、当前状态、最近帧视图和回调 |
| 类型 | `frame_decoder_fn` | 完整帧回调函数 | 参数为 payload 指针和长度 |
| 编码 | `frame_encoded_size(size)` | 计算编码后帧总长度 | 返回 `size + 5U` |
| 编码 | `frame_encode(data, size, frame, frame_capacity, frame_size)` | 生成完整帧 | payload 最大 `255U` 字节 |
| 生命周期 | `frame_decoder_init(self, buffer, capacity, callback)` | 初始化解码器 | `buffer` 和 `capacity` 决定最大接收 payload |
| 解码 | `frame_decoder_decode_byte(self, byte)` | 输入单字节并推进状态机 | 完整帧时返回 `WD_FRAME_STATUS_COMPLETE` |
| 访问 | `frame_decoder_view(self)` | 获取最近完整帧视图 | 返回 `self->view` 地址 |
| 状态 | `frame_decoder_reset(self)` | 重置状态机并清空视图 | 不清零调用者提供的底层缓存 |
| 生命周期 | `frame_decoder_deinit(self)` | 清空解码器对象 | 不释放底层缓存 |

## 配置宏

### `FRAME_HEAD`

配置帧头字节。

```c
#ifndef FRAME_HEAD
#define FRAME_HEAD 0xAAU
#endif
```

如果需要修改帧头，可以在包含 `frame.h` 前定义：

```c
#define FRAME_HEAD 0xA5U
#include "tool/frame/frame.h"
```

`FRAME_HEAD` 必须不大于 `0xFFU`。

### `FRAME_TAIL`

配置帧尾字节。

```c
#ifndef FRAME_TAIL
#define FRAME_TAIL 0x55U
#endif
```

如果需要修改帧尾，可以在包含 `frame.h` 前定义：

```c
#define FRAME_TAIL 0x5AU
#include "tool/frame/frame.h"
```

`FRAME_TAIL` 必须不大于 `0xFFU`。

## 类型定义

### `FrameStatus`

编码和解码状态。

```c
typedef enum frame_status_t
{
    WD_FRAME_STATUS_OK = 0,
    WD_FRAME_STATUS_PENDING,
    WD_FRAME_STATUS_COMPLETE,
    WD_FRAME_STATUS_OVERFLOW,
    WD_FRAME_STATUS_LEN_ERROR,
    WD_FRAME_STATUS_CHECKSUM_ERROR,
    WD_FRAME_STATUS_TAIL_ERROR
} FrameStatus;
```

| 状态 | 含义 |
|---|---|
| `WD_FRAME_STATUS_OK` | 编码成功 |
| `WD_FRAME_STATUS_PENDING` | 解码尚未形成完整帧 |
| `WD_FRAME_STATUS_COMPLETE` | 解码得到完整合法帧 |
| `WD_FRAME_STATUS_OVERFLOW` | 编码输出空间不足、payload 超过 255 字节，或解码缓存不足 |
| `WD_FRAME_STATUS_LEN_ERROR` | `LEN_INV` 与 `LEN` 不匹配 |
| `WD_FRAME_STATUS_CHECKSUM_ERROR` | 累加校验失败 |
| `WD_FRAME_STATUS_TAIL_ERROR` | 尾帧字节不匹配 |

### `frame_decoder_fn`

完整帧回调函数类型。

```c
typedef void (*frame_decoder_fn)(const uint8_t *data, size_t size);
```

回调只接收 payload 数据指针和长度，不接收 `FrameDecoder` 指针。回调触发时，`FrameDecoder.view` 已经更新为同一段 payload。

### `FrameView`

最近完整帧的 payload 视图。

```c
typedef struct frame_view_t
{
    const uint8_t *data;
    size_t size;
} FrameView;
```

| 成员 | 说明 |
|---|---|
| `data` | 指向解码器接收缓存中的 payload 起始地址 |
| `size` | payload 字节数 |

`FrameView` 不复制 payload。下一次调用 `frame_decoder_decode_byte`、`frame_decoder_reset` 或 `frame_decoder_deinit` 后，视图会被清空或失效。

### `FrameDecoder`

状态机解码器对象。

```c
typedef struct frame_decoder_t
{
    FrameView view;
    uint8_t *buffer;
    size_t capacity;
    size_t length;
    size_t index;
    uint8_t checksum;
    uint8_t state;
    frame_decoder_fn callback;
} FrameDecoder;
```

| 成员 | 说明 |
|---|---|
| `view` | 最近完整帧的 payload 视图 |
| `buffer` | 调用者提供的 payload 接收缓存 |
| `capacity` | payload 接收缓存容量 |
| `length` | 当前帧 LEN 字段 |
| `index` | 当前已接收 payload 字节数 |
| `checksum` | 当前帧累加校验值 |
| `state` | 当前状态机状态 |
| `callback` | 完整帧回调函数 |

结构体字段公开是为了便于静态分配和调试观察，调用者不应直接修改内部状态字段。常规访问最近帧时使用 `frame_decoder_view`。

## 编码接口

### `frame_encoded_size(size)`

计算 payload 编码成完整帧后的字节数。

```c
size_t frame_encoded_size(size_t size);
```

返回值固定为：

```text
size + 5U
```

五个固定字节分别是 `HEAD`、`LEN`、`LEN_INV`、`CHECKSUM` 和 `TAIL`。

### `frame_encode(data, size, frame, frame_capacity, frame_size)`

将 payload 编码为完整帧。

```c
FrameStatus frame_encode(const uint8_t *data, size_t size, uint8_t *frame, size_t frame_capacity, size_t *frame_size);
```

成功时返回 `WD_FRAME_STATUS_OK`，并通过 `frame_size` 写出实际帧长。失败时返回 `WD_FRAME_STATUS_OVERFLOW`，并将 `*frame_size` 置为 `0U`。

参数要求：

- `frame` 必须非空。
- `frame_size` 必须非空。
- `size <= 255U`。
- `frame_capacity >= frame_encoded_size(size)`。
- `data == NULL` 仅在 `size == 0U` 时合法。

编码示例：

```c
uint8_t data[] = {1U, 2U, 3U};
uint8_t frame[8U];
size_t frame_size;
FrameStatus status;

status = frame_encode(data, 3U, frame, sizeof(frame), &frame_size);
```

编码结果：

```text
AA 03 FC 01 02 03 09 55
```

其中 `0xFC == ~0x03`，`0x09 == 0x03 + 0x01 + 0x02 + 0x03`。

## 解码接口

### `frame_decoder_init(self, buffer, capacity, callback)`

初始化解码器。

```c
void frame_decoder_init(FrameDecoder *self, uint8_t *buffer, size_t capacity, frame_decoder_fn callback);
```

`self`、`buffer` 必须非空，`capacity` 必须大于 `0U`。`callback` 可以为 `NULL`。

`frame_decoder_init` 只保存调用者提供的 payload 缓存，不分配内存，也不会清零底层缓存内容。`capacity` 决定该解码器可接收的最大 payload 长度；如果后续收到的 `LEN` 大于 `capacity`，解码器会返回 `WD_FRAME_STATUS_OVERFLOW`。

### `frame_decoder_decode_byte(self, byte)`

输入一个字节并推进状态机。

```c
FrameStatus frame_decoder_decode_byte(FrameDecoder *self, uint8_t byte);
```

状态机顺序为：

```text
WAIT_HEAD -> WAIT_LEN -> WAIT_LEN_INV -> DATA -> CHECKSUM -> TAIL
```

常见返回值：

- 未收齐一帧时返回 `WD_FRAME_STATUS_PENDING`。
- 完整帧合法时返回 `WD_FRAME_STATUS_COMPLETE`，并触发回调。
- `LEN_INV` 不匹配时返回 `WD_FRAME_STATUS_LEN_ERROR`。
- checksum 不匹配时返回 `WD_FRAME_STATUS_CHECKSUM_ERROR`。
- tail 不匹配时返回 `WD_FRAME_STATUS_TAIL_ERROR`。
- `LEN` 超过接收缓存容量时返回 `WD_FRAME_STATUS_OVERFLOW`。

完整帧到达时，解码器会按以下顺序处理：

1. 更新 `self->view.data` 和 `self->view.size`。
2. 如果 `callback != NULL`，调用 `callback(data, size)`。
3. 将内部状态切回等待下一帧头。
4. 返回 `WD_FRAME_STATUS_COMPLETE`。

### `frame_decoder_view(self)`

获取最近完整帧视图。

```c
const FrameView *frame_decoder_view(const FrameDecoder *self);
```

如果上一帧已完整解析，返回的视图指向解码器内部 `view`。如果尚未解析出完整帧，或最近一次解码输入已经开始处理下一帧，视图内容为空：

```c
view->data == NULL
view->size == 0U
```

### `frame_decoder_reset(self)`

重置解码器状态。

```c
void frame_decoder_reset(FrameDecoder *self);
```

该接口会清空当前状态机进度和最近帧视图，但不会清零调用者提供的 payload 缓存，也不会修改回调函数。

### `frame_decoder_deinit(self)`

清空解码器对象。

```c
void frame_decoder_deinit(FrameDecoder *self);
```

`frame_decoder_deinit` 会清空状态、缓存引用、容量和回调函数。它不会释放 payload 缓存，因为该缓存不由 `frame` 模块分配。

## 零长度 payload

零长度 payload 是合法帧：

```text
AA 00 FF 00 55
```

此时 `LEN == 0U`，`LEN_INV == 0xFFU`，checksum 只等于 `LEN`，也就是 `0U`。回调会被正常触发，参数为：

```text
data = buffer
size = 0U
```

## 错误恢复

解码器遇到以下错误时会返回对应状态，并重置为等待帧头：

- `LEN > capacity`
- `LEN_INV != ~LEN`
- checksum 不匹配
- tail 不匹配

触发错误的当前字节不会重新参与下一轮帧头判断。例如 tail 错误字节刚好等于 `FRAME_HEAD` 时，它也只作为错误 tail 处理，不会立即作为下一帧帧头。

## 完整示例

```c
#include "tool/frame/frame.h"

#include <stdint.h>

static uint8_t rx_payload[255U];
static FrameDecoder decoder;

static void on_frame(const uint8_t *data, size_t size)
{
    (void)data;
    (void)size;
}

void protocol_init(void)
{
    frame_decoder_init(&decoder, rx_payload, sizeof(rx_payload), on_frame);
}

void uart_rx_byte(uint8_t byte)
{
    FrameStatus status;

    status = frame_decoder_decode_byte(&decoder, byte);
    if (status == WD_FRAME_STATUS_COMPLETE)
    {
        const FrameView *view;

        view = frame_decoder_view(&decoder);
        (void)view;
    }
}

void protocol_send(const uint8_t *data, size_t size)
{
    uint8_t tx_frame[260U];
    size_t tx_size;

    if (frame_encode(data, size, tx_frame, sizeof(tx_frame), &tx_size) == WD_FRAME_STATUS_OK)
    {
        /* 将 tx_frame[0..tx_size) 交给底层发送接口。 */
    }
}
```

## 检查

独立测试路径：

- `clib-tests/frame/frame_test.c`
- `clib-tests/frame/build.ps1`

运行：

```powershell
powershell -ExecutionPolicy Bypass -File .\clib-tests\frame\build.ps1
.\clib-tests\frame\build\frame_test.exe
```

测试覆盖普通 payload、零长度 payload、单字节流式解码、噪声跳过、LEN 反码错误、checksum 错误、tail 错误、接收缓存不足、编码 payload 过长和编码输出空间不足。
