#pragma once

#include <stdio.h>

#include "parser.h"
#include "strings.h"

enum Operation {
    OP_NONE,
    OP_ASM,
    OP_HEX,
    OP_TXT,
    OP_SET,
    OP_EQU,
    OP_ORG,
    OP_VAR,
    OP_SUB,
    OP_RTS,
    OP_JSR,
};

struct Quad {
    enum Operation operation;
    struct String  first;
    struct String  second;
    struct String  third;
    struct Quad   *next;
};

void ASM(struct String assembly);

void HEX(struct String name, char value);
void TXT(struct String name, struct String value);

void EQU(struct String left, struct Numerical *right);
void ORG(int location);

void VAR(struct String name, unsigned size);
void SET(struct String name, struct String value);

void SUB(struct String subname);
void RTS(struct String subname);
void JSR(struct String subname);

void WriteIntermediate(FILE *fp);
