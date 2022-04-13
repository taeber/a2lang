#include "codegen.h"

#include <string.h>

#include "asm.h"
#include "io.h"
#include "symbols.h"
#include "text.h"

static struct {
    const struct String     *name;
    const struct Subroutine *subr;
} subroutine;

static void freeOperand(char *operand) { free(operand); }

static LoadOp  loads[]  = { freeOperand, [REG_A] = LDA, [REG_X] = LDX, [REG_Y] = LDY };
static StoreOp stores[] = { freeOperand, [REG_A] = STA, [REG_X] = STX, [REG_Y] = STY };

// WARNING: lookupFree free's key
static inline struct Symbol *lookupFree(char *key, bool isRequired)
{
    struct Symbol *sym = (isRequired ? Lookup : TryLookup)(key);
    free(key);
    return sym;
}

static char *qualify(const struct String *scope, const struct String *name)
{
    if (scope) {
        return stringf("%.*s.%.*s", scope->len, scope->text, name->len, name->text);
    }
    return string(name);
}

static struct Symbol *getsym(const struct String *name)
{
    struct Symbol *sym = lookupFree(qualify(subroutine.name, name), false);
    if (!sym) {
        sym = lookupFree(string(name), true);
    }
    return sym;
}

// parsed IP
struct IPInfo {
    const struct Symbol *ident;

    enum Register reg;

    bool  isPointer;
    char *addr;

    bool     isArray;
    uint16_t count;

    bool hasSubscript;
    struct {
        bool           var;
        uint16_t       num;
        struct Symbol *sym;
        enum Register  reg;
    } index;

    const struct Symbol *member;
};

static struct IPInfo IPInfo(const struct IdentPhrase *id)
{
    struct IPInfo info = { 0 };

    info.ident = getsym(&id->identifier.String);

    if (IsPointer(info.ident)) {
        // Pointer
        info.isPointer = true;
    }

    int32_t cnt  = GetCount(info.ident);
    info.isArray = cnt >= 0;
    if (info.isArray) {
        info.count = cnt;
    }

    if (id->subscript) {
        info.hasSubscript = true;
        switch (id->subscript->type) {
        case NUM_NUMBER:
            info.index.var = false;
            info.index.num = id->subscript->Number;
            info.index.sym = NULL;
            break;
        case NUM_IDENT:
            info.index.sym = getsym(&id->subscript->Identifier.String);
            if (IsVariable(info.index.sym)) {
                info.index.var = true;
            } else {
                info.index.var = false;
                info.index.num = GetNumber(info.index.sym);
            }
            info.index.reg = GetRegister(info.index.sym);
            break;
        case NUM_NONE:
            info.hasSubscript = false;
            break;
        }
    }

    if (id->field) {
        info.member = GetMember(info.ident, &id->field->String, 0);
    }

    if (!info.hasSubscript && !id->field) {
        info.reg = GetRegister(info.ident);
    }

    return info;
}

static inline struct Location inRegister(enum Register reg)
{
    return (struct Location) { .type = LOC_REGISTER, .reg = reg };
}

