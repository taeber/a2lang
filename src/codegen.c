#include "codegen.h"

#include <string.h>

#include "asm.h"
#include "io.h"
#include "symbols.h"
#include "text.h"

struct Scope {
    const struct String *subr;
    const char          *loop;
    const char          *done;
    struct Scope        *prev;
};

struct Scope *Scope(const struct String *subr, const char *loop, const char *done, struct Scope *prev)
{
    require(subr || (loop && done), "missing arguments to Scope");
    struct Scope *scope = calloc(1, sizeof *scope);
    require(scope, "failed to allocate memory for Scope");
    scope->subr = subr;
    scope->loop = loop;
    scope->done = done;
    scope->prev = prev;
    return scope;
}

static struct Scope  global;
static struct Scope *scope = &global;

static inline void enterLoop(const char *loop, const char *done) { scope = Scope(NULL, loop, done, scope); }
static inline void enterSubroutine(const struct String *subr) { scope = Scope(subr, NULL, NULL, scope); }

static void leaveScope(void)
{
    require(scope != &global, "cannot leave global scope");
    struct Scope *old = scope;
    scope = old->prev;
    free(old);
}

static const struct String *subroutineName(void)
{
    for (struct Scope *p = scope; p && p != &global; p = p->prev) {
        if (p->subr) {
            return p->subr;
        }
    }
    return NULL;
}

static const struct Scope *getLoop(void)
{
    for (struct Scope *p = scope; p && p != &global; p = p->prev) {
        if (p->loop) {
            return p;
        }
    }
    return NULL;
}

static inline struct Symbol *getsym(const struct String *name)
{
    return LookupScoped(subroutineName(), name);
}

// Converts an IdentPhrase to an Operand.
static struct Operand *reduce(const struct IdentPhrase *id)
{
    struct Symbol *identsym = getsym(&id->identifier.String);

    enum Register reg = GetRegister(identsym);
    if (reg != REG_NONE) {
        const char *name = RegisterName(reg);
        if (RegisterSize(reg) == 2) {
            return OpRegisterWord(name[0], name[1]);
        }
        return OpRegister(name[0]);
    }

    if (!id->subscript && !id->field) {
        uint16_t size = GetSize(identsym);
        require(size <= 0xFF, "too big");
        if (IsLiteral(identsym)) {
            return OpImmediate(strcopy(GetName(identsym)), size);
        }
        return OpAbsolute(GetName(identsym), size);
    }

    if (id->subscript) {
        uint8_t size = GetBaseSize(identsym);
        if (IsPointer(identsym)) {
            // Pointers
            require(size == 1,
                "only byte pointers can be indexed: %s",
                phrase(id));
            return OpIndirectOffset(GetName(identsym), indextxt(id->subscript), size);
        }
        // Arrays
        int32_t itemCount = GetItemCount(identsym);
        require(itemCount > 0, "expected array");

        struct Symbol *indexsym = NULL;

        switch (id->subscript->type) {
        case NUM_IDENT:
            indexsym = getsym(&id->subscript->Identifier.String);
            require(!IsVariable(indexsym) || GetSize(indexsym) == 1,
                "variable index is not byte size: %s",
                string(&id->subscript->Identifier.String));
            // This is a workaround for a2asm's lack of support for
            // identifiers in arithmetic expressions.
            if (IsLiteral(indexsym)) {
                return OpOffset(GetName(identsym), hex4(GetNumber(indexsym)), false, size);
            }
            return OpOffset(GetName(identsym), strcopy(GetName(indexsym)), IsVariable(indexsym), size);
            // fallthrough
        case NUM_NUMBER:
            return OpOffset(GetName(identsym), numerical(id->subscript), false, size);

        case NUM_NONE:
        default:
            fatalf("%s: unsupported numerical type for index: %d",
                __func__,
                id->subscript->type);
        }
    }

