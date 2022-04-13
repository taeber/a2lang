#include "symbols.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "io.h"
#include "text.h"

enum LiteralType {
    LIT_NONE = 0,
    LIT_NUM,
    LIT_CHAR,
    LIT_TEXT,
};

enum ParamType {
    PARAM_NO = 0,
    PARAM_IN,
    PARAM_OUT,
};

struct Symbol {
    // Symbol "Table" fields
    struct Symbol *next;

    // Actual data fields
    char *name;

    struct Symbol *type;
    uint16_t       size;

    struct Location loc;

    bool isType;
    bool isPointer;
    bool isArray;

    uint16_t count;

    bool           isGroup;
    struct Symbol *members;
    struct Symbol *group;

    bool           isCallable;
    struct Symbol *params, *outputs;

    enum ParamType paramType;
    struct Symbol *subroutine;

    const char *uqname; // Unqualified name like `len`, not `Print.len`
    uint16_t    offset;

    enum LiteralType literal;
    union {
        uint16_t number;
        char     character;
        char    *text;
    };

    bool isVariable;
} symbols;

static struct Symbol *bytetype, *chartype, *wordtype;

static const char *registerNames[REG_Y + 1][REG_Y + 1] = {
    [REG_NONE] = { [REG_NONE] = "", [REG_A] = "A", [REG_X] = "X", [REG_Y] = "Y" },
    [REG_A]    = { [REG_NONE] = "A", [REG_A] = "", [REG_X] = "AX", [REG_Y] = "AY" },
    [REG_X]    = { [REG_NONE] = "X", [REG_A] = "XA", [REG_X] = "", [REG_Y] = "XY" },
    [REG_Y]    = { [REG_NONE] = "Y", [REG_A] = "YA", [REG_X] = "YX", [REG_Y] = "" },
};

// Creates a Symbol object and prepends it to the global symbols list.
// WARNING: ownership of name is the new object; the caller really shouldn't use
// name after calling Symbol.
static struct Symbol *Symbol(char *name)
{
    require(!TryLookup(name), "name conflict: %s", name);
    struct Symbol *symbol = calloc(1, sizeof(*symbol));
    require(symbol, "%s: failed to allocate memory", __func__);
    symbol->name = name;
    symbol->next = symbols.next;
    symbols.next = symbol;
    return symbol;
}

static uint16_t memsize(const struct Symbol *symbol)
{
    if (symbol->loc.type == LOC_REGISTER) {
        return 0;
    }
    return GetSize(symbol);
}

static struct Symbol *addRegister(enum Register reg)
{
    struct Symbol *symbol = Symbol(strcopy(RegisterName(reg)));
    symbol->loc.type      = LOC_REGISTER;
    symbol->loc.reg       = reg;
    symbol->type          = RegisterHigh(reg) != REG_NONE ? wordtype : bytetype;
    return symbol;
}

static struct Symbol *addType(char *name, uint16_t size)
{
    struct Symbol *sym = Symbol(name);
    sym->size          = size;
    sym->isType        = true;
    return sym;
}

static struct Symbol *makeMember(
    struct Symbol *group, char *name, struct TypeInfo type,
    struct Location loc, enum ParamType paramType)
{
    const char *prefix = GetName(group);

    struct Symbol *sym = Symbol(stringf("%s.%s", prefix, name));
    sym->uqname        = &sym->name[strlen(prefix) + 1];
    sym->type          = LookupType(type.name);
    sym->loc           = loc;
    sym->isPointer     = type.isPointer;
    sym->isArray       = type.isArray;
    sym->count         = type.count;
    sym->isVariable    = true;

    sym->paramType = paramType;
    switch (paramType) {
    case PARAM_NO:
        sym->group = group;
        break;
    case PARAM_IN:
        sym->subroutine = group;
        sym->group      = group->params;
        break;
    case PARAM_OUT:
        sym->subroutine = group;
        sym->group      = group->outputs;
        break;
    }

    struct Symbol *member = sym->group;
    while (member->members) {
        member = member->members;
    }
    if (member != sym->group) {
        if (loc.type == LOC_OFFSET) {
            sym->offset = loc.offset;
            sym->loc.type = LOC_NONE;
        } else {
            sym->offset = sym->group->size;
        }
    }
    member->members = sym;

