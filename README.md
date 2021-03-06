A2: A Programming Language for the Apple II
===========================================

A2 is a programming language targeting the MOS 6502 computer architecture of
the Apple II.
It is more higher-level than 6502 assembly, but not as high-level as C.

_Copyright © 2022 Taeber Rapczak \<taeber@rapczak.com>_.
_License: [MIT](LICENSE)_.

Here's a [sample](samples/hello.a2):

```swift
; hello.a2 (without comments)

asm {
	ORG $800
	JSR main
	JMP $3D0
}

use [
    COUT : sub <- [ch: char @ A] @ $FDED
    CROUT: sub @ $FD8E
]

var PTR: word @ $06

let Println = sub <- [txt: text @ PTR] {
    var i: int @ Y
    i := 0
    while txt_i <> 0 {
        COUT(txt_i)
        i  += 1
    }
    CROUT()
}

let main = sub {
    CROUT()
    Println("Hello, world!")
}
```

Check out the [samples](samples/) as well as the [test
cases](tests/) being used to verify the correctness of the compiler.

_(The [grammar](grammar.peg) is written in [PEG][], if you're into that kind of
thing. And if you *are* into that kind of thing, check out my
[VIM plugin for PEG][].)_

[PEG]: https://en.wikipedia.org/wiki/Parsing_expression_grammar
[VIM plugin for PEG]: https://github.com/taeber/vim-peg

## Quickstart

```
$ git clone https://github.com/taeber/a2lang
$ cd a2lang
$ ./a2 build samples/hello.a2
$ ./a2 run
# Opens an Apple II emulator.
# Load the build/DISK.DSK disk image, then run:
]BRUN PROG
```

![Screenshot of hello.a2 running in MAME](screenshot.png)

## Types

The language provides very few data types.
The three fundamental types are `byte`, `char`, and `word`.
* `byte` is simply an 8-bit value.
* `word` is a 16-bit, little-endian, unsigned value.
* `char` is also 8-bit, like `byte`, but it logically represents a single ASCII
character.

In addition, there are composite types that allow for collections of both
homogeneous (`array`) and heterogeneous (`group`) items.

```
; Comments start with a semi-colon.

var scores: byte^10  ; array of 10 bytes

; Define a new group called Point3D
let Point3D = [
    X: byte  ; Like Go, commas are required by the grammar
    Y: byte  ; but the compiler accepts a new line as well.
    Z: byte
]

var points: Point3D
```

There are also pointers to memory locations, but unlike other languages and
specifically because of the limitations of the 6502, their locations must be
known at compile-time and they must reside in the Zero Page.

You can also define new types aliases.
These are builtin aliases:

```
let [
    int  = :byte
    addr = :word
    text = :char^
]
```


## Control Flow

A2 provides conditional checks (`if`) and looping (`while`), but only one
kind for each.
There are **no** `for`-loops, `do`-loops, `else`, or `switch`
statements.
Inspiration was taken from Go when considering what not to include
in the language.
Comparisons always require two arguments and, with the exception of `<>` for
*not equals*, are operators are C-like.

## Subroutines (Functions)

The 6502 has builtin support for subroutines using the `JSR` (Jump to
Subroutine) and `RTS` (Return from Subroutine), however, it doesn't support
parameters in the programming language sense of the word. A2 adds support for
input and output parameters.

```
; Println writes the NUL-terminated msg pointed at by PTR along with a carriage
; return and outputs the number of characters written, n.
use Println: sub
    <- [msg: char^ @ PTR]
    -> [n: byte]
```

### Lack of automatic storage

It is vital to note that local variables (including arguments) do not have
automatic storage like in C. This means that recursion is not generally
supported; rather, it can be done, but you have to manage your own stack.
This was done to make it easier to access locals when writing inline assembly.

When implementing a subroutine in assembly, you can access the return value by
using its qualified name, such as in: `LDA Println.n`.
Furthermore, the 6502 stack is hardwired to be the First Page (`$100-1FF`) and is therefore only 256 bytes, so the recursion depth would be seriously limited.

