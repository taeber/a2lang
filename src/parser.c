#include "parser.h"
#include "grammar.h"

static const char *buf;
static FILE       *fp;

#define output(indent, fmt, ...)                  \
    do {                                          \
        for (unsigned i = 0; i < (indent)*4; i++) \
            fputc(' ', fp);                       \
        fprintf(fp, (fmt), __VA_ARGS__);          \
    } while (0)

static unsigned line(const char *text);

static void printArguments(struct Arguments *args, const char *prefix, unsigned indent);
static void printBlock(struct Block *block, unsigned indent);
static void printCall(struct Call *call, unsigned indent);
static void printParameters(struct Parameters *params, const char *prefix, unsigned indent);
static void printString(const struct String *str, const char *prefix, unsigned indent);
static void printNumerical(struct Numerical *num, const char *prefix, unsigned indent);
static void printSubroutine(struct Subroutine *subr, unsigned indent);

static unsigned line(const char *text)
{
    unsigned line = 1;
    for (const char *ch = buf; ch != text; ch++) {
        if (*ch == '\n') {
            line++;
        }
    }
    return line;
}

static void printString(const struct String *str, const char *prefix, unsigned indent)
{
    output(indent, "%s %.*s\n", prefix ? prefix : "String", str->len, str->text);
}

static void printNumerical(struct Numerical *num, const char *prefix, unsigned indent)
{
    switch (num->type) {
    case NUM_IDENT:
        printString(&num->Identifier.String, prefix, indent);
        break;
    case NUM_NUMBER:
        output(indent, "%s %d\n", prefix, num->Number);
        break;
    case NUM_NONE:
        break;
    }
}

static void printType(struct Type *type, unsigned indent)
{
    switch (type->type) {
    case TYPE_ARRAY:
        output(indent, "%s\n", "Array");
        printString(&type->Array.type.String, "Type", indent + 1);
        printNumerical(&type->Array.size, "Size", indent + 1);
        break;
    case TYPE_POINTER:
        output(indent, "%s\n", "Pointer");
        printString(&type->Pointer.String, "Type", indent + 1);
        break;
    case TYPE_SUBROUTINE:
        printSubroutine(&type->Subroutine, indent);
        break;
    case TYPE_IDENT:
        printString(&type->Identifier.String, "Type", indent);
        break;
    case TYPE_UNKNOWN:
        break;
    }
}

static void printIdentPhrase(struct IdentPhrase *phrase, unsigned indent)
{
    if (!phrase->subscript && !phrase->field) {
        // Plan old Identifier
        printString(&phrase->identifier.String, "Identifier", indent);
        return;
    }
    output(indent, "%s\n", "IdentPhrase");
    printString(&phrase->identifier.String, "Identifier", indent + 1);
    if (phrase->subscript) {
        printNumerical(phrase->subscript, "Index", indent + 1);
    }
    if (phrase->field) {
        printString(&phrase->field->String, "Field", indent + 1);
    }
}

static void printValue(struct Value *value, unsigned indent)
{
    switch (value->type) {
    case VAL_IDENT:
        printIdentPhrase(&value->IdentPhrase, indent);
        break;
    case VAL_NUMBER:
        output(indent, "Number %d\n", value->Number);
        break;
    case VAL_TEXT:
        printString(&value->Text, "Text", indent);
        break;
    case VAL_CHAR:
        output(indent, "Char %c\n", value->Char);
        break;
    case VAL_SUB:
        printSubroutine(&value->Subroutine, indent);
        break;
    case VAL_CALL:
        printCall(&value->Call, indent);
        break;
    case VAL_TUPLE:
        output(indent, "%s\n", "Tuple");
        printArguments(&value->Tuple, "Item", indent + 1);
        break;
    case VAL_GROUPTYPE:
        output(indent, "%s\n", "Group");
        printParameters(&value->Group, "Item", indent + 1);
        break;
    case VAL_TYPE:
        printType(&value->Type, indent);
        break;
    case VAL_UNKNOWN:
        break;
    }
}

