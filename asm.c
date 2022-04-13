#include "asm.h"

#include <stdio.h>
#include <string.h>

#include "io.h"
#include "text.h"

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
                 OP_BCC = "BCC",
                 OP_BCS = "BCS",
                 OP_BEQ = "BEQ",
                 OP_BNE = "BNE",
                 OP_CLC = "CLC",
                 OP_CMP = "CMP",
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
                 OP_STA = "STA",
                 OP_STX = "STX",
                 OP_STY = "STY",
                 OP_TAX = "TAX",
                 OP_TAY = "TAY",
                 OP_TYA = "TYA";

static struct Instruction  directiveHead;
static struct Instruction *directive = &directiveHead;

static struct Instruction  codeHead;
static struct Instruction *code = &codeHead;

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

    instruction->op    = op;

    if (operand) {
        strncpy(instruction->operand, operand, sizeof instruction->operand);
        free(operand);
    }

    instruction->assembly = assembly;
    instruction->comment  = comment;

    instruction->next = NULL;

    return instruction;
}

static void addCode(const char *label, const char *op, char *operand)
{
    code = code->next = Instruction(label, op, operand, NULL, NULL);
}

void ADC(char *operand) { addCode(NULL, OP_ADC, operand); }
void ASM(char *assembly) { code = code->next = Instruction(NULL, NULL, NULL, assembly, NULL); }
void BCC(char *operand) { addCode(NULL, OP_BCC, operand); }
void BCS(char *operand) { addCode(NULL, OP_BCS, operand); }
void BEQ(char *operand) { addCode(NULL, OP_BEQ, operand); }
void BNE(char *operand) { addCode(NULL, OP_BNE, operand); }
void CLC(void) { addCode(NULL, OP_CLC, NULL); }
void CMP(char *operand) { addCode(NULL, OP_CMP, operand); }
void DEC(char *operand) { addCode(NULL, OP_DEC, operand); }
void DEX(void) { addCode(NULL, OP_DEX, NULL); }
void DEY(void) { addCode(NULL, OP_DEY, NULL); }
void EQU(const char *name, char *operand)
{
    directive = directive->next = Instruction(name, OP_EQU, operand, NULL, NULL);
}
void INC(char *operand) { addCode(NULL, OP_INC, operand); }
void INX(void) { addCode(NULL, OP_INX, NULL); }
void INY(void) { addCode(NULL, OP_INY, NULL); }
void JMP(char *location) { addCode(NULL, OP_JMP, location); }
void JSR(char *name) { addCode(NULL, OP_JSR, name); }
void LDA(char *operand) { addCode(NULL, OP_LDA, operand); }
void LDX(char *operand) { addCode(NULL, OP_LDX, operand); }
void LDY(char *operand) { addCode(NULL, OP_LDY, operand); }
void Label(const char *label) { addCode(label, NULL, NULL); }
void REM(char *comment) { code = code->next = Instruction(NULL, NULL, NULL, NULL, comment); }
void RTS(const char *label) { addCode(label, "RTS", NULL); }
void STA(char *operand) { addCode(NULL, OP_STA, operand); }
void STX(char *operand) { addCode(NULL, OP_STX, operand); }
void STY(char *operand) { addCode(NULL, OP_STY, operand); }
void TAX(void) { addCode(NULL, OP_TAX, NULL); }
void TAY(void) { addCode(NULL, OP_TAY, NULL); }
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

void VAR(const char *name, uint16_t size)
{
    static const unsigned maxPerLine = 32;
    require(size > 0, "Variable %s cannot have size 0", name);
    int zeros = size * 2;
    while (zeros > maxPerLine) {
        data = data->next = Instruction(name, OP_HEX, stringf("%.*x", maxPerLine, 0), NULL, NULL);
        name              = NULL;
        zeros -= maxPerLine;
    }
    data = data->next = Instruction(name, OP_HEX, stringf("%.*x", zeros, 0), NULL, NULL);
}

static const char *WriteInstruction(FILE *fp, struct Instruction *p, const char *label)
{
    // Inline assembly or comment
    if (p->assembly) {
        fputs(p->assembly, fp);
        return label;
    }

    if (p->comment) {
        fprintf(fp, "\t* %s\n", p->comment);
        return label;
    }

    if (label && p->label[0] != '\0') {
        fprintf(fp, "%s\tEQU %s\n", label, p->label);
        return WriteInstruction(fp, p, NULL);
    }

    if (p->label[0] != '\0' && !p->op) {
        return p->label;
    }

    if (!label) {
        label = p->label;
    }

    fprintf(fp, "%s\t%s", label ? label : "", p->op);
    if (p->operand[0] != '\0') {
        fprintf(fp, " %s", p->operand);
    }
    fputc('\n', fp);
    return NULL;
}

void WriteInstructions(FILE *fp)
{
    const char *label = NULL;
    for (struct Instruction *p = directiveHead.next; p; p = p->next) {
        label = WriteInstruction(fp, p, label);
    }
    require(!label, "floating label after directives: %s", label);
    for (struct Instruction *p = codeHead.next; p; p = p->next) {
        label = WriteInstruction(fp, p, label);
    }
    if (label) {
        fprintf(fp, "%s\tNOP\n", label);
        label = NULL;
    }
    for (struct Instruction *p = dataHead.next; p; p = p->next) {
        label = WriteInstruction(fp, p, label);
    }
    if (label) {
        fprintf(fp, "%s\tHEX 00\n", label);
        label = NULL;
    }
}
