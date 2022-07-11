#include "grammar.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "io.h"
#include "parser.h"

static const char *start;
static const char *possibleBadText;

#define copy(dst, src) (memcpy((dst), (src), sizeof *(dst)));
#define dupe(T) (memcpy(malloc(sizeof *(T)), (T), sizeof *(T)))
#define append(arr, len, item)                         \
    do {                                               \
        (len)++;                                       \
        (arr) = realloc((arr), (len) * sizeof *(arr)); \
        copy((arr) + (len)-1, (item));                 \
    } while (0);

static inline bool isDigit(char ch) { return (ch >= '0' && ch <= '9'); }
static inline bool isLower(char ch) { return (ch >= 'a' && ch <= 'z'); }
static inline bool isUpper(char ch) { return (ch >= 'A' && ch <= 'Z'); }

static inline bool isIdentStart(char ch) { return isLower(ch) || isUpper(ch); }
static inline bool isIdentCont(char ch) { return isIdentStart(ch) || isDigit(ch); }

static const char *consume(const char *text, char expected)
{
    if (*text == expected) {
        return Whitespace(text + 1);
    }
    return NoParse;
}

static const char *consumeToken(const char *text, const char *token, bool isSpaceRequired)
{
    size_t len = strlen(token);
    if (strncmp(token, text, len) == 0) {
        text += len;
        if (!isSpaceRequired) {
            return Whitespace(text);
        }
        if ((text = Space(text))) {
            return Whitespace(text);
        }
    }
    return NoParse;
}

static const char *additionalArgument(const char *text, struct Argument *arg)
{
    if ((text = Separator(text))) {
        if ((text = Argument(text, arg))) {
            return text;
        }
    }
    return NoParse;
}

static const char *additionalParameter(const char *text, struct Parameter *param)
{
    if ((text = Separator(text))) {
        if ((text = Parameter(text, param))) {
            return text;
        }
    }
    return NoParse;
}

const char *Argument(const char *text, struct Argument *outArg)
{
    outArg->_text = text;
    const char *remaining;
    if ((remaining = Identifier(text, &outArg->name))) {
        if ((remaining = consume(remaining, '='))) {
            if ((remaining = Value(remaining, &outArg->value))) {
                return remaining;
            }
        }
    }
    if ((remaining = Value(text, &outArg->value))) {
        outArg->name.String.len = 0;
        return remaining;
    }
    return NoParse;
}

const char *Arguments(const char *text, struct Arguments *outArgs)
{
    struct Argument arg;
    const char     *remaining;

    if ((remaining = consume(text, '('))) {
        const char *remaining2;
        outArgs->len       = 0;
        outArgs->arguments = NULL;

        if ((remaining2 = consume(remaining, ')'))) {
            return remaining2;
        }

        if ((remaining2 = Argument(remaining, &arg))) {
            append(outArgs->arguments, outArgs->len, &arg);
            remaining = remaining2;

            while ((remaining2 = additionalArgument(remaining2, &arg))) {
                append(outArgs->arguments, outArgs->len, &arg);
                remaining = remaining2;
            }
        }

        if ((remaining2 = Separator(remaining))) {
            remaining = remaining2;
        }

        return consume(remaining, ')');
    }

    if ((remaining = Argument(text, &arg))) {
        outArgs->len       = 1;
        outArgs->arguments = dupe(&arg);
        return remaining;
    }

    return NoParse;
}

const char *Array(const char *text, struct Array *outArray)
{
    if ((text = Identifier(text, &outArray->type))) {
        if (text[0] == '^') {
            return Numerical(text+1, &outArray->size);
        }
    }
    return NoParse;
}

const char *Assembly(const char *text, struct String *outAsm)
{
    if ((text = consumeToken(text, "asm", false))) {
        // Special handling is done for asm blocks since the result is
        // whitespace-sensitive. In short, any space up to and including a
        // newline directly after the opening brace is disgarded.
        if (*text == '{') {
            text++;
            while (*text == ' ') {
                text++;
            }
            if (*text == '\n') {
                text++;
            }
            outAsm->len  = 0;
            outAsm->text = text;
            while (*text != '}') {
                outAsm->len++;
                text++;
            }
            text = consume(text, '}');
            return Whitespace(text);
        }
    }
    return NoParse;
}

