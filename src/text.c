#include "text.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static char buf[256];

char *absoluteX(const char *value) { return stringf("%s,X", value); }
char *absoluteY(const char *value) { return stringf("%s,Y", value); }

char *asciich(char ch) { return stringf("\"%c\"", ch); }

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

char *phrase(const struct IdentPhrase *val)
{
    char *name   = string(&val->identifier.String),
         *index  = numerical(val->subscript),
         *member = val->field ? string(&val->field->String) : NULL;

    char *str = stringf(
        "%s%s%s%s%s",
        name,
        index ? "_" : "",
        index ? index : "",
        member ? "." : "",
        member ? member : "");

    free(name);
    if (index) {
        free(index);
    }
    if (member) {
        free(member);
    }

    return str;
}

char *qualify(const struct String *scope, const struct String *name)
{
    if (scope) {
        return stringf("%.*s.%.*s", scope->len, scope->text, name->len, name->text);
    }
    return string(name);
}

char *strcopy(const char *txt)
{
    size_t len  = strlen(txt);
    char  *copy = malloc(len + 1);
    return strcpy(copy, txt);
}

char *string(const struct String *string)
{
    if (!string) {
        return NULL;
    }
    char *copy = malloc(string->len + 1);
    strncpy(copy, string->text, string->len);
    copy[string->len] = '\0';
    return copy;
}

char *stringf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof buf, fmt, args);
    va_end(args);
    return strcopy(buf);
}