    if (id->field) {
        // Member
        const struct Symbol *mem = GetMember(identsym, &id->field->String, 0);
        if (IsPointer(identsym)) {
            return OpIndirectOffset(GetName(identsym), stringf("#%u", GetOffset(mem)), GetSize(mem));
        }
        return OpOffset(GetName(identsym), stringf("%u", GetOffset(mem)), false, GetSize(mem));
    }

    fatalf("%s: failed to reduce phrase: %s", __func__, phrase(id));
    return NULL;
}

// Converts a SimpleValue to an Operand and returns its size.
static struct Operand *reduceSimpleValue(const struct Value *value)
{
    switch (value->type) {
    case VAL_IDENT:
        return reduce(&value->IdentPhrase);

    case VAL_NUMBER:
        return OpImmediateNumber(value->Number);

    case VAL_CHAR:
        return OpImmediate(asciich(value->Char), 1);

    case VAL_TEXT:
    case VAL_CALL:
    case VAL_TUPLE:
    case VAL_GROUPTYPE:
    case VAL_TYPE:
    case VAL_SUB:
        fatalf("%s: expected a simple value type; got %d", __func__, value->type);

    case VAL_UNKNOWN:
        break;
    }
    fatalf("%s: unhandled value type: %d", __func__, value->type);
}

// Convenience function to call generateSet using a String (param).
static void setArgument(const char *param, const struct Value *arg)
{
    struct IdentPhrase phrase;
    phrase.identifier.String.len  = strlen(param);
    phrase.identifier.String.text = param;
    phrase.field                  = NULL;
    phrase.subscript              = NULL;
    generateSet(&phrase, arg);
}

void declareParameters(struct Symbol *subsym, const struct Parameters *params)
{
    for (unsigned i = 0; i < params->len; i++) {
        struct Parameter *param = &params->parameters[i];
        struct Symbol    *sym   = AddParameter(
            subsym,
            string(&param->name.String),
            typeinfo(&param->type),
            location(&param->loc));

        if (!HasLocation(sym)) {
            VAR(GetName(sym), GetSize(sym));
            continue;
        }

        const char *addr = GetAddress(sym);
        if (addr) {
            EQU(GetName(sym), strcopy(addr));
        }
    }
}

void declareOutputs(struct Symbol *subsym, const struct Parameters *params)
{
    struct Parameter *param;
    struct Location   loc;
    struct Symbol    *p;
    for (unsigned i = 0; i < params->len; i++) {
        param = &params->parameters[i];
        loc   = location(&param->loc);
        p     = AddOutput(subsym, string(&param->name.String), typeinfo(&param->type), loc);
        switch (loc.type) {
        case LOC_NONE:
            VAR(GetName(p), GetSize(p));
            break;
        case LOC_FIXED:
            EQU(GetName(p), strcopy(loc.addr));
            break;
        case LOC_REGISTER:
            break;
        case LOC_OFFSET:
            fatalf("Outputs cannot have relative locations: %s", GetName(p));
            break;
        }
    }
}

void declareSubroutine(const struct String *name, const struct Subroutine *subr, const struct Numerical *loc)
{
    struct Symbol *subsym = DeclareSubroutine(string(name), location(loc));
    declareParameters(subsym, &subr->input);
    declareOutputs(subsym, &subr->output);
}

void defineGroup(const struct String *name, const struct Parameters *members)
{
    struct Symbol  *group = DeclareGroup(qualify(subroutineName(), name));
    struct Location loc   = { 0 };

    for (unsigned i = 0; i < members->len; i++) {
        struct Parameter *member = &members->parameters[i];

        loc = location(&member->loc);
        switch (loc.type) {
        case LOC_REGISTER:
            fatalf("Group member %s.%s can not be register-bound: %s",
                GetName(group),
                string(&member->name.String),
                RegisterName(loc.reg));
            break;
        case LOC_FIXED:
            require(strcmp(loc.addr, "$00") == 0,
                "0 is the only allowable offset for group member %s.%s; got %s",
                GetName(group),
                string(&member->name.String),
                loc.addr);
            loc.type = LOC_OFFSET;
            break;
        case LOC_OFFSET:
        case LOC_NONE:
            break;
        }

        AddMember(group, string(&member->name.String), typeinfo(&member->type), loc);
    }
}

