#pragma once

#include <stdint.h>

#include "parser.h"

// Conversions to malloc'd C-strings
char *asciich(char ch);
char *hex2(uint8_t x);
char *hex4(uint16_t x);
char *numerical(const struct Numerical *val);
char *phrase(const struct IdentPhrase *val);
char *string(const struct String *str);
char *stringf(const char *fmt, ...);
char *strcopy(const char *txt);
char *qualify(const struct String *scope, const struct String *name);

char *absoluteX(const char *value);
char *absoluteY(const char *value);
char *lo(const char *value);
char *hi(const char *value);

// WARNING: value will be free'd.
char *immediate(char *value);
char *indirectY(char *value);

char *offset(const char *lbl, int16_t off);