## Arithmetic

Perhaps the most tedious aspect of A2, relative to higher-level languages, is
basic arithmetic. A2 does not support complex statements; you must split these
up

```
; Most languages: Z = 42 + B - C
; A2 be like:
Z := 42
Z += B
Z -= C
```

This is because the 6502 has a single register that supports arithmetic
operations, the Accumulator. Therefore, the language makes the user be explicit
about where they are loading and storing values from so as to pick the
appropriate addressing mode.

## Adding type information

Inspired by TypeScript, the language started out as a markup for 6502 assembly.
This can be seen in subroutine declarations.

Take the Apple II ROM subroutine `PRNTAX` found at `$F941`.
It "Prints A-Reg as Two-Digit Hex Number" followed by `X`.
This is declared in A2 as: `use PRNTAX: sub <- [val: word @ AX] @ $F941`.
It can then be called like: `PRNTAX($BEAD)`.

Normally, such assembly routines and their inputs and outputs are documented in
English prose or perhaps some arbitrary convention or not at all!
Arguably the most useful aspect of A2 is this formalization of declarations of
existing assembly code.


## Register-binding and Location-binding

The `PRNTAX` example illustrates both **register-binding** and
**location-binding**.
In short, a variable can _live_ at a fixed memory location or in a register.
For register-bound variables, you can use a single register—`A`, `X`, or `Y`—or
two registers like `AX` in which the most-significant byte is stored in `A` and
the other byte in `X`.

The compiler is aware of the binding and generates the appropriate operations.
One neat example is that an `X`-bound local variable being passed to an
`A`-bound parameter only needs one operation: `TXA`.
Such direct control of the underlying hardware should be used cautiously though;
it's very easy to clobber a register.


## Omitted Features

Like C, the language provides no facilities for input and output, no storage
allocation facilities, no heap, and no garbage collection. The expectation is
that there already exists routines for the system that need only be declared
and called by the A2 program.

## Benefits

Why would you use this language? I hope that it's quicker to write software
for the Apple II (and maybe other 6502-based machines like the Commander X16)
than writing in pure assembly, but that it also allows for super easy
integration with assembly when its needed.


### A note about the character type

On most systems, `byte` and `char` are essentially equivalent, but the Apple II
line of computers uses what is often called "High-ASCII" since the 8th-bit
needs to be set for a character to display normally in Apple II Text Mode.
In ASCII, an uppercase A is `$41` in hexadecimal, but in High-ASCII,
it is `$C1`.

Currently, the compiler treats the two types as a `byte`, but does output text
and character literals in High-ASCII.

## Tips

Since the compiler (`compile`) only translates A2 code into 6502 assembly, an
assembler is still needed convert that into machine binary. I've included the
`a2` bash script to make it less painful. That script will download [`a2asm`][]
from GitHub, build it, and run it.

[`a2asm`]: https://github.com/taeber/a2asm

Anyway, you can add an `a2` alias with tab completion for Bash by running:
```
$ eval $(./a2 bash)
$ a2 help
```

## Bugs

Bugs? Yeah, the compiler is buggy and only partially complete.
I very quickly ran out of time for this passion project.
Still, since `a2` can produce working 6502 binaries that run on my Apple //e, I
decided it was time to put it out there.

Feel free to file a GitHub issue and I'll try to fix it.
Or, you know, sponser me to work on it full time and I'll find all the bugs for you! ;-]


## See Also

* [a2asm](https://github.com/taeber/a2asm): a simple 6502 assembler I wrote in Go.
* [vim-a2](https://github.com/taeber/vim-a2): my VIM plugin for A2.
* [vim-peg](https://github.com/taeber/vim-peg): my VIM plugin for PEG.
* [apple2js](https://github.com/whscullin/apple2js): Apple II emulators written in JavaScript
* [Ample](https://github.com/ksherlock/ample): frontend for MAME as an Apple II emulator
