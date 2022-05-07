#include "asm.h"

#include "io.h"
#include "text.h"

enum Mode {
    MODE_IMMEDIATE,
    MODE_ABSOLUTE,
    MODE_OFFSET,
    MODE_VARIABLE_OFFSET,
    MODE_INDIRECT_OFFSET,
    MODE_REGISTER,
};

struct Operand {
    enum Mode   mode;
    uint8_t     size;
    const char *base;
    char       *offset;
    char       *immlo;
    char       *immhi;
    struct {
        bool     valid;
        uint16_t value;
    } number;
};

static struct Operand ZEROB = {
    .mode   = MODE_IMMEDIATE,
    .size   = 1,
    .immlo  = (char[]) { '0', '\0' },
    .immhi  = (char[]) { '0', '\0' },
    .number = {
        .valid = false,
    },
};

static struct Operand *Operand(
    enum Mode   mode,
    uint8_t     size,
    const char *base,
    char       *offset,
    char       *immlo,
    char       *immhi)
{
    struct Operand *operand = calloc(1, sizeof(*operand));
    require(operand, "calloc failed");
    operand->mode   = mode;
    operand->size   = size;
    operand->base   = base;
    operand->offset = offset;
    operand->immlo  = immlo;
    operand->immhi  = immhi;
    return operand;
}

static inline char regLow(const struct Operand *reg) { return reg->immlo[0]; }
static inline char regHigh(const struct Operand *reg) { return reg->immhi[0]; }

static char *operandString(const struct Operand *operand)
{
    if (operand) {
        switch (operand->mode) {
        case MODE_IMMEDIATE:
            if (operand->immhi) {
                return stringf("#%s,#%s", operand->immhi, operand->immlo);
            } else {
                return stringf("#%s", operand->immlo);
            }
        case MODE_ABSOLUTE:
            return stringf("%s", operand->base);
        case MODE_OFFSET:
            return stringf("%s+%s", operand->base, operand->offset);
        case MODE_VARIABLE_OFFSET:
            return stringf("%s,%s", operand->base, operand->offset);
        case MODE_INDIRECT_OFFSET:
            return stringf("(%s),%s", operand->base, operand->offset);
        case MODE_REGISTER:
            return stringf("@%s%s", operand->immhi ? operand->immhi : "", operand->immlo);
        }
    }
    return NULL;
}

static char *macroString(const char *macro, const struct Operand *dst, const struct Operand *src)
{
    char *lhs  = operandString(dst),
         *rhs  = operandString(src);
    char *repr = stringf("%s %s%s%s", macro, lhs, rhs ? " " : "", rhs ? rhs : "");
    free(lhs);
    if (rhs) {
        free(rhs);
    }
    return repr;
}

// Creates a new Operand representing the high byte of word.
static struct Operand *highByte(const struct Operand *word)
{
    switch (word->mode) {
    case MODE_IMMEDIATE:
        return OpImmediate(strcopy(word->immhi), word->size);

    case MODE_ABSOLUTE:
        return OpOffset(word->base, strcopy("1"), false, word->size);

    case MODE_OFFSET:
        return OpOffset(word->base, stringf("%s+1", word->offset), false, word->size);

    case MODE_VARIABLE_OFFSET:
        return OpOffset(word->base, stringf("%s+1", word->offset), true, word->size);

    case MODE_INDIRECT_OFFSET:
        require(word->offset[0] != '@',
            "register does not have a high byte: %s", word->offset);
        return OpIndirectOffset(word->base, stringf("%s+1", word->offset), word->size);

    case MODE_REGISTER:
        fatalf("%s: invalid mode type: %d", __func__, word->mode);
    }

    fatalf("%s: unhandled mode type: %d", __func__, word->mode);
}