const char *Assignment(const char *text, struct Assignment *outAssign)
{
    if ((text = IdentPhrase(text, &outAssign->ident))) {
        switch (*text) {
        case ':':
        case '+':
        case '\\':
        case '-':
        case '&':
        case '|':
        case '^':
        case '!':
            outAssign->kind = *text;
            text++;
            if ((text = consume(text, '='))) {
                if ((text = Value(text, &outAssign->value))) {
                    return text;
                }
            }
        default:
            break;
        }
    }
    return NoParse;
}

const char *Block(const char *text, struct Block *outBlock)
{
    if ((text = consume(text, '{'))) {
        const char *remaining;
        if ((remaining = Statements(text, outBlock))) {
            text = remaining;
        }
        return consume(text, '}');
    }
    return NoParse;
}

const char *Call(const char *text, struct Call *outCall)
{
    if ((text = IdentPhrase(text, &outCall->ident))) {
        if (*text == '(') {
            if ((text = Arguments(text, &outCall->args))) {
                return text;
            }
        }
        // Cleanup IP since we're backing out.
        if (outCall->ident.subscript) {
            free(outCall->ident.subscript);
            outCall->ident.subscript = NULL;
        }
        if (outCall->ident.field) {
            free(outCall->ident.field);
            outCall->ident.field = NULL;
        }
    }
    return NoParse;
}

const char *CharLiteral(const char *text, char *outChar)
{
    if (text[0] == '`') {
        if (text[1] >= ' ' && text[1] <= '~') {
            *outChar = text[1];
            return Whitespace(text + 2);
        }
    }
    return NoParse;
}

const char *Comment(const char *text)
{
    if (*text == ';') {
        while (*text != '\n') {
            text++;
        }
        return text + 1;
    }
    return NoParse;
}

const char *Compare(const char *text, enum Compare *outCompare)
{
    const char *remaining;
    if ((remaining = consumeToken(text, "<>", false))) {
        *outCompare = COMP_NOTEQUAL;
        return remaining;
    }
    if ((remaining = consumeToken(text, "==", false))) {
        *outCompare = COMP_EQUAL;
        return remaining;
    }
    if ((remaining = consumeToken(text, "<=", false))) {
        *outCompare = COMP_LESSEQUAL;
        return remaining;
    }
    if ((remaining = consumeToken(text, ">=", false))) {
        *outCompare = COMP_GREATEREQUAL;
        return remaining;
    }
    if ((remaining = consume(text, '<'))) {
        *outCompare = COMP_LESS;
        return remaining;
    }
    if ((remaining = consume(text, '>'))) {
        *outCompare = COMP_GREATER;
        return remaining;
    }
    return NoParse;
}

const char *Comparison(const char *text, struct Conditional *outCond)
{
    if ((text = SimpleValue(text, &outCond->left))) {
        if ((text = Compare(text, &outCond->compare))) {
            return SimpleValue(text, &outCond->right);
        }
    }
    return NoParse;
}

const char *Conditional(const char *text, struct Conditional *outCond)
{
    if ((text = consumeToken(text, "if", true))) {
        if ((text = Comparison(text, outCond))) {
            outCond->_text = text;
            return Block(text, &outCond->then);
        }
    }
    return NoParse;
}

const char *Declaration(const char *text, struct Declaration *outDecl)
{
    if ((text = consumeToken(text, "use", true))) {
        if ((text = Parameters(text, &outDecl->Parameters))) {
            // TODO: We have VAR Parameters, so we need to return MULTIPLE declarations?
            return text;
        }
    }
    return NoParse;
}

const char *Definition(const char *text, struct Definition *outDefn)
{
    if ((text = consumeToken(text, "let", true))) {
        if ((text = Arguments(text, &outDefn->Arguments))) {
            // TODO: We have LET Arguments, so we need to return MULTIPLE declarations?
            return text;
        }
    }
    return NoParse;
}

const char *EndOfInput(const char *text)
{
    if (*text == '\0') {
        return text;
    }
    return NoParse;
}