void defineSubroutine(const struct String *name, const struct Subroutine *subr)
{
    enterSubroutine(name);

    char *subname = string(name);
    if (!TryLookup(subname)) {
        declareSubroutine(name, subr, NULL);
    } else {
        require(!HasLocation(Lookup(subname)),
            "cannot define a subroutine that has a declared location: %s",
            subname);
        REM(strcopy("TODO: warn if declaration differs from definition"));
    }
    Label(subname);
    generateBlock(&subr->block);
    RTS();
    free(subname);

    leaveScope();
}

const char *defineText(const struct String *label, const struct String *text)
{
    char *name = NULL;
    if (label) {
        name = qualify(subroutineName(), label);
    }
    struct Symbol *sym     = DefineLiteralText(name, string(text));
    const char    *outname = GetName(sym);
    TXT(outname, GetText(sym));
    return outname;
}

void defineType(const struct String *name, const struct Type *type)
{
    char *typename;
    switch (type->type) {
    case TYPE_ARRAY:
        typename = string(&type->Array.type.String);
        AliasArray(string(name), typename, number(&type->Array.size));
        break;
    case TYPE_POINTER:
        typename = string(&type->Pointer.String);
        AliasPointer(string(name), typename);
        break;
    case TYPE_IDENT:
        typename = string(&type->Identifier.String);
        AliasType(string(name), typename);
        break;
    case TYPE_SUBROUTINE:
    case TYPE_UNKNOWN:
    default:
        fatalf("%s: unhandled kind of type: %d", __func__, type->type);
        return;
    }
    if (typename) {
        free(typename);
    }
}

void generateArithmetic(const struct IdentPhrase *lhs, const struct Value *rhs, char kind)
{
    struct Operand *dst = reduce(lhs),
                   *src = NULL;

    switch (rhs->type) {
    case VAL_IDENT:
        src = reduce(&rhs->IdentPhrase);
        break;

    case VAL_CHAR:
        src = OpImmediate(asciich(rhs->Char), 1);
        break;

    case VAL_NUMBER:
        src = OpImmediateNumber(rhs->Number);
        break;

    case VAL_CALL: {
        generateCall(&rhs->Call);
        if (!lhs->field && !lhs->subscript) {
            struct Symbol *sym = getsym(&lhs->identifier.String);
            require(!IsGroup(sym), "TODO: handle multiple outputs");
        }
        const struct Symbol *subsym   = LookupSubroutine(&rhs->Call.ident.identifier.String, rhs->Call.args.len);
        const struct Symbol *output   = GetOutput(subsym, NULL, 0);
        struct IdentPhrase   phrase   = { 0 };
        phrase.identifier.String.text = GetName(output);
        phrase.identifier.String.len  = strlen(GetName(output));
        src = reduce(&phrase);
    } break;

    case VAL_TUPLE:
    case VAL_TEXT:
    case VAL_GROUPTYPE:
    case VAL_TYPE:
    case VAL_SUB:
    case VAL_UNKNOWN:
        fatalf("%s: unexpected value type: %d", __func__, rhs->type);
    default:
        fatalf("%s: unknown value type: %d", __func__, rhs->type);
        break;
    }

    if (kind == '+') {
        PLUS(dst, src);
    } else if (kind == '-') {
        LESS(dst, src);
    } else if (kind == '&') {
        BITAND(dst, src);
    } else if (kind == '|') {
        OR(dst, src);
    } else if (kind == '^') {
        XOR(dst, src);
    } else if (kind == '!') {
        NOT(dst, src);
    } else {
        fatalf("%s: unexpected kind: %c=", __func__, kind);
    }

    FreeOperand(src);
    FreeOperand(dst);
}

void generateAssembly(const struct Assembly *assembly)
{
    ASM(string(&assembly->String));
}