static void loadAddr(const struct Operand *src)
{
    switch (src->mode) {
    case MODE_ABSOLUTE:
        LDA(stringf("#<%s", src->base));
        LDX(stringf("#>%s", src->base));
        return;
    case MODE_OFFSET:
        LDA(stringf("#<%s+%s", src->base, src->offset));
        LDX(stringf("#>%s+%s", src->base, src->offset));
        return;
    case MODE_VARIABLE_OFFSET: {
        // ptr := arrb_varb
        LDA(stringf("#<%s", src->base));
        LDX(stringf("#>%s", src->base));
        CLC();
        ADC(stringf("%s", src->offset));
        BCC(immediate(strcopy("1"))); // skip INX
        INX();
        return;
    }
    case MODE_INDIRECT_OFFSET:
        // ptr2 := ptr1_10
        LDA(stringf("<%s", src->base));
        LDX(stringf(">%s", src->base));
        CLC();
        ADC(stringf("%s", src->offset));
        BCC(immediate(strcopy("1"))); // skip INX
        INX();
        return;
    case MODE_IMMEDIATE:
    case MODE_REGISTER:
        fatalf("%s: invalid mode type: %d", __func__, src->mode);
    }
    fatalf("%s: unhandled mode type: %d", __func__, src->mode);
}

static void transfer(char dst, char src)
{
    switch (dst) {
    case 'A':
        switch (src) {
        case 'X':
            TXA();
            return;
        case 'Y':
            TYA();
            return;
        case 'A':
            // do nothing
            return;
        }
        break;

    case 'X':
        switch (src) {
        case 'Y':
            TYA();
            TAX();
            return;
        case 'A':
            TAX();
            return;
        case 'X':
            // do nothing
            return;
        }
        break;

    case 'Y':
        switch (src) {
        case 'X':
            TXA();
            TAY();
            return;
        case 'A':
            TAY();
            return;
        case 'Y':
            // do nothing
            return;
        }
        break;
    }
    fatalf("unhandled register transfer: %s <- %s", dst, src);
}

static void loadByte(char dstRegister, const struct Operand *src)
{
    Load LD = LDA;
    if (dstRegister == 'Y') {
        LD = LDY;
    } else if (dstRegister == 'X') {
        LD = LDX;
    }

    switch (src->mode) {
    case MODE_IMMEDIATE:
        LD(stringf("#%s", src->immlo));
        return;
    case MODE_ABSOLUTE:
        LD(stringf("%s", src->base));
        return;
    case MODE_OFFSET:
        LD(stringf("%s+%s", src->base, src->offset));
        return;
    case MODE_VARIABLE_OFFSET:
        if (dstRegister == 'A' || dstRegister == 'X') {
            LDY(stringf("%s", src->offset));
            LD(stringf("%s,Y", src->base));
        } else {
            LDX(stringf("%s", src->offset));
            LD(stringf("%s,X", src->base));
        }
        return;
    case MODE_INDIRECT_OFFSET:
        if (src->offset[0] == '@') {
            require(src->offset[1] == 'Y',
                "only Y can be used as the offset register: got %s",
                &src->offset[1]);
        } else {
            LDY(stringf("%s", src->offset));
        }
        LDA(stringf("(%s),Y", src->base));
        if (dstRegister == 'Y') {
            TAY();
        } else if (dstRegister == 'X') {
            TAX();
        }
        return;
    case MODE_REGISTER:
        transfer(dstRegister, regLow(src));
        return;
    }

    fatalf("%s: unhandled mode type: %d", __func__, src->mode);
}