const char *FieldAccess(const char *text, struct Identifier *outField)
{
    if ((text = consume(text, '.'))) {
        if ((text = Identifier(text, outField))) {
            return text;
        }
    }
    return NoParse;
}

const char *HSpace(const char *text)
{
    if (*text == ' ' || *text == '\t') {
        return text + 1;
    }
    return NoParse;
}

const char *Group(const char *text, struct Parameters *outGroup)
{
    if (*text == '[') {
        return Parameters(text, outGroup);
    }
    return NoParse;
}

const char *Identifier(const char *text, struct Identifier *outIdent)
{
    unsigned len = 0;

    if (isIdentStart(text[len])) {
        do {
            len++;
        } while (isIdentCont(text[len]));
        outIdent->String.len  = len;
        outIdent->String.text = text;

        return Whitespace(text + len);
    }

    return NoParse;
}

const char *IdentPhrase(const char *text, struct IdentPhrase *outIdent)
{
    struct Numerical  subscript;
    struct Identifier field;

    if ((text = Identifier(text, &outIdent->identifier))) {
        const char *remaining;

        remaining = text;
        if ((remaining = Subscript(text, &subscript))) {
            outIdent->subscript = dupe(&subscript);
            text                = remaining;
        } else {
            outIdent->subscript = NULL;
        }

        remaining = text;
        if ((remaining = FieldAccess(remaining, &field))) {
            outIdent->field = dupe(&field);
            text            = remaining;
        } else {
            outIdent->field = NULL;
        }

        return text;
    }

    return NoParse;
}

const char *Location(const char *text, struct Numerical *outNum)
{
    if ((text = consume(text, '@'))) {
        return Numerical(text, outNum);
    }
    return NoParse;
}

const char *Loop(const char *text, struct Conditional *outCond)
{
    if ((text = consumeToken(text, "while", true))) {
        if ((text = Comparison(text, outCond))) {
            outCond->_text = text;
            return Block(text, &outCond->then);
        }
    }
    return NoParse;
}

const char *Number(const char *text, int *num)
{
    const char *remaining;
    if (*text == '$') {
        remaining = text + 1;
        char *end;
        *num = strtoul(remaining, &end, 16);
        if (end != remaining) {
            return Whitespace(end);
        } else {
            return NoParse;
        }
    }

    if (isDigit(*text) || *text == '-') {
        remaining = text;
        char *end;
        *num = strtol(remaining, &end, 10);
        if (end != remaining) {
            return Whitespace(end);
        } else {
            return NoParse;
        }
    }

    if (*text == '%') {
        remaining = text + 1;
        char *end;
        *num = strtoul(remaining, &end, 2);
        if (end != remaining) {
            return Whitespace(end);
        } else {
            return NoParse;
        }
    }

    return NoParse;
}

const char *Numerical(const char *text, struct Numerical *outNumerical)
{
    const char *remaining;
    outNumerical->type = NUM_NONE;
    if ((remaining = Number(text, &outNumerical->Number))) {
        outNumerical->type = NUM_NUMBER;
        return remaining;
    }
    if ((remaining = Identifier(text, &outNumerical->Identifier))) {
        outNumerical->type = NUM_IDENT;
        return remaining;
    }
    return NoParse;
}

const char *Parameter(const char *text, struct Parameter *outParam)
{
    if ((text = Identifier(text, &outParam->name))) {
        if ((text = Type(text, &outParam->type))) {
            const char *remaining;
            if ((remaining = Location(text, &outParam->loc))) {
                return remaining;
            }
            outParam->loc.type = NUM_NONE;
            return text;
        }
    }
    return NoParse;
}

