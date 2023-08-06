.POSIX:
.SUFFIXES:
CC	    = clang
CFLAGS  = -Wall -Wextra -Wpedantic -std=c17 -D_XOPEN_SOURCE \
		  -Wconversion -Wdouble-promotion \
		  -Wno-unused-parameter \
		  -fsanitize=address,undefined -fsanitize-trap \
		  -g3

debug:
	make -j4 compile

release:
	make -j4 CFLAGS='-std=c17 -D_XOPEN_SOURCE -O3' compile


compile: src/main.o src/asm.o src/codegen.o src/grammar.o src/io.o src/parser.o src/symbols.o src/text.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

src/main.o: src/main.c
src/asm.o: src/asm.h src/asm.c src/asm-op.c
src/codegen.o: src/codegen.h src/codegen.c
src/grammar.o: src/grammar.h src/grammar.c
src/io.o: src/io.h src/io.c
src/parser.o: src/parser.h src/parser.c
src/symbols.o: src/symbols.h src/symbols.c
src/text.o: src/text.h src/text.c

src/fake6502.h:
	curl -L https://raw.githubusercontent.com/datajerk/c2t/f5840bdab08c323dd3593758510b9acca8173b44/fake6502.h >$@

vm: src/vm.c src/fake6502.h
	$(CC) $(CFLAGS) $(PWD)/$< $(LDFLAGS) -o $@ $(LDLIBS)

.PHONY: tests
tests: debug vm
	tests/compile-all.bash

.PHONY: clean
clean:
	rm -f compile vm src/*.o src/fake6502.h
	rm -rf ./*.dSYM

.PHONY: rebuild
rebuild:
	make clean && make

.SUFFIXES: .c .o
.c.o:
	$(CC) $(CFLAGS) -c $(PWD)/$< -o $(PWD)/$@
