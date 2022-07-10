#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// https://raw.githubusercontent.com/datajerk/c2t/f5840bdab08c323dd3593758510b9acca8173b44/fake6502.h
#include "fake6502.h"

const static uint16_t EXIT   = 0x03D0,
                      COUT   = 0xFDED,
                      RDKEY  = 0xFD0C,
                      INIT   = 0xFB2F,
                      HOME   = 0xFC58,
                      CROUT  = 0xFD8E,
                      PRBYTE = 0xFDDA;

const static uint8_t RTS = 0x60;

static uint8_t mem[0xffff];

uint8_t read6502(uint16_t address)
{
    return mem[address];
}

void write6502(uint16_t address, uint8_t value)
{
    mem[address] = value;
}

static inline uint16_t u16le(uint8_t val[2])
{
    return ((uint16_t)val[0]) | ((((uint16_t)val[1]) << 8) & 0xFF00);
}

static void ontick(void)
{
    fprintf(stderr,
        "%04X:  %02X %02X %02X  | A=%02X X=%02X Y=%02X | ticks=%u\n",
        pc, mem[pc], mem[pc + 1], mem[pc + 2], a, x, y, clockticks6502);
    switch (pc) {
    case EXIT:
        fprintf(stderr, " clock ticks = %u\n", clockticks6502);
        fprintf(stderr, "instructions = %u\n", instructions);
        exit(0);
        break;
    case COUT:
        putchar(a ^ 0x80);
        fflush(stdout);
        break;
    case CROUT:
        putchar('\n');
        fflush(stdout);
        break;
    case RDKEY:
        a = getchar();
        if (feof(stdin)) {
            a = 0x04; // ASCII for "End of Transmission"
        }
        a |= 0x80;
        break;
    case HOME:
        system("clear");
        break;
    case INIT:
        break;
    case PRBYTE:
        printf("%02X", a);
        fflush(stdout);
        break;
    default:
        return;
    }
    mem[pc] = RTS;
}

static void usage(FILE *out, const char *exepath)
{
    fprintf(out, "usage: %s [flags] binfile\n", exepath);
    fprintf(out, "  --help     displays this message\n");
    fprintf(out, "  --quiet    prevents output to stderr (default)\n");
    fprintf(out, "  --verbose  writes out the CPU status every cycle\n");
}

int main(int argc, char *argv[argc])
{
    if (argc == 0)
        return 42;

    if (argc < 2) {
        usage(stderr, argv[0]);
        return 2;
    }

    const char *filename = "";
    bool        verbose  = false;

    for (int i = 1; i < argc; i++) {
        bool helpWanted = strcmp("-h", argv[i]) == 0
            || strcmp("--help", argv[i]) == 0
            || strcmp("/?", argv[i]) == 0;
        if (helpWanted) {
            usage(stdout, argv[0]);
            return 0;
        }
        if (strcmp("--verbose", argv[i]) == 0) {
            verbose = true;
            continue;
        }
        if (strcmp("--quiet", argv[i]) == 0) {
            verbose = false;
            continue;
        }
        if (argv[i][0] == '-') {
            fprintf(stderr, "error: unknown flag: %s\n", argv[i]);
            usage(stderr, argv[0]);
            return 2;
        }
        filename = argv[i];
    }

    if (!verbose) {
        freopen("/dev/null", "wb", stderr);
    }

    FILE *binfile = fopen(filename, "rb");
    if (!binfile) {
        perror("fopen");
        return 1;
    }

    uint8_t buf[4];
    if (fread(buf, sizeof *buf, sizeof buf, binfile) != 4) {
        perror("fread: expected 4 bytes");
        return 1;
    }

    uint16_t org = u16le(&buf[0]),
             len = u16le(&buf[2]);

    fprintf(stderr, "loading %u bytes into $%4X\n", len, org);
    if (org + len > 0xffff) {
        fprintf(stderr, "%s\n", "program will not fit into memory!");
        return 1;
    }

    if (fread(&mem[org], sizeof *mem, len, binfile) != len) {
        perror("fread");
        return 1;
    }

    fprintf(stderr, "starting execution at: $%4X\n", org);

    hookexternal((void *)ontick);
    reset6502();
    exec6502(org);
}