static struct Location generateLoadIdentPhrase(
    const struct IdentPhrase *id,
    enum Register             reserved,
    bool                      isAddress,
    LoadOp                    LDlo,
    LoadOp                    LDhi)
{
    struct IPInfo        info = IPInfo(id);
    const struct Symbol *sym  = info.ident;
    const char          *name = GetName(sym);
    if (info.hasSubscript) {
        if (info.isPointer) {
            if (info.index.var) {
                switch (GetRegister(info.index.sym)) {
                case REG_Y:
                    break;
                case REG_A:
                    TAY();
                case REG_X:
                    TAX();
                case REG_NONE:
                    LDY(strcopy(GetName(info.index.sym)));
                    break;
                default:
                    fatalf("unsupported register location for %s: %s",
                        GetName(info.index.sym),
                        RegisterName(GetRegister(info.index.sym)));
                }
            } else {
                LDY(immediate(hex2(info.index.num)));
            }
            LDA(indirectY(strcopy(name)));
            return inRegister(REG_A);
        }

        require(info.isArray, "%s is not an array and cannot be indexed", name);
        require(!info.index.var,
            "Arrays cannot use runtime indices; use a pointer: %s_%s", name, GetName(info.index.sym));
        if (info.index.num > info.count) {
            warnf("array index out of bounds for %s: %u > %d", name, info.index.num, info.count);
            REM(stringf("array index out of bounds for %s: %u > %d", name, info.index.num, info.count));
        }
        LDlo(offset(name, info.index.num));
        return inRegister(RegisterLow(reserved));
    }

    uint8_t off = 0;

    if (info.member) {
        require(!info.hasSubscript, "TODO: handle accessing array item's member");
        off = GetOffset(info.member);
    }

    if (isAddress) {
        LDlo(immediate(lo(offset(name, off))));
        LDhi(immediate(hi(offset(name, off))));
        return inRegister(reserved);
    }

    switch (GetSize(info.member ? info.member : sym)) {
    case 2:
        LDhi(offset(name, off+1));
        LDlo(offset(name, off));
        return inRegister(reserved);
    case 1:
        if (IsLiteral(sym)) {
            LDlo(immediate(offset(name, off)));
        } else {
            LDlo(offset(name, off));
        }
        return inRegister(RegisterLow(reserved));
    }
    fatalf("%s%s%s%s size must be <= 2: got %d",
        name,
        info.member ? "." : "",
        info.member ? GetMemberName(info.member) : "",
        GetSize(sym));
}

void copyMemory(const struct Symbol *dst, const struct Symbol *src, uint16_t size)
{
    const char *dstname = GetName(dst),
               *srcname = GetName(src);

    const uint16_t srcsize = GetSize(src);

    if (srcsize > size) {
        warnf("%s does not fit into %s (%d > %d); truncating", srcname, dstname, srcsize, size);
    }
    if (srcsize < size) {
        warnf("%s is smaller than %s (%d < %d)", srcname, dstname, srcsize, size);
        size = srcsize;
    }

    if (size > 0xFF) {
        warnf("TODO: handle copying more than 256 bytes in %s", __func__);
    }

    char *loop = MakeLabel();
    Label(loop);
    LDX(immediate(hex2(size)));
    LDA(absoluteX(srcname));
    STA(absoluteX(dstname));
    DEX();
    BNE(strcopy(loop));
    free(loop);
}

