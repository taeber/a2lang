#include "asm.h"

#include <stdio.h>
#include <string.h>

#include "io.h"
#include "text.h"

#include "asm-op.c"

typedef const char *Operation;

struct Instruction {
    char      label[256];
    Operation op;
    char      operand[256];

    char *assembly;
    char *comment;

    struct Instruction *next;
};

static Operation OP_ADC = "ADC",
                 OP_ASC = "ASC",
                 OP_ASL = "ASL",
                 OP_BCC = "BCC",
                 OP_BCS = "BCS",
                 OP_BEQ = "BEQ",
                 OP_BNE = "BNE",
                 OP_CLC = "CLC",
                 OP_CMP = "CMP",
                 OP_CPX = "CPX",
                 OP_CPY = "CPY",
                 OP_DEC = "DEC",
                 OP_DEX = "DEX",
                 OP_DEY = "DEY",
                 OP_EQU = "EQU",
                 OP_INC = "INC",
                 OP_INX = "INX",
                 OP_INY = "INY",
                 OP_JMP = "JMP",
                 OP_JSR = "JSR",
                 OP_HEX = "HEX",
                 OP_LDA = "LDA",
                 OP_LDX = "LDX",
                 OP_LDY = "LDY",
                 OP_NOP = "NOP",
                 OP_PHA = "PHA",
                 OP_PLA = "PLA",
                 OP_RTS = "RTS",
                 OP_SBC = "SBC",
                 OP_SEC = "SEC",
                 OP_STA = "STA",
                 OP_STX = "STX",
                 OP_STY = "STY",
                 OP_TAX = "TAX",
                 OP_TAY = "TAY",
                 OP_TXA = "TXA",
                 OP_TYA = "TYA";

static struct Instruction  codeHead;
static struct Instruction *code = &codeHead;

static char unusedLabel[256] = { 0 };

static struct Instruction dataHead;
static struct Instruction *data = &dataHead;

static struct Instruction *Instruction(
    const char *label,
    Operation   op,
    char       *operand,
    char       *assembly,
    char       *comment)
{
    struct Instruction *instruction = calloc(1, sizeof(*instruction));
    require(instruction, "calloc failed");

    if (label) {
        strncpy(instruction->label, label, sizeof instruction->label);
    }

    instruction->op = op;

    if (operand) {
        strncpy(instruction->operand, operand, sizeof instruction->operand);
        free(operand);
    }

    instruction->assembly = assembly;
    instruction->comment  = comment;

    instruction->next = NULL;

    return instruction;
}

static void FreeInstruction(struct Instruction *instruction)
{
    if (instruction->assembly) {
        free(instruction->assembly);
        instruction->assembly = NULL;
    }
    if (instruction->comment) {
        free(instruction->comment);
        instruction->comment = NULL;
    }
    instruction->next = NULL;
}

static void addCode(const char *label, const char *op, char *operand)
{
    if (unusedLabel[0] == '\0') {
        code = code->next = Instruction(label, op, operand, NULL, NULL);
        return;
    }
    code = code->next = Instruction(unusedLabel, op, operand, NULL, NULL);
    if (label) {
        EQU(label, strcopy(unusedLabel));
    }
    unusedLabel[0] = '\0';
}

void ADC(char *operand) { addCode(NULL, OP_ADC, operand); }
void ASL(void) { addCode(NULL, OP_ASL, NULL); }

void ASM(char *assembly)
{
    if (unusedLabel[0] != '\0') {
        code = code->next = Instruction(unusedLabel, OP_NOP, NULL, NULL, NULL);
    }
    code = code->next = Instruction(NULL, NULL, NULL, assembly, NULL);
}

void BCC(char *operand) { addCode(NULL, OP_BCC, operand); }
void BCS(char *operand) { addCode(NULL, OP_BCS, operand); }
void BEQ(char *operand) { addCode(NULL, OP_BEQ, operand); }
void BNE(char *operand) { addCode(NULL, OP_BNE, operand); }
void CLC(void) { addCode(NULL, OP_CLC, NULL); }
void CMP(char *operand) { addCode(NULL, OP_CMP, operand); }
void CPX(char *operand) { addCode(NULL, OP_CPX, operand); }
void CPY(char *operand) { addCode(NULL, OP_CPY, operand); }
void DEC(char *operand) { addCode(NULL, OP_DEC, operand); }
void DEX(void) { addCode(NULL, OP_DEX, NULL); }
void DEY(void) { addCode(NULL, OP_DEY, NULL); }
void EQU(const char *name, char *operand)
{
    code = code->next = Instruction(name, OP_EQU, operand, NULL, NULL);
}
void INC(char *operand) { addCode(NULL, OP_INC, operand); }
void INX(void) { addCode(NULL, OP_INX, NULL); }
void INY(void) { addCode(NULL, OP_INY, NULL); }
void JMP(char *location) { addCode(NULL, OP_JMP, location); }
void JSR(char *name) { addCode(NULL, OP_JSR, name); }
void Label(const char *label)
{
    if (unusedLabel[0] != '\0') {
        EQU(label, strcopy(unusedLabel));
        return;
    }
    strncpy(unusedLabel, label, sizeof unusedLabel);
}
void LDA(char *operand) { addCode(NULL, OP_LDA, operand); }
void LDX(char *operand) { addCode(NULL, OP_LDX, operand); }
void LDY(char *operand) { addCode(NULL, OP_LDY, operand); }