const char *Parameters(const char *text, struct Parameters *outParams)
{
    struct Parameter param;
    const char      *remaining;

    if ((remaining = consume(text, '['))) {
        const char *remaining2;
        outParams->len        = 0;
        outParams->parameters = NULL;

        if ((remaining2 = consume(remaining, ']'))) {
            return remaining2;
        }

        if ((remaining2 = Parameter(remaining, &param))) {
            append(outParams->parameters, outParams->len, &param);
            remaining = remaining2;

            while ((remaining2 = additionalParameter(remaining2, &param))) {
                append(outParams->parameters, outParams->len, &param);
                remaining = remaining2;
            }
        } else {
            return NoParse;
        }

        if ((remaining2 = Separator(remaining))) {
            remaining = remaining2;
        }

        if (*remaining != ']') {
            // Probably missing a comma
            possibleBadText = remaining;
            return NoParse;
        }

        return consume(remaining, ']');
    }

    if ((remaining = Parameter(text, &param))) {
        outParams->len        = 1;
        outParams->parameters = dupe(&param);
        return remaining;
    }

    return NoParse;
}

const char *Pointer(const char *text, struct Identifier *outType)
{
    struct Numerical num;
    if ((text = Identifier(text, outType))) {
        if (text[0] == '^') {
            text++;
            if (!Numerical(text, &num)) {
                return Whitespace(text);
            }
        }
    }
    return NoParse;
}

const char *Program(const char *text, struct Program *outProg)
{
    start = text;
    text = Whitespace(text);
    if ((text = Statements(text, &outProg->block))) {
        if ((text = EndOfInput(text))) {
            return text;
        }
    }
    return NoParse;
}

const char *Separator(const char *text)
{
    if (*text == ',') {
        return consume(text, ',');
    }
    // As a convenience, the comma is not required if a newline is used.
    const char *ch = text-1;
    while (ch != start && (*ch == ' ' || *ch == '\t')) {
        ch--;
    }
    if (ch != start && *ch == '\n') {
        return text;
    }
    return NoParse;
}

const char *SimpleValue(const char *text, struct Value *outValue)
{
    if (*text != ':' && *text != '[' && *text != '{' && *text != '(' && *text != '"') {
        return Value(text, outValue);
    }
    return NoParse;
}

const char *Space(const char *text)
{
    if (*text == '\n' || HSpace(text)) {
        return text + 1;
    }
    return NoParse;
}

const char *Statement(const char *text, struct Statement *outStatement)
{
    const char *remaining;
    if ((remaining = Declaration(text, &outStatement->Declaration))) {
        outStatement->type = STMT_DECLARATION;
        return remaining;
    }
    if ((remaining = Variable(text, &outStatement->Variable))) {
        outStatement->type = STMT_VARIABLE;
        return remaining;
    }
    if ((remaining = Definition(text, &outStatement->Definition))) {
        outStatement->type = STMT_DEFINITION;
        return remaining;
    }
    if ((remaining = Call(text, &outStatement->Call))) {
        outStatement->type = STMT_CALL;
        return remaining;
    }
    if ((remaining = Assignment(text, &outStatement->Assignment))) {
        outStatement->type = STMT_ASSIGN;
        return remaining;
    }
    if ((remaining = Conditional(text, &outStatement->Conditional))) {
        outStatement->type = STMT_COND;
        return remaining;
    }
    if ((remaining = Loop(text, &outStatement->Conditional))) {
        outStatement->type = STMT_LOOP;
        return remaining;
    }
    if ((remaining = consumeToken(text, "->", false))) {
        outStatement->type = STMT_RETURN;
        return remaining;
    }
    if ((remaining = consumeToken(text, "stop", true))) {
        outStatement->type = STMT_STOP;
        return remaining;
    }
    if ((remaining = consumeToken(text, "repeat", true))) {
        outStatement->type = STMT_REPEAT;
        return remaining;
    }
    if ((remaining = Assembly(text, &outStatement->Assembly.String))) {
        outStatement->type = STMT_ASSEMBLY;
        return remaining;
    }
    return NoParse;
}

const char *Statements(const char *text, struct Block *block)
{
    struct Statement statement;
    const char      *remaining;

    block->len        = 0;
    block->statements = NULL;
    if ((remaining = Statement(text, &statement))) {
        do {
            append(block->statements, block->len, &statement);
            text = remaining;
        } while ((remaining = Statement(text, &statement)));
        return text;
    }

    return NoParse;
}

