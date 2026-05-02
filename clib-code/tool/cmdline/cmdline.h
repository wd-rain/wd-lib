#ifndef _CMDLINE_H_
#define _CMDLINE_H_

// 依赖

// 配置
#ifndef CMD_LINE_MAX_ARGC
#define CMD_LINE_MAX_ARGC 4U
#endif

#if CMD_LINE_MAX_ARGC < 1U
#error "CMD_LINE_MAX_ARGC must be greater than 0"
#endif

// 类型定义
typedef int (*cmd_line_fn)(int argc, char **argv);

typedef struct cmdline_t
{
    cmd_line_fn fn;
    const char *name;
    const char *desc;
} CmdLine;

// 接口
void cmd_line_init(CmdLine *self, cmd_line_fn fn, const char *name, const char *desc);
int cmd_line_exe(CmdLine *self, char *line);
const char *cmd_line_name(const CmdLine *self);
const char *cmd_line_desc(const CmdLine *self);
void cmd_line_deinit(CmdLine *self);
#endif
