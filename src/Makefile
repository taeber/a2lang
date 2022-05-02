.POSIX:
.SUFFIXES:
CC	    = cc
CFLAGS  = -Wall -Wextra -Wpedantic -std=c17

debug:
	make -j4 CFLAGS='$(CFLAGS) -g' compile

compile: main.o asm.o codegen.o grammar.o io.o parser.o symbols.o text.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

asm.o: asm.h asm.c asm-op.c
codegen.o: codegen.h codegen.c
grammar.o: grammar.h grammar.c
io.o: io.h io.c
parser.o: parser.h parser.c
symbols.o: symbols.h symbols.c
text.o: text.h text.c

tests: debug
	cd ../tests && ./compile-all.bash

clean:
	rm -rf compile *.o ./*.dSYM

rebuild:
	make clean && make

.SUFFIXES: .c .o
.c.o:
	$(CC) $(CFLAGS) -c $(PWD)/$<
