#include "strings.h"

#include <stdarg.h>
#include <stdio.h>

struct String Blank = { .len = 0, .text = NULL };

static char     buf[4096];
static unsigned remaining = sizeof(buf);

struct String Stringf(const char *fmt, ...)
{
    struct String string = {0};

    va_list args;
    va_start(args, fmt);
    unsigned offset = sizeof(buf) - remaining;
    int written = vsnprintf(buf + offset, remaining, fmt, args);
    if (written > 0) {
        string.text = &buf[offset];
        string.len = written;
        remaining -= written;
    }
    va_end(args);

    return string;
}
