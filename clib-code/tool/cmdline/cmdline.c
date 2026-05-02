#include "cmdline.h"

#include <stddef.h>

static const char _cmd_line_none[] = "NONE";

static inline int _cmd_line_is_blank(char c)
{
    return c == ' ' || (unsigned int)((unsigned char)c - (unsigned char)'\t') <= 4U;
}

static char *_cmd_line_match_name(char *line, const char *name, char **name_start)
{
    char *p = line;

    while (_cmd_line_is_blank(*p))
    {
        ++p;
    }

    *name_start = p;
    while (*name != '\0' && *p == *name)
    {
        ++p;
        ++name;
    }

    if (*name != '\0')
    {
        return NULL;
    }

    if (*p != '\0' && !_cmd_line_is_blank(*p))
    {
        return NULL;
    }

    return p;
}

static int _cmd_line_parse(char *read, char *write, char **argv, int max_argc)
{
    int argc = 0;
    int end = 0;

    while (argc < max_argc && !end)
    {
        char c;

        while (_cmd_line_is_blank(*read))
        {
            ++read;
        }

        if (*read == '\0')
        {
            break;
        }

        argv[argc++] = write;

        while ((c = *read++) != '\0' && !_cmd_line_is_blank(c))
        {
            if (c == '\'' || c == '"')
            {
                char quote = c;

                while ((c = *read++) != '\0' && c != quote)
                {
                    *write++ = c;
                }

                if (c == '\0')
                {
                    end = 1;
                    break;
                }
            }
            else
            {
                *write++ = c;
            }
        }

        if (c == '\0')
        {
            end = 1;
        }

        *write++ = '\0';
    }

    argv[argc] = NULL;
    return argc;
}

void cmd_line_init(CmdLine *self, cmd_line_fn fn, const char *name, const char *desc)
{
    if (self == NULL)
    {
        return;
    }

    self->fn = fn;
    self->name = name;
    self->desc = desc;
}

int cmd_line_exe(CmdLine *self, char *line)
{
    int argc = 0;
    char *argv[CMD_LINE_MAX_ARGC + 1];

    if (self == NULL || self->fn == NULL || line == NULL)
    {
        return 0;
    }

    if (self->name != NULL && *self->name != '\0')
    {
        char *name_start;
        char *name_end = _cmd_line_match_name(line, self->name, &name_start);

        if (name_end == NULL)
        {
            return 0;
        }

        argv[argc++] = name_start;
        if (*name_end != '\0')
        {
            *name_end++ = '\0';
            argc += _cmd_line_parse(name_end, name_end, argv + argc, CMD_LINE_MAX_ARGC - argc);
        }
        else
        {
            argv[argc] = NULL;
        }
    }
    else
    {
        argc = _cmd_line_parse(line, line, argv, CMD_LINE_MAX_ARGC);
    }

    if (argc <= 0)
    {
        return 0;
    }

    return self->fn(argc, argv);
}

const char *cmd_line_name(const CmdLine *self)
{
    if (self != NULL && self->name != NULL)
    {
        return self->name;
    }

    return _cmd_line_none;
}

const char *cmd_line_desc(const CmdLine *self)
{
    if (self != NULL && self->desc != NULL)
    {
        return self->desc;
    }

    return _cmd_line_none;
}

void cmd_line_deinit(CmdLine *self)
{
    if (self == NULL)
    {
        return;
    }

    self->fn = NULL;
    self->name = NULL;
    self->desc = NULL;
}
