#include "io.h"

#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>

void Logf(const char *level, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    fprintf(stderr, "%s: ", level);
    vfprintf(stderr, fmt, args);
    fputs(".\n", stderr);

    va_end(args);
}

const char *ReadFile(const char *path)
{
    if (!path) {
        return NULL;
    }

    struct stat stats;
    stat(path, &stats);

    int fd = open(path, O_RDONLY, 0);
    if (!fd) {
        return NULL;
    }

    void *contents = mmap(NULL, stats.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (contents == MAP_FAILED) {
        return NULL;
    }

    return contents;
}