    if (sym->isPointer) {
        require(loc.type == LOC_FIXED,
            "%s: pointers must be in Zero Page",
            sym->name);
        require(loc.fixed < 0xFF,
            "%s: pointers must be in Zero Page (<= $FE); got %s",
            sym->name, loc.addr);
    }

    if (loc.type != LOC_OFFSET) {
        sym->group->size += memsize(sym);
    } else {
        unsigned msize = memsize(sym) + loc.offset;
        if (sym->group->size < msize) {
            sym->group->size = msize;
        }
    }

    sym->group->count++;

    if (type.name) {
        free(type.name), type.name = NULL;
    }

    free(name);
    return sym;
}

struct Symbol *AddConstant(struct Symbol *sub, char *name, struct TypeInfo type, struct Location loc)
{
    struct Symbol *sym;
    if (sub) {
        sym = Symbol(stringf("%s.%s", sub->name, name));
        free(name);
        sym->subroutine = sub;
        sym->uqname     = &sym->name[strlen(sub->name) + sizeof '.' + 1];
    } else {
        sym = Symbol(name);
    }

    sym->loc       = loc;
    sym->type      = LookupType(type.name);
    sym->isPointer = type.isPointer;
    sym->isArray   = type.isArray;
    sym->count     = type.count;
    sym->isGroup   = sym->type->isGroup;
    
    if (sym->isPointer) {
        require(loc.type == LOC_FIXED,
            "%s: pointers must be in Zero Page",
            sym->name);
        require(loc.fixed < 0xFF,
            "%s: pointers must be in Zero Page (<= $FE); got %s",
            sym->name, loc.addr);
    }

    if (type.name) {
        free(type.name), type.name = NULL;
    }

    return sym;
}

struct Symbol *AddOutput(struct Symbol *sub, char *name, struct TypeInfo type, struct Location loc)
{
    return makeMember(sub, name, type, loc, PARAM_OUT);
}

struct Symbol *AddMember(struct Symbol *group, char *name, struct TypeInfo type, struct Location loc)
{
    return makeMember(group, name, type, loc, PARAM_NO);
}

struct Symbol *AddParameter(struct Symbol *sub, char *name, struct TypeInfo type, struct Location loc)
{
    return makeMember(sub, name, type, loc, PARAM_IN);
}

struct Symbol *AddVariable(struct Symbol *sub, char *name, struct TypeInfo type, struct Location loc)
{
    struct Symbol *sym = AddConstant(sub, name, type, loc);
    sym->isVariable    = true;
    return sym;
}

struct Symbol *AliasArray(char *alias, const char *name, uint16_t length)
{
    struct Symbol *sym = AliasType(alias, name);
    sym->isArray       = true;
    sym->count         = length;
    sym->size *= sym->count;
    return sym;
}

struct Symbol *AliasPointer(char *alias, const char *name)
{
    struct Symbol *sym = AliasType(alias, name);
    sym->isPointer     = true;
    sym->size          = 2;
    return sym;
}

struct Symbol *AliasType(char *alias, const char *name)
{
    struct Symbol *sym;
    sym         = Symbol(alias);
    sym->isType = true;
    sym->type   = LookupType(name);
    sym->size   = GetSize(sym->type);
    return sym;
}

struct Symbol *DeclareGroup(char *name)
{
    struct Symbol *sym = Symbol(name);
    sym->isType        = true;
    sym->isGroup       = true;
    return sym;
}

struct Symbol *DeclareSubroutine(char *name, struct Location loc)
{
    struct Symbol *sym = Symbol(name);
    sym->loc           = loc;
    sym->isCallable    = true;
    sym->params        = DeclareGroup(stringf("%s.<-", name));
    sym->outputs       = DeclareGroup(stringf("%s.->", name));
    return sym;
}

struct Symbol *DefineLiteralChar(char *name, char ch)
{
    struct Symbol *sym = TryLookup(name);
    if (sym) {
        require(sym->literal == LIT_NONE && sym->character == 0,
            "Cannot redefine %s to %u", name, ch);
        free(name);
    } else {
        sym = Symbol(name);
    }
    sym->literal   = LIT_CHAR;
    sym->character = ch;
    sym->type      = chartype;
    return sym;
}