static void loadWord(char dstHi, char dstLo, const struct Operand *src)
{
    typedef void (*load)(char *);
    load LDLSB = LDA,
         LDMSB = LDX;

    require(dstHi != dstLo, "%s: register conflict: %c==%c", __func__, dstHi, dstLo);

    if (dstHi == 'A') {
        LDMSB = LDA;
    } else if (dstHi == 'Y') {
        LDMSB = LDY;
    }

    if (dstLo == 'X') {
        LDLSB = LDX;
    } else if (dstLo == 'Y') {
        LDLSB = LDY;
    }

    switch (src->mode) {
    case MODE_IMMEDIATE:
        LDLSB(stringf("#%s", src->immlo));
        LDMSB(stringf("#%s", src->immhi));
        return;
    case MODE_ABSOLUTE:
        LDLSB(stringf("%s", src->base));
        LDMSB(stringf("%s+1", src->base));
        return;
    case MODE_OFFSET:
        LDLSB(stringf("%s+%s", src->base, src->offset));
        LDMSB(stringf("%s+%s+1", src->base, src->offset));
        return;
    case MODE_VARIABLE_OFFSET:
        require(dstHi == 'X' && dstLo == 'A', "TODO: finish %s", __func__);
        LDY(stringf("%s", src->offset));
        LDA(stringf("%s,Y", src->base));
        LDX(stringf("%s+1,Y", src->base)); // XA
        return;
    case MODE_INDIRECT_OFFSET:
        LDY(stringf("%s", src->offset));
        if (dstLo == 'A') {
            if (dstHi == 'X') {
                // XA
                INY();
                LDA(stringf("(%s),Y", src->base));
                TAX();
                DEY();
                LDA(stringf("(%s),Y", src->base));
            } else {
                require(dstHi == 'Y', "expect Y; got %c", dstHi);
                // YA
                LDA(stringf("(%s),Y", src->base));
                PHA();
                INY();
                LDA(stringf("(%s),Y", src->base));
                TAY();
                PLA();
            }
        } else if (dstLo == 'X') {
            LDA(stringf("(%s),Y", src->base));
            TAX();
            INY();
            LDA(stringf("(%s),Y", src->base));
            if (dstHi == 'Y') {
                // YX
                TAY();
            } else {
                // AX
                require(dstHi == 'A', "expect A; got %c", dstHi);
            }
        } else {
            require(dstLo == 'Y', "expect Y; got %c", dstLo);
            // AY
            if (dstHi == 'A') {
                INY();
                LDA(stringf("(%s),Y", src->base));
                PHA();
                DEY();
                LDA(stringf("(%s),Y", src->base));
                TAY();
                PLA();
            } else {
                require(dstHi == 'X', "expect X; got %c", dstHi);
                // XY
                INY();
                LDA(stringf("(%s),Y", src->base));
                TAX();
                DEY();
                LDA(stringf("(%s),Y", src->base));
                TAY();
            }
        }
        return;
    case MODE_REGISTER:
        fatalf("register transfers are unsupported");
    }

    fatalf("%s: unhandled mode type: %d", __func__, src->mode);
}

static void storeByte(const struct Operand *dst)
{
    switch (dst->mode) {
    case MODE_IMMEDIATE:
        STA(stringf("#%s", dst->immlo));
        return;
    case MODE_ABSOLUTE:
        STA(stringf("%s", dst->base));
        return;
    case MODE_OFFSET:
        STA(stringf("%s+%s", dst->base, dst->offset));
        return;
    case MODE_VARIABLE_OFFSET:
        LDY(stringf("%s", dst->offset));
        STA(stringf("%s,Y", dst->base));
        return;
    case MODE_INDIRECT_OFFSET:
        LDY(stringf("%s", dst->offset));
        STA(stringf("(%s),Y", dst->base));
        return;
    case MODE_REGISTER:
        fatalf("%s: logic error MODE_REGISTER %s", __func__, dst->immhi);
    }
    fatalf("%s: unhandled mode type: %d", __func__, dst->mode);
}

static void storeWord(const struct Operand *dst)
{
    storeByte(dst);
    switch (dst->mode) {
    case MODE_IMMEDIATE:
        STX(stringf("#%s", dst->immhi));
        return;
    case MODE_ABSOLUTE:
        STX(stringf("%s+1", dst->base));
        return;
    case MODE_OFFSET:
        STX(stringf("%s+%s+1", dst->base, dst->offset));
        return;
    case MODE_VARIABLE_OFFSET:
        // assumes value is already in Y
        INY();
        STX(stringf("%s,Y", dst->base));
        return;
    case MODE_INDIRECT_OFFSET:
        // assumes value is already in Y
        INY();
        STX(stringf("(%s),Y", dst->base));
        return;
    case MODE_REGISTER:
        fatalf("%s: logic error MODE_REGISTER %s", __func__, dst->immhi);
    }
    fatalf("%s: unhandled mode type: %d", __func__, dst->mode);
}

