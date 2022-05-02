#pragma once

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

void Logf(const char *level, const char *fmt, ...);

#define errorf(...) Logf("error", __VA_ARGS__)
#define fatalf(...) Logf("fatal", __VA_ARGS__), exit(1)
#define warnf(...) Logf("warning", __VA_ARGS__)

#define require(condition, ...) \
    if (!(condition))           \
    fatalf(__VA_ARGS__)

const char *ReadFile(const char *path);
