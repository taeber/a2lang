#pragma once

#include <stdint.h>
#include <stdio.h>

typedef void (*LoadOp)(char *operand);
typedef void (*StoreOp)(char *operand);

void ASM(char *assembly);
void REM(char *comment);
void TXT(const char *name, const char *ascii);
void VAR(const char *name, uint16_t size);

void EQU(const char *name, char *operand);

void JMP(char *location);
void JSR(char *name);
void RTS(const char *label);

void BEQ(char *label);
void BNE(char *label);
void BCC(char *label);
void BCS(char *label);

void CMP(char *operand);

void ADC(char *operand);
void CLC(void);

void INC(char *operand);
void INX(void);
void INY(void);

void DEC(char *operand);
void DEX(void);
void DEY(void);

void LDA(char *operand);
void LDX(char *operand);
void LDY(char *operand);
void STA(char *operand);
void STX(char *operand);
void STY(char *operand);

void TAY(void);
void TAX(void);
void TYA(void);

void Label(const char *label);

void WriteInstructions(FILE *fp);