typedef void (*MathOp)(char *);
static void mathByte(const struct Operand *src, MathOp MATH)
{
    switch (src->mode) {
    case MODE_IMMEDIATE:
        MATH(stringf("#%s", src->immlo));
        return;

    case MODE_ABSOLUTE:
        MATH(stringf("%s", src->base));
        return;

    case MODE_OFFSET:
        MATH(stringf("%s+%s", src->base, src->offset));
        return;

    case MODE_VARIABLE_OFFSET:
        LDY(stringf("%s", src->offset));
        MATH(stringf("%s,Y", src->base));
        return;

    case MODE_INDIRECT_OFFSET:
        if (src->offset[0] == '@') {
            require(src->offset[1] == 'Y',
                "only Y can be used as the offset register: got %s",
                &src->offset[1]);
        }
        LDY(stringf("%s", src->offset));
        MATH(stringf("(%s),Y", src->base));
        return;

    case MODE_REGISTER:
        fatalf("register arithmetic is unsupported");
    }

    fatalf("%s: unhandled mode type: %d", __func__, src->mode);
}

static inline void addByte(const struct Operand *operand) { mathByte(operand, ADC); }
static inline void subtractByte(const struct Operand *operand) { mathByte(operand, SBC); }

struct Arithmetic {
    const char *Name;
    void (*Operation)(const struct Operand *);
    void (*ClearFlag)(void);
    void (*IncrementX)(void);
    void (*IncrementY)(void);
};

static struct Arithmetic addition = {
    .Name       = "ADD",
    .Operation  = addByte,
    .ClearFlag  = CLC,
    .IncrementX = INX,
    .IncrementY = INY,
};

static struct Arithmetic subtract = {
    .Name       = "SUB",
    .Operation  = subtractByte,
    .ClearFlag  = SEC,
    .IncrementX = DEX,
    .IncrementY = DEY,
};

static inline char *mathMacroString(
    const struct Arithmetic *op,
    const char              *suffix,
    const struct Operand    *dst,
    const struct Operand    *src)
{
    char *macroName = stringf("%s%s", op->Name, suffix);
    char *string = macroString(macroName, dst, src);
    free(macroName);
    return string;
}

static void MATHBB(const struct Arithmetic *op, const struct Operand *dst, const struct Operand *src)
{
    REM(mathMacroString(op, "BB", dst, src));

    if (dst->mode != MODE_REGISTER) {
        loadByte('A', src);
        op->ClearFlag();
        op->Operation(dst);
        storeByte(dst);
        return;
    }
    // Optimize += 0, += 1, += 2
    if (src->mode == MODE_IMMEDIATE && src->number.valid) {
        switch (src->number.value) {
        case 0:
            REM("Optimized out += 0");
            return;
        case 2:
            if (regLow(dst) == 'X') {
                op->IncrementX();
            } else if (regLow(dst) == 'Y') {
                op->IncrementY();
            } else {
                break;
            }
            // fallthru
        case 1:
            if (regLow(dst) == 'X') {
                op->IncrementX();
            } else if (regLow(dst) == 'Y') {
                op->IncrementY();
            } else {
                break;
            }
            return;
        default:
            break;
        }
    }
    // Handle adding into a register
    if (regLow(dst) == 'X') {
        TXA();
        op->ClearFlag();
        op->Operation(src);
        TAX();
        return;
    }
    if (regLow(dst) == 'Y') {
        TYA();
        op->ClearFlag();
        op->Operation(src);
        TAY();
        return;
    }
    require(regLow(dst) == 'A', "expected register A; got %s", dst->immlo);
    op->ClearFlag();
    op->Operation(src);
    return;
}

static void MATHWB(const struct Arithmetic *op, const struct Operand *dst, const struct Operand *src)
{
    REM(mathMacroString(op, "WB", dst, src));

    op->ClearFlag();
    loadByte('A', src);
    op->Operation(dst);
    storeByte(dst);

    struct Operand *msb = highByte(dst);
    loadByte('A', &ZEROB);
    op->Operation(msb);
    storeByte(msb);

    FreeOperand(msb);
}

static void MATHWW(const struct Arithmetic *op, const struct Operand *dst, const struct Operand *src)
{
    REM(mathMacroString(op, "WW", dst, src));

    op->ClearFlag();
    loadByte('A', src);
    op->Operation(dst);
    storeByte(dst);

    struct Operand *dstmsb = highByte(dst),
                   *srcmsb = highByte(src);
    loadByte('A', srcmsb);
    op->Operation(dstmsb);
    storeByte(dstmsb);

    FreeOperand(srcmsb);
    FreeOperand(dstmsb);
}