void Optimize(void)
{
    // Associates the remaining unusedLabel with a NOP. This is not really an
    // optimization; rather, it's required for proper execution of the code.
    if (unusedLabel[0] != '\0') {
        code = code->next = Instruction(unusedLabel, OP_NOP, NULL, NULL, NULL);
        unusedLabel[0] = '\0';
    }

    struct Instruction *pred = NULL, *curr = NULL, *succ = NULL;

    curr = codeHead.next;
    while (curr) {
        succ = curr->next;

        if (succ) {
            if (!*curr->label && curr->op == OP_JSR
                && !*succ->label && succ->op == OP_RTS) {
                // JSR + RTS => JMP
                curr->op = OP_JMP;
                curr->next = succ->next;
                FreeInstruction(succ);
                succ = curr->next;
            }
        }

        pred = curr;
        curr = succ;
    }
}

void PHA(void) { addCode(NULL, OP_PHA, NULL); }
void PLA(void) { addCode(NULL, OP_PLA, NULL); }
void REM(char *comment) { code = code->next = Instruction(NULL, NULL, NULL, NULL, comment); }
void RTS(void) { addCode(NULL, OP_RTS, NULL); }
void SBC(char *operand) { addCode(NULL, OP_SBC, operand); }
void SEC(void) { addCode(NULL, OP_SEC, NULL); }
void STA(char *operand) { addCode(NULL, OP_STA, operand); }
void STX(char *operand) { addCode(NULL, OP_STX, operand); }
void STY(char *operand) { addCode(NULL, OP_STY, operand); }
void TAX(void) { addCode(NULL, OP_TAX, NULL); }
void TAY(void) { addCode(NULL, OP_TAY, NULL); }
void TXA(void) { addCode(NULL, OP_TXA, NULL); }
void TYA(void) { addCode(NULL, OP_TYA, NULL); }

void TXT(const char *name, const char *text)
{
    for (const char *ch = text; *ch != '\0'; ch++) {
        if (*ch == '\\') {
            warnf("Escape sequences are unsupported:");
            warnf("  %s", text);
            warnf("  %-*s^^", (int)(ch - text), "");
        }
    }
    data = data->next = Instruction(name, OP_ASC, stringf("\"%s\"", text), NULL, NULL);
    data = data->next = Instruction(NULL, OP_HEX, strcopy("00"), NULL, NULL);
}

char *UnusedLabel(void) { return unusedLabel[0] != '\0' ? strcopy(unusedLabel) : NULL; }

void VAR(const char *name, uint16_t size)
{
    static const unsigned maxPerLine = 32;
    require(size > 0, "Variable %s cannot have size 0", name);
    unsigned zeros = size * 2;
    while (zeros > maxPerLine) {
        data = data->next = Instruction(name, OP_HEX, stringf("%.*x", maxPerLine, 0), NULL, NULL);
        name              = NULL;
        zeros -= maxPerLine;
    }
    data = data->next = Instruction(name, OP_HEX, stringf("%.*x", zeros, 0), NULL, NULL);
}

static void WriteInstruction(FILE *fp, struct Instruction *p)
{
    // Inline assembly or comment
    if (p->assembly) {
        fputs(p->assembly, fp);
        return;
    }

    if (p->comment) {
        fprintf(fp, "* %s\n", p->comment);
        return;
    }

    fprintf(fp, "%s\t%s", p->label, p->op);
    if (p->operand[0] != '\0') {
        fprintf(fp, " %s", p->operand);
    }
    fputc('\n', fp);
}

void WriteInstructions(FILE *fp)
{
    for (struct Instruction *p = codeHead.next; p; p = p->next) {
        WriteInstruction(fp, p);
    }
    for (struct Instruction *p = dataHead.next; p; p = p->next) {
        WriteInstruction(fp, p);
    }
}
