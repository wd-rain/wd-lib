---
aliases:
  - cmdline
  - cmdline.h
  - C 命令行解析
tags:
  - c
  - clib
  - cmdline
  - parser
---

# cmdline

`cmdline` 提供一个轻量级 C 命令行解析器，用于将一行可写字符串解析成 `argc / argv` 形式，并在命令名匹配时调用用户注册的回调函数。当前实现不进行堆内存分配，解析过程会原地修改传入的 `line` 缓冲区。

本文只说明 `cmdline` 本身提供的接口，所有接口都使用独立标题，方便在 Obsidian 大纲中快速定位。

## 接口总览

| 类别  | 接口                                    | 基本功能        | 使用要点                         |
| --- | ------------------------------------- | ----------- | ---------------------------- |
| 配置  | `CMD_LINE_MAX_ARGC`                   | 配置最多解析的参数数量 | 默认值为 `4`，必须大于 `0`            |
| 类型  | `cmd_line_fn`                         | 命令回调函数类型    | 形参为 `argc` 和 `argv`，返回 `int` |
| 类型  | `CmdLine`                             | 命令对象        | 保存回调、命令名和描述文本                |
| 初始化 | `cmd_line_init(self, fn, name, desc)` | 初始化命令对象     | 不复制字符串，只保存指针                 |
| 执行  | `cmd_line_exe(self, line)`            | 匹配并执行命令     | 返回回调函数返回值，`line` 必须可写        |
| 访问  | `cmd_line_name(self)`                 | 获取命令名       | 未设置时返回 `"NONE"`              |
| 访问  | `cmd_line_desc(self)`                 | 获取命令描述      | 未设置时返回 `"NONE"`              |
| 释放  | `cmd_line_deinit(self)`               | 清空命令对象      | 将结构体成员置空                     |

## 配置宏

### `CMD_LINE_MAX_ARGC`

配置一次命令最多解析出的参数数量。

```c
#ifndef CMD_LINE_MAX_ARGC
#define CMD_LINE_MAX_ARGC 4
#endif
```

如果需要调整参数上限，可以在包含 `cmdline.h` 前定义该宏：

```c
#define CMD_LINE_MAX_ARGC 8
#include "cmdline.h"
```

当前实现会在栈上创建：

```c
char* argv[CMD_LINE_MAX_ARGC + 1];
```

额外的一个元素用于保存结尾的 `NULL`。`CMD_LINE_MAX_ARGC` 必须大于 `0`。

## 类型定义

### `cmd_line_fn`

命令回调函数类型。

```c
typedef int (* cmd_line_fn)(int argc, char** argv);
```

回调函数在命令匹配成功并完成参数解析后被调用。

```c
static int set_cmd(int argc, char** argv)
{
    int i;

    for (i = 0; i < argc; ++i)
    {
        printf("argv[%d]=%s\n", i, argv[i]);
    }

    return 0;
}
```

注意：`cmd_line_exe` 会把回调函数返回值返回给调用者。

### `CmdLine`

命令对象类型。

```c
typedef struct cmdline_t
{
    cmd_line_fn fn;
    const char* name;
    const char* desc;
}CmdLine;
```

| 成员 | 说明 |
|---|---|
| `fn` | 命令匹配成功后调用的回调函数 |
| `name` | 命令名，例如 `"set"` |
| `desc` | 命令描述文本，当前实现只保存，不参与执行 |

`name` 和 `desc` 不会被复制，调用者需要保证它们在命令对象使用期间有效。
字段类型为 `const char*`，适合直接保存字符串字面量或只读描述文本。

## 初始化接口

### `cmd_line_init(self, fn, name, desc)`

初始化一个 `CmdLine` 对象。

```c
void cmd_line_init(CmdLine* self, cmd_line_fn fn, const char* name, const char* desc);
```

示例：

```c
CmdLine cmd;

cmd_line_init(&cmd, set_cmd, "set", "set parameter");
```

该函数只进行指针赋值：

- `self->fn = fn`
- `self->name = name`
- `self->desc = desc`

如果 `self == NULL`，函数直接返回。

## 执行接口

### `cmd_line_exe(self, line)`

解析并执行一行命令。

```c
int cmd_line_exe(CmdLine* self, char* line);
```

示例：

```c
CmdLine cmd;
char line[] = "set speed 100";
int ret;

cmd_line_init(&cmd, set_cmd, "set", "set parameter");
ret = cmd_line_exe(&cmd, line);
```

回调收到的参数为：

```text
argc = 3
argv[0] = "set"
argv[1] = "speed"
argv[2] = "100"
argv[3] = NULL
```

返回值规则：

- 命令匹配成功并调用回调时，返回回调函数的返回值。
- `self == NULL`、`self->fn == NULL`、`line == NULL` 时返回 `0`。
- 命令名不匹配时返回 `0`。
- 解析后没有任何参数时返回 `0`。

注意：`line` 必须是可写字符数组，不能直接传入字符串字面量。

错误示例：

```c
cmd_line_exe(&cmd, "set speed 100");  // 错误：字符串字面量不可写
```

正确示例：

```c
char line[] = "set speed 100";
cmd_line_exe(&cmd, line);
```

## 访问接口

### `cmd_line_name(self)`

获取命令对象中的命令名。

```c
const char* cmd_line_name(const CmdLine* self);
```

示例：