void ADDR(const char *pointer, const struct Operand *src)
{
    loadAddr(src);
    STX(stringf("%s+1", pointer));
    STA(stringf("%s", pointer));
}

void FreeOperand(struct Operand *operand)
{
    if (!operand) {
        return;
    }
    if (operand->offset) {
        free(operand->offset);
    }
    if (operand->immhi) {
        free(operand->immhi);
    }
    if (operand->immlo) {
        free(operand->immlo);
    }
    free(operand);
}

static void COPYBB(const struct Operand *dst, const struct Operand *src)
{
    REM(macroString(__func__, dst, src));
    if (dst->mode == MODE_REGISTER) {
        loadByte(regLow(dst), src);
        return;
    }
    loadByte('A', src);
    storeByte(dst);
}

static void COPYWB(const struct Operand *dst, const struct Operand *src)
{
    REM(macroString(__func__, dst, src));
    if (dst->mode == MODE_REGISTER) {
        loadByte(regLow(dst), src);
        loadByte(regHigh(dst), &ZEROB);
        return;
    }
    loadByte('A', src);
    loadByte('X', &ZEROB);
    storeWord(dst);
}

static void COPYWW(const struct Operand *dst, const struct Operand *src)
{
    REM(macroString(__func__, dst, src));
    if (dst->mode == MODE_REGISTER) {
        loadWord(regHigh(dst), regLow(dst), src);
        return;
    }
    loadWord('X', 'A', src);
    storeWord(dst);
}

void COPY(const struct Operand *dst, const struct Operand *src)
{
    if (dst->size == 1) {
        COPYBB(dst, src);
        if (src->size == 2) {
            warnf("right-hand side will be truncated to a byte");
            REM(strcopy("WARNING: VALUE TRUNCATED"));
        }
    } else if (dst->size == 2 && src->size == 1) {
        COPYWB(dst, src);
    } else if (dst->size == 2 && src->size == 2) {
        COPYWW(dst, src);
    } else {
        fatalf("%s: bad operand size: %u %u", __func__, dst->size, src->size);
    }
}

static void compareByte(char reg, const struct Operand *val)
{
    Compare Compare = CMP;
    if (reg == 'Y') {
        Compare = CPY;
    } else if (reg == 'X') {
        Compare = CPX;
    }

    switch (val->mode) {
    case MODE_IMMEDIATE:
        Compare(stringf("#%s", val->immlo));
        return;

    case MODE_ABSOLUTE:
        Compare(stringf("%s", val->base));
        return;

    case MODE_OFFSET:
        Compare(stringf("%s+%s", val->base, val->offset));
        return;

    case MODE_VARIABLE_OFFSET:
        if (reg == 'A' || reg == 'X') {
            LDY(stringf("%s", val->offset));
            CMP(stringf("%s,Y", val->base));
        } else {
            LDX(stringf("%s", val->offset));
            CMP(stringf("%s,X", val->base));
        }
        return;

    case MODE_INDIRECT_OFFSET:
        if (val->offset[0] == '@') {
            require(val->offset[1] == 'Y',
                "only Y can be used as the offset register: got %s",
                &val->offset[1]);
        }
        if (reg == 'Y') {
            TYA();
        } else if (reg == 'X') {
            TXA();
        }
        LDY(stringf("%s", val->offset));
        CMP(stringf("(%s),Y", val->base));
        return;

    case MODE_REGISTER:
        fatalf("register comparisons are unsupported");
    }

    fatalf("%s: unhandled mode type: %d", __func__, val->mode);
}

static void compareByteUnless0(char reg, const struct Operand *val) {
    if (val->mode == MODE_IMMEDIATE) {
        if (val->number.valid && val->number.value == 0) {
            // No reason to output CMP #0
            return;
        }
    }
    compareByte(reg, val);
}

