#pragma once

#include "parser.h"


/* Indicates a failure of a production rule to parse. */
#define NoParse (const char *)0

/* Production Rules from grammar.peg
The implementation is tedious, but fairly straight-forward in that each
production rule maps to a function. Each function takes the text to be parsed,
attempts to recognize some of it, and returns the remaining text along with some
semantic value if successful, or returns NoParse to indicate failure.

Note, since this is C, only the remaining text is actually "returned", but the
semantic values are an out-parameter.
*/

const char *Program(const char *text, struct Program *outProg);
const char *Block(const char *text, struct Block *outProg);
const char *Statements(const char *text, struct Block *outBlock);
const char *Statement(const char *text, struct Statement *outStatement);
const char *Declaration(const char *text, struct Declaration *outDecl);
const char *Variable(const char *text, struct Variable *outVar);
const char *Parameters(const char *text, struct Parameters *outParams);
const char *Parameter(const char *text, struct Parameter *outParam);
const char *Separator(const char *text);
const char *Definition(const char *text, struct Definition *outDefn);
const char *Arguments(const char *text, struct Arguments *outArgs);
const char *Argument(const char *text, struct Argument *outArg);
const char *Value(const char *text, struct Value *outValue);
const char *Identifier(const char *text, struct Identifier *outIdent);
const char *IdentPhrase(const char *text, struct IdentPhrase *outIdent);
const char *Type(const char *text, struct Type *outType);
const char *Array(const char *text, struct Array *outArray);
const char *Tuple(const char *text, struct Arguments *outTuple);
const char *Group(const char *text, struct Parameters *outGroup);
const char *Pointer(const char *text, struct Identifier *outType);
const char *Numerical(const char *text, struct Numerical *outNumerical);
const char *Subroutine(const char *text, struct Subroutine *outSub);
const char *Call(const char *text, struct Call *outCall);
const char *Location(const char *text, struct Numerical *outNumerical);
const char *Number(const char *text, int *outNumber);
const char *TextLiteral(const char *text, struct String *outText);
const char *CharLiteral(const char *text, char *outChar);
const char *Subscript(const char *text, struct Numerical *outNumerical);
const char *FieldAccess(const char *text, struct Identifier *outField);
const char *Assignment(const char *text, struct Assignment *outAssign);
const char *Conditional(const char *text, struct Conditional *outCond);
const char *Loop(const char *text, struct Conditional *outCond);
const char *Comparison(const char *text, struct Conditional *outCond);
const char *Compare(const char *text, enum Compare *outCompare);
const char *SimpleValue(const char *text, struct Value *outValue);
const char *Assembly(const char *text, struct String *outAsm);
const char *Whitespace(const char *text);
const char *HSpace(const char *text);
const char *Space(const char *text);
const char *Comment(const char *text);
const char *EndOfInput(const char *text);

const char *badText(void);
