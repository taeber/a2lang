#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "parser.h"

struct Symbol;

enum Register {
    REG_NONE = 0x00, // 0000_0000
    REG_A    = 0x01, // 0000_0001
    REG_X    = 0x02, // 0000_0010
    REG_Y    = 0x04, // 0000_0100
    REG_AX   = (REG_A << 4) | REG_X, // 0001_0010
    REG_AY   = (REG_A << 4) | REG_Y, // 0001_0010
    REG_XA   = (REG_X << 4) | REG_A, // 0010_0001
    REG_XY   = (REG_X << 4) | REG_Y, // 0010_0100
    REG_YA   = (REG_Y << 4) | REG_A, // 0100_0001
    REG_YX   = (REG_Y << 4) | REG_X, // 0100_0010
};

const char          *RegisterName(enum Register reg);
inline enum Register RegisterHigh(enum Register reg) { return (reg >> 4) & 0x0F; }
inline enum Register RegisterLow(enum Register reg) { return reg & 0x0F; }
inline uint8_t       RegisterSize(enum Register reg)
{
    if (reg > REG_Y) {
        return 2;
    }
    if (reg > REG_NONE) {
        return 1;
    }
    return 0;
}

enum LocationType {
    LOC_NONE = 0,
    LOC_FIXED,
    LOC_OFFSET,
    LOC_REGISTER,
};

struct Location {
    enum LocationType type;
    char             *addr;
    union {
        uint16_t      fixed;
        uint8_t       offset;
        enum Register reg;
    };
};

struct TypeInfo {
    char    *name;
    bool     isPointer;
    bool     isArray;
    uint16_t count;
};

void InitializeSymbols(void);

struct Symbol       *DeclareSubroutine(char *name, struct Location loc);
struct Symbol       *AddParameter(struct Symbol *sub, char *name, struct TypeInfo type, struct Location loc);
struct Symbol       *AddOutput(struct Symbol *sub, char *name, struct TypeInfo type, struct Location loc);
const struct Symbol *GetParameter(const struct Symbol *subsym, const struct String *name, uint16_t number);
const struct Symbol *GetOutput(const struct Symbol *subsym, const struct String *name, uint16_t number);

struct Symbol       *DeclareGroup(char *name);
struct Symbol       *AddMember(struct Symbol *group, char *name, struct TypeInfo type, struct Location loc);
const struct Symbol *GetMember(const struct Symbol *group, const struct String *name, uint16_t number);
const char          *GetMemberName(const struct Symbol *sym);
uint16_t             GetOffset(const struct Symbol *member);

struct Symbol *AddConstant(struct Symbol *sub, char *name, struct TypeInfo type, struct Location loc);
struct Symbol *AddVariable(struct Symbol *sub, char *name, struct TypeInfo type, struct Location loc);

// let littleo = `o
struct Symbol *DefineLiteralChar(char *name, char ch);
// let cNumItems = 5
struct Symbol *DefineLiteralNumber(char *name, uint16_t value);
// let msg = "Hi!"
struct Symbol *DefineLiteralText(char *label, char *text);

// let int = :byte
struct Symbol *AliasType(char *alias, const char *name);
// let text = :char^
struct Symbol *AliasPointer(char *alias, const char *name);
// let Point3D = :byte^3
struct Symbol *AliasArray(char *alias, const char *name, uint16_t length);

bool          IsCallable(const struct Symbol *sym);
bool          IsChar(const struct Symbol *sym);
bool          IsGroup(const struct Symbol *sym);
bool          IsLiteral(const struct Symbol *sym);
bool          IsPointer(const struct Symbol *sym);
bool          IsVariable(const struct Symbol *sym);
bool          IsWord(const struct Symbol *sym);
const char   *GetAddress(const struct Symbol *sym);
int32_t       GetItemCount(const struct Symbol *sym);
const char   *GetName(const struct Symbol *sym);
uint16_t      GetNumber(const struct Symbol *sym);
enum Register GetRegister(const struct Symbol *sym);
uint16_t      GetBaseSize(const struct Symbol *sym);
uint16_t      GetItemSize(const struct Symbol *sym);
uint16_t      GetSize(const struct Symbol *sym);
const char   *GetText(const struct Symbol *sym);
bool          HasLocation(const struct Symbol *sym);

struct Symbol *TryLookup(const char *name);
struct Symbol *Lookup(const char *name);
struct Symbol *LookupScoped(const struct String *scope, const struct String *name);
struct Symbol *LookupType(const char *name);

struct Symbol *TryLookupSubroutine(const struct String *name);
struct Symbol *LookupSubroutine(const struct String *name, uint16_t numParams);
enum Register  LookupRegister(const struct String *name);

char *MakeLabel(void);
char *MakeLocalLabel(const struct String *scope);

void DumpSymbols(FILE *fp);