// left == right
void IFEQ(const struct Operand *left, const struct Operand *right, const char *then, const char *done)
{
    if (left->size == 1 && right->size == 2) {
        IFEQ(right, left, then, done);
        return;
    }

    REM(macroString(__func__, left, right));
    REM(stringf("  %s %s", then, done));

    if (left->size == 1 && right->size == 1) {
        if (left->mode == MODE_REGISTER) {
            compareByte(regLow(left), right);
            BEQ(strcopy(then));
            JMP(strcopy(done));
        } else {
            loadByte('A', left);
            compareByteUnless0('A', right);
            BEQ(strcopy(then));
            JMP(strcopy(done));
        }
    } else if (left->size == 2 && right->size == 1) {
        if (left->mode == MODE_REGISTER) {
            compareByte(regHigh(left), &ZEROB);
            BNE(strcopy(done));
            compareByte(regLow(left), right);
            BEQ(strcopy(then));
            JMP(strcopy(done));
        } else {
            struct Operand *msb = highByte(left);

            loadByte('A', msb);
            // No need to CMP #$00 because loading affects Z-flag
            BNE(strcopy(done));
            loadByte('A', left);
            compareByteUnless0('A', right);
            BEQ(strcopy(then));
            JMP(strcopy(done));

            FreeOperand(msb);
        }
    } else if (left->size == 2 && right->size == 2) {
        struct Operand *rmsb = highByte(right);
        if (left->mode == MODE_REGISTER) {
            compareByte(regHigh(left), rmsb);
            BNE(strcopy(done));
            compareByte(regLow(left), right);
            BEQ(strcopy(then));
            JMP(strcopy(done));
        } else {
            struct Operand *lmsb = highByte(left);

            loadByte('A', lmsb);
            compareByte('A', rmsb);
            BNE(strcopy(done));
            loadByte('A', left);
            compareByteUnless0('A', right);
            BEQ(strcopy(then));
            JMP(strcopy(done));

            FreeOperand(lmsb);
        }
        FreeOperand(rmsb);
    } else {
        fatalf("%s: bad operand size: %u %u", __func__, left->size, right->size);
    }
}

// left >= right
void IFGE(const struct Operand *left, const struct Operand *right, const char *then, const char *done)
{
    REM(macroString(__func__, left, right));
    REM(stringf("  %s %s", then, done));

    if (left->size == 1 && right->size == 1) {
        if (left->mode == MODE_REGISTER) {
            compareByte(regLow(left), right);
            BCS(strcopy(then));
            JMP(strcopy(done));
        } else {
            loadByte('A', left);
            compareByteUnless0('A', right);
            BCS(strcopy(then));
            JMP(strcopy(done));
        }
    } else if (left->size == 1 && right->size == 2) {
        if (right->mode == MODE_REGISTER) {
            compareByte(regHigh(right), &ZEROB);
            // left >= right
            BNE(strcopy(done)); // rightH is >0 so right >0xFF and left < right
            compareByte(regLow(left), right);
            BCS(strcopy(then));
            JMP(strcopy(done));
        } else {
            struct Operand *msb = highByte(right);

            loadByte('A', msb);
            BNE(strcopy(done));
            loadByte('A', left);
            compareByteUnless0('A', right);
            BCS(strcopy(then));
            JMP(strcopy(done));

            FreeOperand(msb);
        }
    } else if (left->size == 2 && right->size == 1) {
        if (left->mode == MODE_REGISTER) {
            compareByte(regHigh(left), &ZEROB);
            BNE(strcopy(then)); // leftH is >0 so left >0xFF > right
            compareByteUnless0(regLow(left), right);
            BCS(strcopy(then));
            JMP(strcopy(done));
        } else {
            struct Operand *msb = highByte(left);

            loadByte('A', msb);
            // No need to CMP #0; load affects Z-flag.
            // No need to BCC(done) because it will ALWAYS be false (u8 cannot be < 0)
            BNE(strcopy(then)); // leftH >0 so left >0xff > right
            loadByte('A', left);
            compareByteUnless0('A', right);
            BCS(strcopy(then));
            JMP(strcopy(done));

            FreeOperand(msb);
        }
    } else if (left->size == 2 && right->size == 2) {
        struct Operand *rmsb = highByte(right);
        if (left->mode == MODE_REGISTER) {
            compareByte(regHigh(left), rmsb);
            BNE(strcopy(done));
            compareByte(regLow(left), right);
            BCS(strcopy(then));
            JMP(strcopy(done));
        } else {
            struct Operand *lmsb = highByte(left);

            loadByte('A', lmsb);
            compareByteUnless0('A', rmsb);
            BCC(strcopy(done));
            BNE(strcopy(then));
            loadByte('A', left);
            compareByteUnless0('A', right);
            BCS(strcopy(then));
            JMP(strcopy(done));

            FreeOperand(lmsb);
        }
        FreeOperand(rmsb);
    } else {
        fatalf("%s: bad operand size: %u %u", __func__, left->size, right->size);
    }
}