void generateAssignment(const struct Assignment *assign)
{
    switch (assign->kind) {
    case ':':
        generateSet(&assign->ident, &assign->value);
        return;
    case '+':
    case '-':
    case '&': // AND
    case '|': // ORA
    case '^': // EOR
    case '!': // (bitwise not) EOR #%11111111
        generateArithmetic(&assign->ident, &assign->value, assign->kind);
        return;
    }

    fatalf("%s: unhandled assignment type: %c=", __func__, assign->kind);
}

void generateBlock(const struct Block *block)
{
    for (unsigned i = 0; i < block->len; i++) {
        generateStatement(&block->statements[i]);
    }
}

void generateCall(const struct Call *call)
{
    if (call->ident.subscript || call->ident.field) {
        fatalf("TODO: handle subscripts and fields in calls");
    }

    const struct String *subname = &call->ident.identifier.String;
    struct Symbol       *subsym  = LookupSubroutine(subname, call->args.len);

    const struct Symbol   *param;
    const struct Argument *arg;

    // Process non–register-bound arguments first.
    for (unsigned i = 0; i < call->args.len; i++) {
        arg               = &call->args.arguments[i];
        param             = GetParameter(subsym, &arg->name.String, i);
        enum Register reg = GetRegister(param);
        if (reg == REG_NONE) {
            setArgument(GetName(param), &arg->value);
        }
    }

    // Then process register-bound arguments.
    for (unsigned i = 0; i < call->args.len; i++) {
        arg               = &call->args.arguments[i];
        param             = GetParameter(subsym, &arg->name.String, i);
        enum Register reg = GetRegister(param);
        if (reg != REG_NONE) {
            setArgument(GetName(param), &arg->value);
        }
    }

    JSR(string(subname));
}

void generateConditional(const struct Conditional *cond, bool isLoop)
{
    static const COND IFxx[] = {
        [COMP_EQUAL]        = IFEQ,
        [COMP_NOTEQUAL]     = IFNE,
        [COMP_LESS]         = IFLT,
        [COMP_LESSEQUAL]    = IFLE,
        [COMP_GREATER]      = IFGT,
        [COMP_GREATEREQUAL] = IFGE,
        [COMP_ALWAYS]       = IFTT,
    };

    char *lblLoop = UnusedLabel();
    if (!lblLoop) {
        lblLoop = MakeLocalLabel(subroutineName());
        Label(lblLoop);
    }

    char *lblThen = MakeLocalLabel(subroutineName()),
         *lblDone = MakeLocalLabel(subroutineName());

    if (isLoop) {
        enterLoop(lblLoop, lblDone);
    }

    struct Operand *left  = reduceSimpleValue(&cond->left),
                   *right = reduceSimpleValue(&cond->right);

    IFxx[cond->compare](left, right, lblThen, lblDone);

    Label(lblThen);
    generateBlock(&cond->then);

    if (isLoop) {
        JMP(strcopy(lblLoop));
    }

    Label(lblDone);

    if (isLoop) {
        leaveScope();
    }

    free(lblDone);
    free(lblThen);
    free(lblLoop);
    FreeOperand(right);
    FreeOperand(left);
}

void generateDeclaration(const struct Parameter *decl)
{
    const struct String *name = &decl->name.String;

    {
        struct Location loc = location(&decl->loc);
        switch (loc.type) {
        case LOC_FIXED: {
            char *label = qualify(subroutineName(), &decl->name.String);
            EQU(label, strcopy(loc.addr));
            free(label);
            break;
        }
        case LOC_NONE:
        case LOC_REGISTER:
            break;
        case LOC_OFFSET:
            fatalf("unhandled location type for %s: %d", string(name), loc.type);
        }
        if (loc.addr) {
            free(loc.addr);
        }
    }

    switch (decl->type.type) {
    case TYPE_SUBROUTINE:
        require(!subroutineName(),
            "cannot nest subroutines: %.*s in %.*s",
            name->len, name->text,
            subroutineName()->len, subroutineName()->text);
        declareSubroutine(name, &decl->type.Subroutine, &decl->loc);
        return;
    case TYPE_POINTER:
        // fallthru
    case TYPE_ARRAY:
        // fallthru
    case TYPE_IDENT:
        AddConstant(
            TryLookupSubroutine(subroutineName()),
            string(&decl->name.String),
            typeinfo(&decl->type),
            location(&decl->loc));
        return;
    case TYPE_UNKNOWN:
        break;
    }
    fatalf("unhandled declaration type: %d", decl->type.type);
}

