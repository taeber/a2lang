#pragma once

#include <stdbool.h>

struct Program;

const char *Parse(const char *text, struct Program *outProg, unsigned *outLine);

void Generate(FILE *fp, struct Program *program);