struct Symbol *DefineLiteralNumber(char *name, uint16_t value)
{
    struct Symbol *sym = TryLookup(name);
    if (sym) {
        require(sym->literal == LIT_NONE && sym->number == 0,
            "Cannot redefine %s to %u", name, value);
        if (value > 0xFF && GetSize(sym->type) == 1) {
            warnf("Literal %s will be truncated to declared size: %s", name, GetName(sym->type));
            value = (uint8_t)value;
        }
        free(name);
        // Handle `let SUBR = $1234`
        if (IsCallable(sym)) {
            sym->loc.type = LOC_FIXED;
            sym->loc.addr = hex4(value);
            return sym;
        }
    } else {
        sym = Symbol(name);
    }
    sym->literal  = LIT_NUM;
    sym->number   = value;
    sym->type     = value > 0xFF ? wordtype : bytetype;
    sym->loc.type = LOC_FIXED;
    sym->loc.addr = hex4(value);
    return sym;
}

struct Symbol *DefineLiteralText(char *name, char *text)
{
    struct Symbol *sym = NULL;
    if (name) {
        sym = TryLookup(name);
        if (sym) {
            require(sym->literal == LIT_NONE && sym->character == 0,
                "Cannot redefine %s to %s", name, text);
            free(name);
        }
    }
    if (!sym) {
        sym = Symbol(name ? name : MakeLabel());
    }
    sym->type    = chartype;
    sym->text    = text;
    sym->literal = LIT_TEXT;
    sym->isArray = true;
    sym->count   = strlen(text) + 1;
    return sym;
}

static void writeSymbolRow(
    FILE *fp, const struct Symbol *symbol, unsigned maxname, unsigned maxtype,
    unsigned maxloc, unsigned maxsizes, char *buf)
{
    // Name
    fprintf(fp, " %-*s  ", maxname, symbol->name);

    // Type
    char *out = buf;
    out += sprintf(out, "%c%s%c",
        symbol->isType ? ':' : ' ',
        symbol->isType && symbol->isGroup ? "[]" : GetName(symbol->type),
        symbol->isPointer || symbol->isArray ? '^' : ' ');
    if (symbol->isArray) {
        out += sprintf(out, "%-3d", symbol->count);
    }
    *out = '\0';
    fprintf(fp, "%-*s  ", maxtype, buf);

    // Loc
    switch (symbol->loc.type) {
    case LOC_NONE:
        fprintf(fp, "%-*s  ", maxloc, "");
        break;
    case LOC_REGISTER: {
        const char *reg = RegisterName(symbol->loc.reg);
        fprintf(fp, "@%s %-*s", reg, (int)(maxloc - strlen(reg)), "");
        break;
    }
    case LOC_FIXED:
        fprintf(fp, "%-*s  ", maxloc, symbol->loc.addr);
        break;
    case LOC_OFFSET:
        fprintf(fp, "+$%.2X   ", symbol->loc.offset);
        break;
    }

    // Callable
    fputs(symbol->isCallable ? "()" : "  ", fp);
    switch (symbol->paramType) {
    case PARAM_IN:
        fputs("<- ", fp);
        break;
    case PARAM_OUT:
        fputs(" ->", fp);
        break;
    case PARAM_NO:
        fputs("   ", fp);
        break;
    }
    fputs(" ", fp);

    // Sizes
    if (!symbol->isCallable) {
        const uint16_t size  = GetSize(symbol),
                       msize = memsize(symbol);
        // Size
        fprintf(fp, "  %3u", size);
        // Memsize
        if (size != msize) {
            fprintf(fp, "/%-3u ", msize);
        } else {
            fputs("     ", fp);
        }
        // Offset
        if (symbol->uqname) {
            fprintf(fp, "+%-3d", symbol->offset);
        } else {
            fputs("    ", fp);
        }
    } else {
        fprintf(fp, "%-*s", maxsizes, "");
    }
    fputs("  ", fp);

    // Value
    if (symbol->literal == LIT_CHAR) {
        fprintf(fp, "'%c'", symbol->character);
    }
    if (symbol->literal == LIT_NUM) {
        fprintf(fp, "$%X", symbol->number);
        if (symbol->number <= 0xFF) {
            fprintf(fp, " (%u)", symbol->number);
        }
    }
    if (symbol->literal == LIT_TEXT) {
        fprintf(fp, "\"%s\"", symbol->text);
    }
    if (symbol->isVariable) {
        fputs("var", fp);
    }

    fputc('\n', fp);
}

