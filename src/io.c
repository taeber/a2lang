#include "io.h"

#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

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
    int         fd;

    // Special case: "-" means read from stdin
    if (strcmp(path, "-") == 0) {
        fd = STDIN_FILENO; // Use stdin file descriptor
    } else {
        fd = open(path, O_RDONLY, 0);
        if (fd < 0) {
            return NULL;
        }
    }

    // Try to stat the file - if this fails, it might be a pipe/fifo
    if (stat(path, &stats) == 0 && S_ISREG(stats.st_mode)) {
        // Regular file - use mmap for efficiency
        void *contents = mmap(NULL, stats.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (contents != MAP_FAILED) {
            return contents;
        }
        // Fall through to read() if mmap fails
    }

    // Pipe, FIFO, or mmap failed - read into dynamically allocated buffer
    size_t capacity = 4096;
    size_t size     = 0;
    char  *buffer   = malloc(capacity);
    if (!buffer) {
        close(fd);
        return NULL;
    }

    ssize_t bytes_read;
    while ((bytes_read = read(fd, buffer + size, capacity - size - 1)) > 0) {
        size += bytes_read;

        // If buffer is getting full, expand it
        if (size >= capacity - 1) {
            capacity *= 2;
            char *new_buffer = realloc(buffer, capacity);
            if (!new_buffer) {
                free(buffer);
                close(fd);
                return NULL;
            }
            buffer = new_buffer;
        }
    }

    close(fd);

    if (bytes_read < 0) {
        free(buffer);
        return NULL;
    }

    // Null-terminate the buffer
    buffer[size] = '\0';
    return buffer;
}