const char *Subroutine(const char *text, struct Subroutine *outSub)
{
    memset(outSub, 0, sizeof *outSub);

    if ((text = consumeToken(text, "sub", false))) {
        const char *remaining;
        if ((remaining = consumeToken(text, "<-", false))) {
            text = Parameters(remaining, &outSub->input);
            if (!text) {
                // syntax_error
                return NoParse;
            }
        }
        if ((remaining = consumeToken(text, "->", false))) {
            text = Parameters(remaining, &outSub->output);
            if (!text) {
                // syntax_error
                return NoParse;
            }
        }
        return text;
    }

    return NoParse;
}

const char *Subscript(const char *text, struct Numerical *outNumerical)
{
    if ((text = consume(text, '_'))) {
        if ((text = Numerical(text, outNumerical))) {
            return text;
        }
    }
    return NoParse;
}

const char *TextLiteral(const char *text, struct String *outText)
{
    if (*text == '"') {
        const char *ch = &text[1];
        while (*ch != '"') {
            if (*ch == '\\') {
                ch++;
                if (!(*ch == '"' || *ch == '\\' || *ch == 'n' || *ch == 'r' || *ch == 't')) {
                    return NoParse;
                }
            }
            ch++;
        }
        outText->text = text + 1;
        outText->len  = ch - outText->text;
        return Whitespace(ch + 1);
    }
    return NoParse;
}

const char *Tuple(const char *text, struct Arguments *outTuple)
{
    if (*text == '(') {
        return Arguments(text, outTuple);
    }
    return NoParse;
}

const char *Type(const char *text, struct Type *outType)
{
    if ((text = consume(text, ':'))) {
        const char *remaining;
        if ((remaining = Subroutine(text, &outType->Subroutine))) {
            outType->type = TYPE_SUBROUTINE;
            return remaining;
        }
        if ((remaining = Pointer(text, &outType->Pointer))) {
            outType->type = TYPE_POINTER;
            return remaining;
        }
        if ((remaining = Array(text, &outType->Array))) {
            outType->type = TYPE_ARRAY;
            return remaining;
        }
        if ((remaining = Identifier(text, &outType->Identifier))) {
            outType->type = TYPE_IDENT;
            return remaining;
        }
    }
    return NoParse;
}

const char *Value(const char *text, struct Value *outValue)
{
    const char *remaining;
    if ((remaining = Number(text, &outValue->Number))) {
        outValue->type = VAL_NUMBER;
        return remaining;
    }
    if ((remaining = TextLiteral(text, &outValue->Text))) {
        outValue->type = VAL_TEXT;
        return remaining;
    }
    if ((remaining = CharLiteral(text, &outValue->Char))) {
        outValue->type = VAL_CHAR;
        return remaining;
    }
    if ((remaining = Subroutine(text, &outValue->Subroutine))) {
        if ((remaining = Block(remaining, &outValue->Subroutine.block))) {
            outValue->type = VAL_SUB;
            return remaining;
        }
    }
    if ((remaining = Call(text, &outValue->Call))) {
        outValue->type = VAL_CALL;
        return remaining;
    }
    if ((remaining = IdentPhrase(text, &outValue->IdentPhrase))) {
        outValue->type = VAL_IDENT;
        return remaining;
    }
    if ((remaining = Tuple(text, &outValue->Tuple))) {
        outValue->type = VAL_TUPLE;
        return remaining;
    }
    if ((remaining = Group(text, &outValue->Group))) {
        outValue->type = VAL_GROUPTYPE;
        return remaining;
    }
    if ((remaining = Type(text, &outValue->Type))) {
        outValue->type = VAL_TYPE;
        return remaining;
    }
    return NoParse;
}

const char *Variable(const char *text, struct Variable *outVar)
{
    if ((text = consumeToken(text, "var", true))) {
        if ((text = Parameters(text, &outVar->Parameters))) {
            // TODO: We have VAR Parameters, so we need to return MULTIPLE declarations?
            return text;
        }
    }
    return NoParse;
}

const char *Whitespace(const char *text)
{
    const char *remaining;
    while ((remaining = Space(text)) || (remaining = Comment(text))) {
        text = remaining;
    }
    return text;
}

const char *badText(void) { return possibleBadText; }