void DumpSymbols(FILE *fp)
{
    // Headers
    static const char *Name     = "Name",
                      *Type     = "Type",
                      *Location = "Loc",
                      *Callable = "()<=>",
                      *Sizes    = "Size/Mem +Off",
                      *Value    = "Value";

    unsigned maxname = strlen(Name);
    unsigned maxtype = strlen(Type);
    for (struct Symbol *p = symbols.next; p; p = p->next) {
        unsigned len = strlen(p->name);
        if (maxname < len) {
            maxname = len;
        }
        if (p->isType && maxtype < len) {
            maxtype = len;
        }
    }

    maxtype += strlen(":^255");

    const unsigned maxloc   = strlen("$0000");
    const unsigned maxsizes = strlen(Sizes);

    fputs("SYMBOL TABLE\n", fp);
    fprintf(fp, " %-*s  %-*s  %-*s  %s  %s  %s\n",
        maxname, Name,
        maxtype, Type,
        maxloc, Location,
        Callable,
        Sizes,
        Value);

    char buf[256];
    require(sizeof buf >= maxtype,
        "%s: buf size is too small (%lu > %d)", __func__, sizeof buf, maxtype);

    for (struct Symbol *p = symbols.next; p; p = p->next) {
        buf[0] = '\0';
        writeSymbolRow(fp, p, maxname, maxtype, maxloc, maxsizes, buf);
    }
}

const char *GetAddress(const struct Symbol *sym)
{
    if (sym && sym->loc.type == LOC_FIXED) {
        return sym->loc.addr;
    }
    return NULL;
}

int32_t GetCount(const struct Symbol *sym)
{
    if (sym && (sym->isArray || sym->isGroup)) {
        return sym->count;
    }
    return -1;
}

uint16_t GetItemSize(const struct Symbol *sym)
{
    if (sym->isArray) {
        warnf("%s is not an array; assuming byte^ with size 1", GetName(sym));
    }
    return GetSize(sym->type);
}

static const struct Symbol *getMember(
    const struct Symbol *group,
    const struct String *name,
    uint16_t             number)
{
    const struct Symbol *p = group->members;
    if (!name || name->len == 0) {
        for (uint16_t i = 0; i < number; i++) {
            p = p->members;
        }
        require(p, "group %s does not have %u members", group->name, number+1);
        return p;
    }
    // Name comparison
    for (; p; p = p->members) {
        const char *ch    = name->text,
                   *qname = p->uqname;
        for (int i = 0; i < name->len; i++) {
            if (!qname || *ch != *qname) {
                goto next;
            }
        }
        return p;
    next:;
    }
    fatalf("unknown member %s.%*s", group->name, name->len, name->text);
}

const struct Symbol *GetMember(const struct Symbol *group, const struct String *name, uint16_t number)
{
    return getMember(group->type, name, number);
}

const char *GetMemberName(const struct Symbol *sym) { return sym && sym->uqname ? sym->uqname : ""; }

const char *GetName(const struct Symbol *sym) { return sym ? sym->name : ""; }

uint16_t GetNumber(const struct Symbol *sym)
{
    require(sym && sym->literal == LIT_NUM, "%s is not a number", sym->name);
    return sym->number;
}

uint16_t GetOffset(const struct Symbol *sym) { return sym ? sym->offset : 0; }

const struct Symbol *GetOutput(const struct Symbol *subsym, const struct String *name, uint16_t number)
{
    return getMember(subsym->outputs, name, number);
}

const struct Symbol *GetParameter(const struct Symbol *subsym, const struct String *name, uint16_t number)
{
    return getMember(subsym->params, name, number);
}

