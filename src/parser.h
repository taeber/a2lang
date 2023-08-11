#pragma once

#include <stdio.h>

struct String {
    unsigned    len;
    const char *text;
};

struct Identifier {
    struct String String;
};

struct Parameter;
struct Parameters {
    unsigned          len;
    struct Parameter *parameters;
};

struct Declaration {
    struct Parameters Parameters;
};

struct Variable {
    struct Parameters Parameters;
};

struct Argument;
struct Arguments {
    unsigned         len;
    struct Argument *arguments;
};

struct Definition {
    struct Arguments Arguments;
};

struct Assembly {
    struct String String;
};

struct Block {
    unsigned          len;
    struct Statement *statements;
};

struct Program {
    struct Block block;
};

struct Subroutine {
    struct Parameters input;
    struct Parameters output;
    struct Block      block;
};

enum NumericalType {
    NUM_NONE,
    NUM_NUMBER,
    NUM_IDENT,
};

struct Numerical {
    enum NumericalType type;
    union {
        struct Identifier Identifier;
        int               Number;
    };
};

struct IdentPhrase {
    struct Identifier  identifier;
    struct Numerical  *subscript;
    struct Identifier *field;
};

struct Call {
    struct IdentPhrase ident;
    struct Arguments   args;
};

struct Array {
    struct Identifier type;
    struct Numerical  size;
};

enum TypeType {
    TYPE_UNKNOWN,
    TYPE_SUBROUTINE,
    TYPE_ARRAY,
    TYPE_POINTER,
    TYPE_IDENT,
};

struct Type {
    enum TypeType type;
    union {
        struct Subroutine Subroutine;
        struct Identifier Identifier;
        struct Array      Array;
        struct Identifier Pointer;
    };
};

struct Parameter {
    struct Identifier name;
    struct Type       type;
    struct Numerical  loc;
};

enum ValueType {
    VAL_UNKNOWN,
    VAL_NUMBER,
    VAL_TEXT,
    VAL_CHAR,
    VAL_SUB,
    VAL_CALL,
    VAL_IDENT,
    VAL_TUPLE,
    VAL_GROUPTYPE,
    VAL_TYPE,
};

struct Value {
    enum ValueType type;
    union {
        int                Number;
        struct String      Text;
        char               Char;
        struct Subroutine  Subroutine;
        struct Call        Call;
        struct IdentPhrase IdentPhrase;
        struct Arguments   Tuple;
        struct Parameters  Group;
        struct Type        Type;
    };
};

struct Argument {
    struct Identifier name;
    struct Value      value;
    const char       *_text;
};

struct Assignment {
    struct IdentPhrase ident;
    char               kind;
    struct Value       value;
};

enum Compare {
    COMP_EQUAL,
    COMP_NOTEQUAL,
    COMP_LESS,
    COMP_LESSEQUAL,
    COMP_GREATER,
    COMP_GREATEREQUAL,
    COMP_ALWAYS,
};

struct Conditional {
    struct Value left;
    enum Compare compare;
    struct Value right;
    struct Block then;
    const char  *_text;
};

enum StatementType {
    STMT_UNKNOWN,
    STMT_DECLARATION,
    STMT_VARIABLE,
    STMT_DEFINITION,
    STMT_CALL,
    STMT_ASSIGN,
    STMT_COND,
    STMT_LOOP,
    STMT_RETURN,
    STMT_STOP,
    STMT_REPEAT,
    STMT_ASSEMBLY,
};

struct Statement {
    enum StatementType type;
    union {
        struct Declaration Declaration;
        struct Variable    Variable;
        struct Definition  Definition;
        struct Call        Call;
        struct Assignment  Assignment;
        struct Conditional Conditional;
        struct Assembly    Assembly;
    };
};

void WriteAST(FILE *output, struct Program *prog);
