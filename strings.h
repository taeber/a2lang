#pragma once

#include <stdlib.h>

struct String {
    unsigned    len;
    const char *text;
};

extern struct String Blank;

struct String Stringf(const char *fmt, ...);
