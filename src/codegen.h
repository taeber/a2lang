#pragma once

#include <stdint.h>

#include "asm.h"
#include "parser.h"
#include "symbols.h"

void declareSubroutine(const struct String *name, const struct Subroutine *subr, const struct Numerical *loc);
void declareParameters(struct Symbol *subsym, const struct Parameters *params);
void declareOutputs(struct Symbol *subsym, const struct Parameters *params);

void defineGroup(const struct String *name, const struct Parameters *members);
void defineSubroutine(const struct String *name, const struct Subroutine *subr);
void defineType(const struct String *name, const struct Type *type);

const char *defineText(const struct String *label, const struct String *text);

void generateAssembly(const struct Assembly *assembly);

void generateAssignment(const struct Assignment *assign);
void generateSet(const struct IdentPhrase *lhs, const struct Value *value);
void generateArithmetic(const struct IdentPhrase *lhs, const struct Value *rhs, char kind);

void generateBlock(const struct Block *block);
void generateCall(const struct Call *call);
void generateConditional(const struct Conditional *cond, bool isLoop);
void generateDeclarations(const struct Declaration *declaration);
void generateDeclaration(const struct Parameter *decl);
void generateVariables(const struct Variable *Variable);
void generateVariable(const struct Parameter *var);
void generateDefinitions(const struct Definition *definitions);
void generateDefinition(const struct Argument *def);
void generateLiteralChar(const struct String *name, char ch);
void generateLiteralNumber(const struct String *name, int number);
void generateStatement(const struct Statement *stmt);

struct Location location(const struct Numerical *loc);
uint16_t        number(const struct Numerical *num);
struct TypeInfo typeinfo(const struct Type *type);

char *indextxt(const struct Numerical *num);