enum Register GetRegister(const struct Symbol *sym)
{
    if (!sym || sym->loc.type != LOC_REGISTER) {
        return REG_NONE;
    }
    return sym->loc.reg;
}

uint16_t GetSize(const struct Symbol *sym)
{
    if (!sym) {
        return 0;
    }
    if (sym->isPointer) {
        return wordtype->size;
    }
    int size = sym->isType ? sym->size : GetSize(sym->type);
    if (!sym->isArray) {
        return size;
    }
    return size * sym->count;
}

const char *GetText(const struct Symbol *sym)
{
    require(sym && sym->literal == LIT_TEXT, "%s is not a text literal", GetName(sym));
    return sym->text;
}

bool HasLocation(const struct Symbol *sym) { return sym && sym->loc.type != LOC_NONE; }

void InitializeSymbols(void)
{
    // Fundamental data types
    bytetype = addType(strcopy("byte"), 1);
    chartype = addType(strcopy("char"), 1);
    wordtype = addType(strcopy("word"), 2);

    // Useful, builtin type aliases
    AliasType(strcopy("int"), bytetype->name);
    AliasType(strcopy("addr"), wordtype->name);
    AliasPointer(strcopy("text"), chartype->name);

    // 6502 Registers
    addRegister(REG_A);
    addRegister(REG_X);
    addRegister(REG_Y);

    // 6502 Psuedo-registers
    addRegister(REG_AX);
    addRegister(REG_AY);
    addRegister(REG_XA);
    addRegister(REG_XY);
    addRegister(REG_YA);
    addRegister(REG_YX);
}

bool IsCallable(const struct Symbol *sym) { return sym && sym->isCallable; }

bool IsChar(const struct Symbol *sym)
{
    for (; sym; sym = sym->type) {
        if (sym == chartype) {
            return true;
        }
    }
    return false;
}

bool IsLiteral(const struct Symbol *sym) { return sym->literal != LIT_NONE; }

bool IsPointer(const struct Symbol *sym)
{
    for (; sym; sym = sym->type) {
        if (sym->isPointer) {
            return true;
        }
    }
    return false;
}

bool IsVariable(const struct Symbol *sym) { return sym->isVariable; }

bool IsWord(const struct Symbol *sym)
{
    for (; sym; sym = sym->type) {
        if (sym == wordtype) {
            return true;
        }
    }
    return false;
}

struct Symbol *Lookup(const char *name)
{
    struct Symbol *sym = TryLookup(name);
    require(sym, "unknown symbol: %s", name);
    return sym;
}

enum Register LookupRegister(const struct String *name)
{
    char          *key = string(name);
    struct Symbol *sym = Lookup(key);
    free(key);
    return GetRegister(sym);
}

struct Symbol *LookupSubroutine(const struct String *name, uint16_t numParams)
{
    char *n = string(name);

    struct Symbol *sym = Lookup(n);
    require(sym->isCallable, "%s is not a subroutine", n);
    require(numParams == 0 || numParams == sym->params->count,
        "%s does not have %d parameters", n, numParams);

    free(n);
    return sym;
}

struct Symbol *LookupType(const char *name)
{
    struct Symbol *sym = Lookup(name);
    require(sym->isType, "%s is not a type", name);
    return sym;
}

char *MakeLabel(void)
{
    static unsigned count = 0;
    return stringf("A2_%d", count++);
}

extern inline enum Register RegisterHigh(enum Register reg);
extern inline enum Register RegisterLow(enum Register reg);

const char *RegisterName(enum Register reg)
{
    return registerNames[RegisterHigh(reg)][RegisterLow(reg)];
}

struct Symbol *TryLookup(const char *name)
{
    for (struct Symbol *p = symbols.next; p; p = p->next) {
        if (strcmp(p->name, name) == 0) {
            return p;
        }
    }
    return NULL;
}

struct Symbol *TryLookupSubroutine(const struct String *subname)
{
    if (subname) {
        char          *name = string(subname);
        struct Symbol *sym  = TryLookup(name);
        free(name);
        if (sym && sym->isCallable) {
            return sym;
        }
    }
    return NULL;
}