void IFGT(const struct Operand *left, const struct Operand *right, const char *then, const char *done)
{
    // left > right :=: right < left
    IFLT(right, left, then, done);
}

// left < right
void IFLT(const struct Operand *left, const struct Operand *right, const char *then, const char *done)
{
    if (left->size == 1 && right->size == 2) {
        // M < N :=: N >= M
        IFGE(right, left, then, done);
        return;
    }

    REM(macroString(__func__, left, right));
    REM(stringf("  %s %s", then, done));

    if (left->size == 1 && right->size == 1) {
        if (left->mode == MODE_REGISTER) {
            compareByte(regLow(left), right);
            BCC(strcopy(then));
            JMP(strcopy(done));
        } else {
            loadByte('A', left);
            compareByteUnless0('A', right);
            BCC(strcopy(then));
            JMP(strcopy(done));
        }
    } else if (left->size == 2 && right->size == 1) {
        if (left->mode == MODE_REGISTER) {
            compareByte(regHigh(left), &ZEROB);
            BNE(strcopy(done)); // leftH is >0 so left >0xFF and cannot be < right
            compareByte(regLow(left), right);
            BCC(strcopy(then));
            JMP(strcopy(done));
        } else {
            struct Operand *msb = highByte(left);

            loadByte('A', msb);
            BCC(strcopy(then)); // leftH < rightH so left < right
            BNE(strcopy(done)); // leftH <> rightH so leftH > rightH (left >= right)
            loadByte('A', left);
            compareByteUnless0('A', right);
            BCC(strcopy(then));
            JMP(strcopy(done));

            FreeOperand(msb);
        }
    } else if (left->size == 2 && right->size == 2) {
        struct Operand *rmsb = highByte(right);
        if (left->mode == MODE_REGISTER) {
            compareByte(regHigh(left), rmsb);
            BNE(strcopy(done));
            compareByte(regLow(left), right);
            BCC(strcopy(then));
            JMP(strcopy(done));
        } else {
            struct Operand *lmsb = highByte(left);

            loadByte('A', lmsb);
            compareByteUnless0('A', rmsb);
            BCC(strcopy(then));
            BNE(strcopy(done));
            loadByte('A', left);
            compareByteUnless0('A', right);
            BCC(strcopy(then));
            JMP(strcopy(done));

            FreeOperand(lmsb);
        }
        FreeOperand(rmsb);
    } else {
        fatalf("%s: bad operand size: %u %u", __func__, left->size, right->size);
    }
}

// left <= right
void IFLE(const struct Operand *left, const struct Operand *right, const char *then, const char *done)
{
    // For M <= N, N >= M produces shorter code, so swap the operands.
    IFGE(right, left, then, done);
}

