#include <string.h>

#include "asm.h"
#include "compiler.h"
#include "io.h"
#include "symbols.h"

static struct Program program;

static bool writeAST, dumpInstructions, dumpSymbols;

static void onexit(void)
{
    if (writeAST) {
        WriteAST(stderr, &program);
    }
    if (dumpSymbols) {
        DumpSymbols(stderr);
    }
    if (dumpInstructions) {
        WriteInstructions(stderr);
    }
}

static void usage(void)
{
    puts("Compile an A2 file into 6502 assembly\n");
    puts("usage: compile [-h|--help] [-asm] [-ast] [-sym] file");
    puts("   --help|-h  Display this help message");
    puts("   -asm       Write assembly to stderr");
    puts("   -ast       Show the parsed, Abstract Syntax Tree");
    puts("   -sym       Dump the Symbol Table");
}

int main(int argc, const char *argv[argc])
{
    if (argc < 2) {
        usage();
        return 2;
    }

    const char *path = NULL;

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] != '-') {
            path = argv[i];
        } else if (strcmp("-ast", argv[i]) == 0) {
            writeAST = true;
        } else if (strcmp("-asm", argv[i]) == 0) {
            dumpInstructions = true;
        } else if (strcmp("-sym", argv[i]) == 0) {
            dumpSymbols = true;
        } else if (strcmp("-h", argv[i]) == 0 || strcmp("--help", argv[i]) == 0) {
            usage();
            return 0;
        } else {
            warnf("unknown flag %s", argv[i]);
            usage();
            return 0;
        }
    }

    require(path, "no input file specified");

    const char *contents = ReadFile(path);
    require(contents, "failed to read file: %s", path);

    atexit(onexit);

    unsigned badLine = 0;
    const char *remaining = Parse(contents, &program, &badLine);
    if (remaining && remaining[0] != '\0') {
        const char *endofline = remaining;
        while (*endofline != '\0' && *endofline != '\n' ) {
            endofline++;
        }
        if (*endofline == '\n') {
            endofline++;
            while (*endofline != '\0' && *endofline != '\n') {
                endofline++;
            }
        }
        while (remaining != contents && *remaining != '\n') {
            // find start of line
            remaining--;
        }
        if (*remaining == '\n') {
            remaining--;
            while (remaining != contents && *remaining != '\n') {
                // find start of prev line
                remaining--;
            }
        }
        fprintf(stderr, "syntax error around line %u", badLine);
        fprintf(stderr, "%.*s\n", (int)(endofline - remaining), remaining);
        return 1;
    }
    if (!remaining) {
        fatalf("invalid program");
    }

    Generate(stdout, &program);

    return 0;
}