void generateDeclarations(const struct Declaration *declaration)
{
    for (unsigned i = 0; i < declaration->Parameters.len; i++) {
        generateDeclaration(&declaration->Parameters.parameters[i]);
    }
}

void generateDefinition(const struct Argument *def)
{
    const struct String *name = &def->name.String;

    switch (def->value.type) {
    case VAL_NUMBER:
        generateLiteralNumber(name, def->value.Number);
        return;
    case VAL_TEXT:
        defineText(name, &def->value.Text);
        return;
    case VAL_CHAR:
        generateLiteralChar(name, def->value.Char);
        return;
    case VAL_SUB:
        defineSubroutine(name, &def->value.Subroutine);
        return;
    case VAL_CALL:
        REM(strcopy("TODO: handle VAL_CALL"));
        return;
    case VAL_IDENT:
        REM(strcopy("TODO: handle VAL_IDENT"));
        return;
    case VAL_TUPLE:
        REM(strcopy("TODO: handle VAL_TUPLE"));
        return;
    case VAL_GROUPTYPE:
        defineGroup(name, &def->value.Group);
        return;
    case VAL_TYPE:
        defineType(name, &def->value.Type);
        return;
    case VAL_UNKNOWN:
        break;
    }
    fatalf("unknown value type: %d", def->value.type);
}

void generateDefinitions(const struct Definition *definitions)
{
    for (unsigned i = 0; i < definitions->Arguments.len; i++) {
        generateDefinition(&definitions->Arguments.arguments[i]);
    }
}

void generateLiteralChar(const struct String *name, char ch)
{
    struct Symbol *lit = DefineLiteralChar(qualify(subroutineName(), name), ch);
    EQU(GetName(lit), asciich(ch));
}

void generateLiteralNumber(const struct String *name, int number)
{
    struct Symbol *lit = DefineLiteralNumber(qualify(subroutineName(), name), number);
    if (GetSize(lit) == 2 || IsCallable(lit)) {
        EQU(GetName(lit), hex4(number));
        return;
    }
    require(GetSize(lit) == 1, "Unexpected size for %s; got %d", GetName(lit), GetSize(lit));
    EQU(GetName(lit), hex2(number));
}

static void generatePoint(const char *pointer, const struct Value *rhs)
{
    struct Operand *src = NULL;
    switch (rhs->type) {
    case VAL_TEXT:
        src = OpAbsolute(defineText(NULL, &rhs->Text), 2);
        break;

    case VAL_CALL: {
        generateCall(&rhs->Call);
        const struct Symbol *subsym = LookupSubroutine(&rhs->Call.ident.identifier.String, 0);
        const struct Symbol *output = GetOutput(subsym, NULL, 0);
        require(GetRegister(output) == REG_NONE,
            "cannot take address of register: %s", GetName(output));
        src = OpAbsolute(GetName(output), 2);
    } break;

    case VAL_IDENT:
        src = reduce(&rhs->IdentPhrase);
        break;

    case VAL_CHAR:
        fatalf("cannot take address of literal character: %c", rhs->Char);
    case VAL_NUMBER:
        fatalf("cannot take address of literal number: %d", rhs->Number);
    case VAL_TUPLE:
        fatalf("cannot take address of tuple");
    case VAL_GROUPTYPE:
    case VAL_TYPE:
    case VAL_SUB:
    case VAL_UNKNOWN:
        fatalf("%s: unexpected value type: %d", __func__, rhs->type);
    default:
        fatalf("%s: unknown value type: %d", __func__, rhs->type);
    }

    ADDR(pointer, src);

    FreeOperand(src);
}