// left <> right
void IFNE(const struct Operand *left, const struct Operand *right, const char *then, const char *done)
{
    if (left->size == 1 && right->size == 2) {
        IFNE(right, left, then, done);
        return;
    }

    REM(macroString(__func__, left, right));
    REM(stringf("  %s %s", then, done));

    if (left->size == 1 && right->size == 1) {
        if (left->mode == MODE_REGISTER) {
            compareByte(regLow(left), right);
            BNE(strcopy(then));
            JMP(strcopy(done));
        } else {
            loadByte('A', left);
            compareByteUnless0('A', right);
            BNE(strcopy(then));
            JMP(strcopy(done));
        }
    } else if (left->size == 2 && right->size == 1) {
        if (left->mode == MODE_REGISTER) {
            compareByte(regHigh(left), &ZEROB);
            BNE(strcopy(then));
            compareByte(regLow(left), right);
            BNE(strcopy(then));
            JMP(strcopy(done));
        } else {
            struct Operand *msb = highByte(left);

            loadByte('A', msb);
            // No need to CMP #$00 because loading affects Z-flag.
            BNE(strcopy(then));
            loadByte('A', left);
            compareByteUnless0('A', right);
            BNE(strcopy(then));
            JMP(strcopy(done));

            FreeOperand(msb);
        }
    } else if (left->size == 2 && right->size == 2) {
        struct Operand *rmsb = highByte(right);
        if (left->mode == MODE_REGISTER) {
            compareByte(regHigh(left), rmsb);
            BNE(strcopy(then));
            compareByte(regLow(left), right);
            BNE(strcopy(then));
            JMP(strcopy(done));
        } else {
            struct Operand *lmsb = highByte(left);

            loadByte('A', lmsb);
            compareByteUnless0('A', rmsb);
            BNE(strcopy(then));
            loadByte('A', left);
            compareByteUnless0('A', right);
            BNE(strcopy(then));
            JMP(strcopy(done));

            FreeOperand(lmsb);
        }
        FreeOperand(rmsb);
    } else {
        fatalf("%s: bad operand size: %u %u", __func__, left->size, right->size);
    }
}

void LESS(const struct Operand *dst, const struct Operand *src)
{
    if (dst->size == 1) {
        MATHBB(&subtract, dst, src);
        if (src->size == 2) {
            warnf("right-hand side will be truncated to a byte");
            REM(strcopy("WARNING: VALUE TRUNCATED"));
        }
    } else if (dst->size == 2 && src->size == 1) {
        MATHWB(&subtract, dst, src);
    } else if (dst->size == 2 && src->size == 2) {
        MATHWW(&subtract, dst, src);
    } else {
        fatalf("%s: bad operand size: %u %u", __func__, dst->size, src->size);
    }
}

struct Operand *OpAbsolute(const char *base, uint8_t size)
{
    return Operand(MODE_ABSOLUTE, size, base, NULL, NULL, NULL);
}

struct Operand *OpImmediate(char *value, uint8_t size)
{
    return Operand(MODE_IMMEDIATE, size, NULL, NULL, value, NULL);
}

struct Operand *OpImmediateNumber(uint16_t number)
{
    struct Operand *operand;
    if (number > 0xFF) {
        operand = OpImmediateWord(hex2(number), hex2(number >> 8));
    } else {
        operand = OpImmediate(hex2(number), 1);
    }
    operand->number.valid = true;
    operand->number.value = number;
    return operand;
}

struct Operand *OpImmediateWord(char *lo, char *hi)
{
    return Operand(MODE_IMMEDIATE, 2, NULL, NULL, lo, hi);
}

struct Operand *OpIndirectOffset(const char *pointer, char *offset, uint8_t size)
{
    return Operand(MODE_INDIRECT_OFFSET, size, pointer, offset, NULL, NULL);
}

struct Operand *OpOffset(const char *base, char *offset, bool isVariable, uint8_t size)
{
    enum Mode mode = isVariable ? MODE_VARIABLE_OFFSET : MODE_OFFSET;
    return Operand(mode, size, base, offset, NULL, NULL);
}

struct Operand *OpRegister(char reg)
{
    return Operand(MODE_REGISTER, 1, NULL, NULL, stringf("%c", reg), NULL);
}

struct Operand *OpRegisterWord(char reghi, char reglo)
{
    return Operand(MODE_REGISTER, 2, NULL, NULL, stringf("%c", reglo), stringf("%c", reghi));
}

void PLUS(const struct Operand *dst, const struct Operand *src)
{
    if (dst->size == 1) {
        MATHBB(&addition, dst, src);
        if (src->size == 2) {
            warnf("right-hand side will be truncated to a byte");
            REM(strcopy("WARNING: VALUE TRUNCATED"));
        }
    } else if (dst->size == 2 && src->size == 1) {
        MATHWB(&addition, dst, src);
    } else if (dst->size == 2 && src->size == 2) {
        MATHWW(&addition, dst, src);
    } else {
        fatalf("%s: bad operand size: %u %u", __func__, dst->size, src->size);
    }
}