static void printArguments(struct Arguments *args, const char *prefix, unsigned indent)
{
    for (unsigned i = 0; i < args->len; i++) {
        struct Argument *arg  = &args->arguments[i];
        struct String   *name = &arg->name.String;
        output(indent, "%s line=%d\n", prefix, line(arg->_text));
        if (name->len > 0) {
            printString(name, "Name", indent + 1);
        } else {
            output(indent + 1, "%s\n", "Name (none)");
        }
        printValue(&arg->value, indent + 1);
    }
}

static void printParameters(struct Parameters *params, const char *prefix, unsigned indent)
{
    for (unsigned p = 0; p < params->len; p++) {
        struct String *name = &params->parameters[p].name.String;
        output(indent, "%s line=%d\n", prefix, line(name->text));
        printString(name, "Name", indent + 1);
        printType(&params->parameters[p].type, indent + 1);
        printNumerical(&params->parameters[p].loc, "Location", indent + 1);
    }
}

static void printConditional(struct Conditional *cond, unsigned indent, const char *type)
{
    const char *ops[] = { "==", "<>", "<", "<=", ">", ">=" };
    output(indent, "%s line=%d\n", type, line(cond->_text));
    output(indent + 1, "%s\n", ops[cond->compare]);
    printValue(&cond->left, indent + 2);
    printValue(&cond->right, indent + 2);
    output(indent + 1, "%s\n", "Then");
    printBlock(&cond->then, indent + 2);
}

static void printCall(struct Call *call, unsigned indent)
{
    output(indent, "%s\n", "Call");
    printIdentPhrase(&call->ident, indent + 1);
    if (call->args.len > 0) {
        printArguments(&call->args, "Arg", indent + 1);
    } else {
        output(indent + 1, "%s\n", "Args (none)");
    }
}

static void printBlock(struct Block *block, unsigned indent)
{
    for (unsigned i = 0; i < block->len; i++) {
        struct Statement *s = &block->statements[i];
        switch (s->type) {
        case STMT_ASSEMBLY:
            output(indent, "Assembly line=%d {\n%.*s\n", line(s->Assembly.String.text), s->Assembly.String.len, s->Assembly.String.text);
            output(indent, "%c\n", '}');
            break;
        case STMT_ASSIGN:
            output(indent, "Set %c= line=%d\n", s->Assignment.kind, line(s->Assignment.ident.identifier.String.text));
            printIdentPhrase(&s->Assignment.ident, indent + 1);
            printValue(&s->Assignment.value, indent + 1);
            break;
        case STMT_CALL:
            printCall(&s->Call, indent);
            break;
        case STMT_DECLARATION:
            output(indent, "%s\n", "Declaration");
            printParameters(&s->Declaration.Parameters, "Use", indent + 1);
            break;
        case STMT_VARIABLE:
            output(indent, "%s\n", "Variable");
            printParameters(&s->Variable.Parameters, "Var", indent + 1);
            break;
        case STMT_DEFINITION:
            output(indent, "%s\n", "Definition");
            printArguments(&s->Definition.Arguments, "Let", indent + 1);
            break;
        case STMT_COND:
            printConditional(&s->Conditional, indent, "If");
            break;
        case STMT_LOOP:
            printConditional(&s->Conditional, indent, "While");
            break;
        case STMT_RETURN:
            output(indent, "%s\n", "Return");
            break;
        case STMT_STOP:
            output(indent, "%s\n", "Stop");
            break;
        case STMT_REPEAT:
            output(indent, "%s\n", "Repeat");
            break;
        case STMT_UNKNOWN:
            output(indent, "%s\n", "!!unknown statement type!!");
            break;
        }
    }
}

static void printSubroutine(struct Subroutine *subr, unsigned indent)
{
    output(indent, "%s\n", "Subroutine");
    printParameters(&subr->input, "<-", indent + 1);
    printParameters(&subr->output, "->", indent + 1);
    printBlock(&subr->block, indent + 1);
}

static void printProgram(struct Program *prog)
{
    output(0, "%s\n", "Program line=1");
    printBlock(&prog->block, 1);
}

const char *Parse(const char *text, struct Program *outProg, unsigned *outLine)
{
    buf = text;
    const char *result = Program(text, outProg);
    if (result == NoParse) {
        result = badText();
        if (result) {
            *outLine = line(result);
        }
    }
    return result;
}

void WriteAST(FILE *output, struct Program *prog)
{
    fp = output;
    printProgram(prog);
}