void generateRepeat(void)
{
    const struct Scope *loop = getLoop();
    require(loop, "cannot call repeat outside of a loop");
    REM(strcopy("REPEAT"));
    JMP(strcopy(loop->loop));
}


static inline bool isPhrasePointer(const struct IdentPhrase *id)
{
    return (!id || id->field || id->subscript)
        ? false
        : IsPointer(getsym(&id->identifier.String));
}

void generateSet(const struct IdentPhrase *lhs, const struct Value *rhs)
{
    bool isSrcPointer = rhs->type == VAL_IDENT && isPhrasePointer(&rhs->IdentPhrase);

    if (isPhrasePointer(lhs)) {
        struct Symbol *dst = getsym(&lhs->identifier.String);
        if (!isSrcPointer) {
            // ptr := nonPTR
            generatePoint(GetName(dst), rhs);
            return;
        }
        struct Symbol *src = getsym(&rhs->IdentPhrase.identifier.String);
        // ptr := ptr
        if (strcmp(GetAddress(dst), GetAddress(src)) == 0) {
            warnf("optimized out assigning pointer to itself: %s := %s",
                GetName(dst), GetName(src));
            return;
        }
    }

    struct Operand *dst = reduce(lhs),
                   *src = NULL;

    // Special handling
    switch (rhs->type) {
    case VAL_IDENT:
        src = reduce(&rhs->IdentPhrase);
        break;

    case VAL_CHAR:
        src = OpImmediate(asciich(rhs->Char), 1);
        break;

    case VAL_NUMBER:
        src = OpImmediateNumber(rhs->Number);
        break;

    case VAL_CALL: {
        generateCall(&rhs->Call);
        if (!lhs->field && !lhs->subscript) {
            struct Symbol *sym = getsym(&lhs->identifier.String);
            require(!IsGroup(sym), "TODO: handle multiple outputs");
        }
        const struct Symbol *subsym = LookupSubroutine(&rhs->Call.ident.identifier.String, 0);
        const struct Symbol *output = GetOutput(subsym, NULL, 0);
        struct IdentPhrase phrase = {0};
        phrase.identifier.String.text = GetName(output);
        phrase.identifier.String.len  = strlen(GetName(output));
        src = reduce(&phrase);
    } break;

    case VAL_TEXT:
    case VAL_TUPLE:
        fatalf("TODO: handle text and tuple assignments");

    case VAL_GROUPTYPE:
    case VAL_TYPE:
    case VAL_SUB:
    case VAL_UNKNOWN:
        fatalf("%s: unexpected value type: %d", __func__, rhs->type);
    default:
        fatalf("%s: unknown value type: %d", __func__, rhs->type);
        break;
    }

    COPY(dst, src);

    FreeOperand(src);
    FreeOperand(dst);
}

void generateStatement(const struct Statement *stmt)
{
    switch (stmt->type) {
    case STMT_DECLARATION:
        generateDeclarations(&stmt->Declaration);
        return;
    case STMT_VARIABLE:
        generateVariables(&stmt->Variable);
        return;
    case STMT_DEFINITION:
        generateDefinitions(&stmt->Definition);
        return;
    case STMT_CALL:
        generateCall(&stmt->Call);
        return;
    case STMT_ASSIGN:
        generateAssignment(&stmt->Assignment);
        return;
    case STMT_COND:
        generateConditional(&stmt->Conditional, false);
        return;
    case STMT_LOOP:
        generateConditional(&stmt->Conditional, true);
        return;
    case STMT_RETURN:
        RTS();
        return;
    case STMT_STOP:
        generateStop();
        return;
    case STMT_REPEAT:
        generateRepeat();
        return;
    case STMT_ASSEMBLY:
        generateAssembly(&stmt->Assembly);
        return;
    case STMT_UNKNOWN:
        break;
    }
    fatalf("unknown statement type: %d", stmt->type);
}