```c
printf("name=%s\n", cmd_line_name(&cmd));
```

返回值规则：

- `self != NULL` 且 `self->name != NULL` 时，返回 `self->name`。
- `self == NULL` 或 `self->name == NULL` 时，返回 `"NONE"`。

返回值类型为 `const char*`，调用者不应修改返回的字符串。

### `cmd_line_desc(self)`

获取命令对象中的描述文本。

```c
const char* cmd_line_desc(const CmdLine* self);
```

示例：

```c
printf("desc=%s\n", cmd_line_desc(&cmd));
```

返回值规则：

- `self != NULL` 且 `self->desc != NULL` 时，返回 `self->desc`。
- `self == NULL` 或 `self->desc == NULL` 时，返回 `"NONE"`。

返回值类型为 `const char*`，调用者不应修改返回的字符串。

## 释放接口

### `cmd_line_deinit(self)`

清空命令对象。

```c
void cmd_line_deinit(CmdLine* self);
```

示例：

```c
cmd_line_deinit(&cmd);
```

该函数会将 `fn`、`name` 和 `desc` 都置为 `NULL`。如果 `self == NULL`，函数直接返回。

## 解析规则

### 空白分隔

空格、`\t`、`\n`、`\v`、`\f`、`\r` 都会被视为空白字符。

```c
char line[] = "set speed 100";
```

解析结果：

```text
argv[0] = "set"
argv[1] = "speed"
argv[2] = "100"
```

### 引号参数

单引号和双引号可以包住包含空白的参数，引号本身不会写入 `argv`。

```c
char line[] = "set name \"hello world\"";
```

解析结果：

```text
argv[0] = "set"
argv[1] = "name"
argv[2] = "hello world"
```

单引号同样支持：

```c
char line[] = "set name 'hello world'";
```

### 不支持反斜杠转义

当前实现不支持 `\` 转义，反斜杠会作为普通字符保留。

```c
char line[] = "set path C:\\tmp\\file.txt";
```

解析结果：

```text
argv[0] = "set"
argv[1] = "path"
argv[2] = "C:\tmp\file.txt"
```

### 命令名匹配

如果 `CmdLine.name` 非空，`cmd_line_exe` 会先匹配命令名。

```c
cmd_line_init(&cmd, set_cmd, "set", "set parameter");
```

这行会匹配：

```text
set speed 100
```

这行不会匹配：

```text
get speed 100
```

命令名前缀不会误匹配。例如命令名是 `"set"` 时，`"setup speed 100"` 不会匹配。

如果 `name == NULL` 或 `name` 为空字符串，`cmd_line_exe` 会跳过命令名匹配，直接解析整行并调用回调。

## 完整示例

```c
#include "clib-code/cmdline/cmdline.h"

#include <stdio.h>

static int set_cmd(int argc, char** argv)
{
    int i;

    printf("argc=%d\n", argc);
    for (i = 0; i < argc; ++i)
    {
        printf("argv[%d]=%s\n", i, argv[i]);
    }

    return 0;
}

int main(void)
{
    CmdLine cmd;
    char line[] = "set name \"hello world\"";
    int ret;

    cmd_line_init(&cmd, set_cmd, "set", "set parameter");
    printf("name=%s\n", cmd_line_name(&cmd));
    printf("desc=%s\n", cmd_line_desc(&cmd));
    ret = cmd_line_exe(&cmd, line);
    cmd_line_deinit(&cmd);

    return ret;
}
```

编译：

```powershell
gcc -std=c99 -Wall -Wextra -g -O0 example.c clib-code\cmdline\cmdline.c -o example.exe
```

输出：

```text
name=set
desc=set parameter
argc=3
argv[0]=set
argv[1]=name
argv[2]=hello world
```

## 检查

当前仓库未保留独立测试文件。可以先执行语法检查确认接口声明和实现一致：

```powershell
gcc -std=c99 -Wall -Wextra -pedantic -fsyntax-only clib-code\cmdline\cmdline.c
```

如需手动验证返回值，可以编写一个回调返回固定值的示例：

```c
static int set_cmd(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    return 123;
}

int main(void)
{
    CmdLine cmd;
    char line[] = "set speed 100";
    int ret;

    cmd_line_init(&cmd, set_cmd, "set", "set parameter");
    ret = cmd_line_exe(&cmd, line);
    cmd_line_deinit(&cmd);

    return ret == 123 ? 0 : 1;
}
```

## 通用注意事项

> [!warning]
> `cmd_line_exe` 会原地修改传入的 `line`。调用者必须传入可写字符数组，不能传入字符串字面量或只读存储区中的字符串。

- `cmdline` 不分配堆内存，`argv` 指向 `line` 内部的不同位置。
- 回调函数执行期间可以读取 `argv`，但不应在 `line` 生命周期结束后继续保存这些指针。
- `cmd_line_exe` 返回 `0` 同时可能表示未执行、参数为空，也可能是回调本身返回了 `0`。如果需要区分这些情况，回调返回值需要自行约定。
- `cmd_line_name` 和 `cmd_line_desc` 返回只读字符串指针，未设置对应字段时返回 `"NONE"`。
- 超过 `CMD_LINE_MAX_ARGC` 的参数会被忽略。
- 未闭合的引号会把已读取的内容作为当前参数的一部分，不返回错误码。
- `desc` 当前只作为描述字段保存，解析和执行流程不会使用它。
