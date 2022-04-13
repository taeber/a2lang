#include "ir.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Quad  head;
struct Quad *tail = &head;

static struct Quad *QuadAlloc(void)
{
    struct Quad *quad = malloc(sizeof(*quad));
    if (!quad) {
        perror("malloc");
        exit(1);
    }
    return quad;
}

static void Quad(enum Operation operation, struct String first, struct String second, struct String third)
{
    struct Quad *quad = QuadAlloc();
    quad->operation   = operation;
    quad->first       = first;
    quad->second      = second;
    quad->third       = third;
    quad->next        = NULL;
    tail = tail->next = quad;
}

#define Quad1(op) (Quad(op, Blank, Blank, Blank))
#define Quad2(op, first) (Quad(op, first, Blank, Blank))
#define Quad3(op, first, second) (Quad(op, first, second, Blank))

static struct String hex(int val) { return Stringf("$%0X", val); }
static struct String dec(int val) { return Stringf("%d", val); }
// static struct String loop(unsigned num) { return Stringf("_loop%d", num); }
// static struct String loopend(unsigned num) { return Stringf("_loop%dend", num); }
// static struct String cond(unsigned num) { return Stringf("_cond%d", num); }
// static struct String condend(unsigned num) { return Stringf("_cond%dend", num); }

static struct String num(struct Numerical *num)
{
    switch (num->type) {
    case NUM_NUMBER:
        return hex(num->Number);
    case NUM_IDENT:
        return num->Identifier.String;
    case NUM_NONE:
        return Blank;
    }
}

const char *ops[] = {
    "",
    "ASM",
    "HEX",
    "TXT",
    "SET",
    "EQU",
    "ORG",
    "VAR",
    "SUB",
    "RTS",
    "JSR",
};

void ASM(struct String assembly) { Quad2(OP_ASM, assembly); }
void EQU(struct String left, struct Numerical *right) { Quad3(OP_EQU, left, num(right)); }
void HEX(struct String name, char value) { Quad3(OP_HEX, name, Stringf("%02X", value)); }
void JSR(struct String subname) { Quad2(OP_JSR, subname); }
void ORG(int location) { Quad2(OP_ORG, hex(location)); }
void RTS(struct String subname) { Quad2(OP_RTS, subname); }
void SET(struct String name, struct String value) { Quad3(OP_SET, name, value); }
void SUB(struct String subname) { Quad2(OP_SUB, subname); }
void TXT(struct String name, struct String value) { Quad3(OP_TXT, name, value); }
void VAR(struct String name, unsigned size) { Quad3(OP_VAR, name, dec(size)); }

void WriteIntermediate(FILE *fp)
{
    for (struct Quad *p = head.next; p; p = p->next) {
        if (p->operation == OP_NONE) {
            continue;
        }
        if (p->operation == OP_ASM) {
            fprintf(fp, "%.*s", p->first.len, p->first.text);
            continue;
        }
        fprintf(fp, "!%s\t%.*s\t%.*s\t%.*s\n",
            ops[p->operation],
            p->first.len, p->first.text,
            p->second.len, p->second.text,
            p->third.len, p->third.text);
    }
}