void declareParameters(struct Symbol *subsym, const struct Parameters *params)
{
    for (int i = 0; i < params->len; i++) {
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
    for (int i = 0; i < params->len; i++) {
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
    struct Symbol  *group = DeclareGroup(qualify(subroutine.name, name));
    struct Location loc   = { 0 };

    for (int i = 0; i < members->len; i++) {
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
    subroutine.name = name;
    subroutine.subr = subr;

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
    char *end = stringf("%s._end", subname);
    RTS(end);
    free(end);
    free(subname);

    subroutine.name = NULL;
    subroutine.subr = NULL;
}

const char *defineText(const struct String *label, const struct String *text)
{
    char *name = NULL;
    if (label) {
        name = qualify(subroutine.name, label);
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

void generateAdd(const struct IdentPhrase *lhs, const struct Value *value)
{
    struct IPInfo dst = IPInfo(lhs);
    if (value->type == VAL_NUMBER) {
        // Optimizations based on known values (len,time)
        if (dst.reg == REG_Y) {
            switch (value->Number) {
            case 0:
                return;
            case 2:
                INY();
                // falltrhu
            case 1:
                INY();
                return;
            case -2:
                DEY();
                // fallthru
            case -1:
                DEY();
                return;
            default:
                TYA();
                CLC();
                ADC(immediate(hex2(value->Number)));
                TAY();
                return;
            }
        }
        switch (value->Number) {
        case 0:
            warnf("optimization: removing unused code: %s += 0", GetName(dst.ident));
            return;
        case 2:
            INC(strcopy(GetName(dst.ident))); // 3,6
            // fallthru
        case 1:
            INC(strcopy(GetName(dst.ident))); // 3,6 = 6,12
            return;
        case -2:
            DEC(strcopy(GetName(dst.ident)));
            // fallthru
        case -1:
            DEC(strcopy(GetName(dst.ident)));
            return;
        default:
            LDA(strcopy(GetName(dst.ident))); // 3,4
            CLC(); // 1,2
            ADC(immediate(hex2(value->Number))); // 2,2
            STA(strcopy(GetName(dst.ident))); // 3,4 = 9,12
            return;
        }
    }
    REM(strcopy("TODO: finish STMT_ASSIGN for +="));
}

void generateAssembly(const struct Assembly *assembly)
{
    ASM(string(&assembly->String));
}

static void generateAssignValue2(const struct IPInfo *dst, const struct Value *value, const struct Symbol *member)
{
    // TODO: support register-based variables => just LD* or TAX,TAY...
    if (dst->reg == REG_NONE) {
        struct Location src = generateLoad(value, REG_XA, dst->isPointer && !dst->hasSubscript);
        // TODO: update GenerateStore with new IPInfo struct
        generateStore(GetName(dst->ident), NULL, member, src);
    } else {
        generateLoad(value, dst->reg, dst->isPointer && !dst->hasSubscript);
    }
}

void generateAssignValue(const struct IdentPhrase *lhs, const struct Value *value)
{
    struct IPInfo dst = IPInfo(lhs);
    if (value->type != VAL_TUPLE) {
        generateAssignValue2(&dst, value, NULL);
        return;
    }
    const struct Arguments *args = &value->Tuple;
    for (int i = 0; i < args->len; i++) {
        const struct Symbol *member = GetMember(dst.ident, &args->arguments->name.String, i);
        generateAssignValue2(&dst, &args->arguments[i].value, member);
    }
}

void generateBlock(const struct Block *block)
{
    for (int i = 0; i < block->len; i++) {
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
    for (int i = 0; i < call->args.len; i++) {
        arg               = &call->args.arguments[i];
        param             = GetParameter(subsym, &arg->name.String, i);
        enum Register reg = GetRegister(param);
        if (reg == REG_NONE) {
            struct Location src = generateLoad(&arg->value, REG_XA, IsPointer(param));
            generateStore(GetName(param), NULL, NULL, src);
        }
    }

    // Then process register-bound arguments.
    for (int i = 0; i < call->args.len; i++) {
        arg               = &call->args.arguments[i];
        param             = GetParameter(subsym, &arg->name.String, i);
        enum Register reg = GetRegister(param);
        if (reg != REG_NONE) {
            generateLoad(&arg->value, reg, IsPointer(param));
            // Just load; no need to store anything in memory!
        }
    }

    JSR(string(subname));
}

void requireSimpleValue(const struct Value *value)
{
    switch (value->type) {
    case VAL_NUMBER:
    case VAL_CHAR:
    case VAL_TEXT:
    case VAL_IDENT:
        return;

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

void generateConditional(const struct Conditional *cond, bool isLoop)
{
    static const char *_end = "_end",
                      *_then = "_then";

    requireSimpleValue(&cond->left);
    requireSimpleValue(&cond->right);

    // Example:
    //   ch == `A
    //   A2_0 LDA .ch
    //        CMP #'A'
    //        BEQ A2_0.then
    //        JMP A2_0.end
    //   A2_0.then: ...
    //   A2_0.end:
    //
    // Note: the targets of all branch operations are all RELATIVE. While it
    //  would be more efficient to inverse the branch:
    //        LDA .ch
    //        CMP #'A'
    //        BNE A2_0.end
    //   A2_0.then: ...
    //   A2_0.end
    //
    // we aren't keeping track of the size of the "then" block to see if it's
    // within 128 bytes. Therefore, it's safer to use the JMP .end variant.
    // However, this is potentionally something the optimizer could check for.

    char *label = MakeLabel();
    Label(label);

    struct Location loaded = generateLoad(&cond->left, REG_XA, false);
    // TODO: support more options
    require(loaded.type == LOC_REGISTER && loaded.reg == REG_A,
        "TODO: support more options like word comparisons in %s", __func__);

    if (cond->right.type == VAL_NUMBER) {
        if (cond->right.Number != 0) {
            // Optimization: no need to CMP #0 since it's implied by previous LD op
            CMP(immediate(hex2(cond->right.Number)));
        }
    } else if (cond->right.type == VAL_CHAR) {
        CMP(immediate(asciich(cond->right.Char)));
    } else {
        REM(strcopy("TODO: handle more condition types"));
        return;
    }

    switch (cond->compare) {
    case COMP_EQUAL:
        BEQ(stringf("%s.%s", label, _then));
        break;
    case COMP_LESSEQUAL:
        BEQ(stringf("%s.%s", label, _then));
        // fallthru
    case COMP_LESS:
        BCC(stringf("%s.%s", label, _then));
        break;
    case COMP_NOTEQUAL:
        BNE(stringf("%s.%s", label, _then));
        break;
    case COMP_GREATER:
        BNE(stringf("%s.%s", label, _then));
        // fallthru
    case COMP_GREATEREQUAL:
        BCS(stringf("%s.%s", label, _then));
        break;
    }
    JMP(stringf("%s.%s", label, _end));
    Label(stringf("%s.%s", label, _then));
    generateBlock(&cond->then);
    if (isLoop) {
        JMP(strcopy(label));
    }
    Label(stringf("%s.%s", label, _end));
    free(label);
}

void generateDeclaration(const struct Parameter *decl)
{
    const struct String *name = &decl->name.String;

    {
        struct Location loc = location(&decl->loc);
        switch (loc.type) {
        case LOC_FIXED: {
            char *label = qualify(subroutine.name, &decl->name.String);
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
        require(!subroutine.name,
            "cannot nest subroutines: %.*s in %.*s",
            name->len, name->text,
            subroutine.name->len, subroutine.name->text);
        declareSubroutine(name, &decl->type.Subroutine, &decl->loc);
        return;
    case TYPE_POINTER:
        // fallthru
    case TYPE_ARRAY:
        // fallthru
    case TYPE_IDENT:
        AddConstant(
            TryLookupSubroutine(subroutine.name),
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
    for (int i = 0; i < declaration->Parameters.len; i++) {
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
    for (int i = 0; i < definitions->Arguments.len; i++) {
        generateDefinition(&definitions->Arguments.arguments[i]);
    }
}

void generateLiteralChar(const struct String *name, char ch)
{
    struct Symbol *lit = DefineLiteralChar(qualify(subroutine.name, name), ch);
    EQU(GetName(lit), asciich(ch));
}

void generateLiteralNumber(const struct String *name, int number)
{
    struct Symbol *lit = DefineLiteralNumber(qualify(subroutine.name, name), number);
    if (GetSize(lit) == 2 || IsCallable(lit)) {
        EQU(GetName(lit), hex4(number));
        return;
    }
    require(GetSize(lit) == 1, "Unexpected size for %s; got %d", GetName(lit), GetSize(lit));
    EQU(GetName(lit), hex2(number));
}

struct Location generateLoad(const struct Value *value, enum Register reserved, bool isAddress)
{
    require(reserved != REG_NONE, "%s: must specify value for reserved", __func__);
    LoadOp LDlo = loads[RegisterLow(reserved)],
           LDhi = loads[RegisterHigh(reserved)];

    switch (value->type) {
    case VAL_NUMBER: {
        require(!isAddress, "cannot take address of literal: %d", value->Number);
        require(value->Number <= UINT16_MAX, "%s: number is too large: %d", __func__, value->Number);
        uint16_t num = value->Number;
        uint8_t lsb = (num & 0x00FF) >> 0,
                msb = (num & 0xFF00) >> 8;
        LDlo(immediate(hex2(lsb)));
        if (msb) {
            LDhi(immediate(hex2(msb)));
            return inRegister(reserved);
        }
        return inRegister(RegisterLow(reserved));
    }
    case VAL_CHAR:
        require(!isAddress, "cannot take address of literal: %c", value->Char);
        LDlo(immediate(asciich(value->Char)));
        return inRegister(RegisterLow(reserved));
    case VAL_TEXT: {
        const char *txt = defineText(NULL, &value->Text);
        LDlo(immediate(lo(txt)));
        LDhi(immediate(hi(txt)));
        return inRegister(reserved);
    }
    case VAL_CALL:
        break;
    case VAL_IDENT:
        return generateLoadIdentPhrase(&value->IdentPhrase, reserved, isAddress, LDlo, LDhi);
    case VAL_TUPLE:
        break;
    case VAL_GROUPTYPE:
    case VAL_TYPE:
    case VAL_SUB:
    case VAL_UNKNOWN:
        break;
    }
    fatalf("%s: unhandled value type: %d", __func__, value->type);
}

void generateStore(const char *ident, const struct Numerical *subscript, const struct Symbol *member, struct Location src)
{
    if (subscript) {
        fatalf("TODO: handle subscripts in %s", __func__);
    }

    uint16_t       off = GetOffset(member);
    const struct Symbol *sym = member ? member : Lookup(ident);

    switch (src.type) {
        case LOC_REGISTER: {
            require(src.reg != REG_NONE, "%s: must specify value for src.reg", __func__);
            StoreOp STlo = stores[RegisterLow(src.reg)],
                    SThi = stores[RegisterHigh(src.reg)];
            switch (GetSize(sym)) {
            case 2:
                SThi(offset(ident, off + 1));
                break;
            case 1:
                if (RegisterHigh(src.reg) != REG_NONE) {
                    warnf("truncation: %s is too small to store a word", ident);
                    REM(stringf("WARNING! REG %s IS UNUSED. VALUE TRUNCATED.", RegisterName(RegisterHigh(src.reg))));
                }
                break;
            default:
                fatalf("%s: size is too large for %s: %d", __func__, ident, GetSize(sym));
                break;
            }
            if (off > 0) {
                STlo(offset(ident, off));
            } else {
                STlo(strcopy(ident));
            }
        } break;
    case LOC_FIXED:
        REM("TODO: handle fixed location store");
        break;
    case LOC_NONE:
    case LOC_OFFSET:
    default:
        fatalf("%s: unsupported location type: %d", __func__, src.type);
    }
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
        switch (stmt->Assignment.kind) {
        case ':':
            generateAssignValue(&stmt->Assignment.ident, &stmt->Assignment.value);
            return;
        case '+':
            generateAdd(&stmt->Assignment.ident, &stmt->Assignment.value);
            return;
        default:
            REM(stringf("TODO: handle STMT_ASSIGN for %c=", stmt->Assignment.kind));
            return;
        }
    case STMT_COND:
        generateConditional(&stmt->Conditional, false);
        return;
    case STMT_LOOP:
        generateConditional(&stmt->Conditional, true);
        return;
    case STMT_RETURN:
        RTS(NULL);
        return;
    case STMT_STOP:
        REM(strcopy("TODO: handle STMT_STOP"));
        return;
    case STMT_REPEAT:
        REM(strcopy("TODO: handle STMT_REPEAT"));
        return;
    case STMT_ASSEMBLY:
        generateAssembly(&stmt->Assembly);
        return;
    case STMT_UNKNOWN:
        break;
    }
    fatalf("unknown statement type: %d", stmt->type);
}

void generateVariable(const struct Parameter *var)
{
    struct Symbol *sym = AddVariable(
        TryLookupSubroutine(subroutine.name),
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
    for (int i = 0; i < variable->Parameters.len; i++) {
        generateVariable(&variable->Parameters.parameters[i]);
    }
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

void setAddr(const char *dst, const char *src, enum Register reserved)
{
    enum Register reg = reserved != REG_NONE ? reserved : REG_A;
    loads[reg](immediate(lo(src)));
    stores[reg](offset(dst, 0));
    loads[reg](immediate(hi(src)));
    stores[reg](offset(dst, 1));
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

    WriteInstructions(fp);
}