void generateStop(void)
{
    const struct Scope *loop = getLoop();
    require(loop, "cannot call stop outside of a loop");
    REM(strcopy("STOP"));
    JMP(strcopy(loop->done));
}

void generateVariable(const struct Parameter *var)
{
    struct Symbol *sym = AddVariable(
        TryLookupSubroutine(subroutineName()),
        string(&var->name.String),
        typeinfo(&var->type),
        location(&var->loc));

    if (!HasLocation(sym)) {
        require(GetSize(sym) > 0, "Variable size cannot be 0: %*s", var->name.String.len, var->name.String.text);
        VAR(GetName(sym), GetSize(sym));
        return;
    }

    const char *addr = GetAddress(sym);
    if (addr) {
        EQU(GetName(sym), strcopy(addr));
        return;
    }
}

void generateVariables(const struct Variable *variable)
{
    for (unsigned i = 0; i < variable->Parameters.len; i++) {
        generateVariable(&variable->Parameters.parameters[i]);
    }
}

char *indextxt(const struct Numerical *num)
{
    switch (num->type) {
    case NUM_IDENT: {
        struct Symbol *sym = getsym(&num->Identifier.String);
        enum Register reg = GetRegister(sym);
        if (reg != REG_NONE) {
            return stringf("@%s", RegisterName(reg));
        }
        char *name = strcopy(GetName(sym));
        if (IsLiteral(sym)) {
            return immediate(name);
        }
        return name;
    }
    case NUM_NUMBER:
        require(num->Number <= 0xFF && num->Number >= -128, "bad byte offset: %d", num->Number);
        return immediate(hex2((uint8_t)num->Number));
    case NUM_NONE:
        break;
    }
    fatalf("%s: unhandled numerical type: %d", __func__, num->type);
}

struct Location location(const struct Numerical *num)
{
    struct Location loc = { .type = LOC_NONE };
    if (!num) {
        return loc;
    }
    switch (num->type) {
    case NUM_NONE:
        return loc;
    case NUM_NUMBER:
        loc.type  = LOC_FIXED;
        loc.fixed = num->Number;
        loc.addr  = hex4(num->Number);
        if (loc.fixed != num->Number) {
            warnf("location may be invalid: %d => %u", num->Number, loc.fixed);
        }
        return loc;
    case NUM_IDENT:
        loc.reg = LookupRegister(&num->Identifier.String);
        if (loc.reg != REG_NONE) {
            loc.type = LOC_REGISTER;
        } else {
            loc.type = LOC_FIXED;
            loc.addr = string(&num->Identifier.String);
        }
        return loc;
    }
    fatalf("%s: unhandled Numerical type: %d", __func__, num->type);
}

uint16_t number(const struct Numerical *num)
{
    require(num, "%s: num is required", __func__);
    switch (num->type) {
    case NUM_NUMBER:
        return (uint16_t)num->Number;
    case NUM_IDENT:
        return GetNumber(getsym(&num->Identifier.String));
    case NUM_NONE:
        break;
    }
    fatalf("%s: unhandled Numerical type: %d", __func__, num->type);
}

struct TypeInfo typeinfo(const struct Type *type)
{
    struct TypeInfo info = { 0 };
    switch (type->type) {
    case TYPE_ARRAY:
        info.name    = string(&type->Array.type.String);
        info.isArray = true;
        info.count   = number(&type->Array.size);
        return info;
    case TYPE_POINTER:
        info.name      = string(&type->Pointer.String);
        info.isPointer = true;
        return info;
    case TYPE_IDENT:
        info.name = string(&type->Identifier.String);
        return info;
    case TYPE_UNKNOWN:
    case TYPE_SUBROUTINE:
        break;
    }
    fatalf("%s: unhandled TypeInfo type: %d", __func__, type->type);
}

void Generate(FILE *fp, struct Program *program)
{
    if (!program) {
        return;
    }

    InitializeSymbols();

    generateBlock(&program->block);

    Optimize();

    WriteInstructions(fp);
}
