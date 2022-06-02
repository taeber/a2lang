#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

// 6502 mneumonics
void JMP(char *location);
void JSR(char *name);
void RTS(void);

typedef void (*Branch)(char *);
void BEQ(char *label);
void BNE(char *label);
void BCC(char *label);
void BCS(char *label);

typedef void (*Compare)(char *);
void CMP(char *operand);
void CPX(char *operand);
void CPY(char *operand);

void ADC(char *operand);
void SBC(char *operand);
void CLC(void);
void SEC(void);

void AND(char *operand);
void ORA(char *operand);
void EOR(char *operand);

void ASL(void);

void INC(char *operand);
void INX(void);
void INY(void);

void DEC(char *operand);
void DEX(void);
void DEY(void);

void PHA(void);
void PLA(void);

typedef void (*Load)(char *);
void LDA(char *operand);
void LDX(char *operand);
void LDY(char *operand);

typedef void (*Store)(char *);
void STA(char *operand);
void STX(char *operand);
void STY(char *operand);

void TAX(void);
void TAY(void);
void TXA(void);
void TYA(void);

// MERLIN-style pseudo operations

void ASM(char *assembly);
void REM(char *comment);
void TXT(const char *name, const char *ascii);
void VAR(const char *name, uint16_t size);

void EQU(const char *name, char *operand);

// Add a label.
void Label(const char *label);
// Returns a copy of the label that was last added only if it doesn't have instructions.
char *UnusedLabel(void);

// Run the Asembly-level optimizer
void Optimize(void);

// Write all the instructions out to fp.
void WriteInstructions(FILE *fp);

// Constructors for the various types of Operands.
struct Operand;

struct Operand *OpImmediate(char *value, uint8_t size);
struct Operand *OpImmediateWord(char *lo, char *hi);
struct Operand *OpImmediateNumber(uint16_t num);
struct Operand *OpAbsolute(const char *base, uint8_t size);
struct Operand *OpOffset(const char *base, char *offset, bool isVariable, uint8_t size);
struct Operand *OpIndirectOffset(const char *pointer, char *offset, uint8_t size);
struct Operand *OpRegister(char reg);
struct Operand *OpRegisterWord(char reghi, char reglo);

void FreeOperand(struct Operand *operand);

// What follows could be thought of as macro instructions.

// dst := src
void COPY(const struct Operand *dst, const struct Operand *src);
// dst += src
void PLUS(const struct Operand *dst, const struct Operand *src);
// dst -= src
void LESS(const struct Operand *dst, const struct Operand *src);
// dst &= src
void BITAND(const struct Operand *dst, const struct Operand *src);
// dst |= src
void OR(const struct Operand *dst, const struct Operand *src);
// dst ^= src
void XOR(const struct Operand *dst, const struct Operand *src);
// dst != src
void NOT(const struct Operand *dst, const struct Operand *src);

typedef void (*COND)(const struct Operand *, const struct Operand *, const char *, const char *);
// left == right
void IFEQ(const struct Operand *left, const struct Operand *right, const char *then, const char *done);
// left <> right
void IFNE(const struct Operand *left, const struct Operand *right, const char *then, const char *done);
// left < right
void IFLT(const struct Operand *left, const struct Operand *right, const char *then, const char *done);
// left <= right
void IFLE(const struct Operand *left, const struct Operand *right, const char *then, const char *done);
// left >= right
void IFGE(const struct Operand *left, const struct Operand *right, const char *then, const char *done);
// left > right
void IFGT(const struct Operand *left, const struct Operand *right, const char *then, const char *done);

// pointer := src
void ADDR(const char *pointer, const struct Operand *src);
