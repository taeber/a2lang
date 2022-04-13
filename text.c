#include "text.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static char buf[256];

static inline char hiascii(char ch) { return ch | 0x80; }

char *absoluteX(const char *value)
{
    return stringf("%s,X", value);
}

char *asciich(char ch) { return hex2(hiascii(ch)); }

char *hex2(uint8_t x) { return hex4(x); }
char *hex4(uint16_t x) { return stringf("$%.2X", x); }

char *hi(const char *value) { return stringf(">%s", value); }

char *immediate(char *value)
{
    char *imm = stringf("#%s", value);
    free(value);
    return imm;
}

char *indirectY(char *value)
{
    char *idy = stringf("(%s),Y", value);
    free(value);
    return idy;
}

char *lo(const char *value) { return stringf("<%s", value); }

char *numerical(const struct Numerical *val)
{
    if (val) {
        switch (val->type) {
        case NUM_IDENT:
            return string(&val->Identifier.String);
        case NUM_NUMBER:
            return hex4((uint16_t)val->Number);
        case NUM_NONE:
            break;
        }
    }
    return NULL;
}

char *offset(const char *lbl, int16_t off)
{
    if (off == 0) {
        return strcopy(lbl);
    }
    return stringf("%s+%d", lbl, off);
}

char *strcopy(const char *txt)
{
    size_t len  = strlen(txt);
    char * copy = malloc(len + 1);
    return strcpy(copy, txt);
}

char *string(const struct String *string) { return stringf("%.*s", string->len, string->text); }

char *stringf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof buf, fmt, args);
    va_end(args);
    return strcopy(buf);
}
