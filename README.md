# Mini-SK

Mini-SK is provides a functional programming environment in the form
of an S/K/I/B/C combinator reduction machine. It specifically targets
“small” machines of the past including the ZX Spectrum with most data
sizes limited to 16 bits.  The code is written in C89 (ANSI C) to
allow it to be compiled with older compilers on ancient systems.  It
also runs happily on modern 32-bit and 64-bit computers, however.

Dispite its simplicity, Mini-SK provides many of the features we'd
normally expect from a functional language, including full laziness
(expressions are only evaluated when needed, and only once even if
they are shared).  Efficient tail recursion is intrinsically
supported.

Mini-SK is copyright 2020, Melissa O'Neill and distributed under the
MIT License.

## Overview

Expressing computation using combinator expressions was originally
suggested by [Moses Schönfinkel][1] in his 1924 paper [_On the
Building Blocks of Mathematical Logic_][2].  In that paper, he
suggested the following fundamental operators:
```
(((S f) g) x) -> ((f x) (g x))    -- Fusion      [S]
((K x) y)     -> x                -- Constant    [C]
(I x)         -> x                -- Identity    [I]
(((B f) g) x) -> (f (g x))        -- Composition [Z] 
(((C f) x) y) -> ((f y) x)        -- Interchange [T]
```

The letters in square brackets are the ones used by Schönfinkel.
Schönfinkel noted that S and K are suffient because they can implement
the others as follows:
```
((S K) K)         ≡ I
(((S (K f)) g) x) ≡ (((B f) g) x)
(((S f) (K x)) y) ≡ (((C f) x) y)
```
but in practice it is often preferable to use the full set as it avoids needless duplication of data, only to discard it with `K`.

Elsewhere in the literature, the many of the parentheses shown above
are omitted, writing `S K K S` instead of `(((S K) K) S)`.  In the
interests of keeping its parser as simple as possible, Mini-SK does
not support the omitting implied parentheses.  You can however, enter
omit redundant spaces, and enter applications using the `@` sign, thus
you may write the above as for example `@@@SKKS`.  You can mix both
notations, and in fact, `@` and `(` are synonyms, and close parentheses
are optional and ignored!

In addition, the implementation supports placeholders `a`..`z` that can be
passed into expressions to understand what they do and perform
computations, (thus `((K a) b)` reduces to `a`), and Church numerals,
entered as # followed by the number, such as `#10`.  A number of
pre-written expressions are provided as macros that are expanded by
the parser (except for the version for the 16K ZX Spectrum where they
are omitted to save space).  These macros are entered as enter as `$`
immediately followed by a descriptive name, such as `$t` for true, or
`$quicksort` for the QuickSort algorithm.

### Scope and Features

Although the S/K/I/B/C combinators may seem primitive, they are
sufficient to compute all computable functions.  In version 1.0 of
Mini-SK, these combinators are all that are supported, but a number of
predefined examples are available as convenience macros, allowing you
to experiment with lists, QuickSort, and even other languages such as
Jot without needing to type in complex and cryptic expressions.

Version 1.2 extends the minimal base by providing programmer-friendly
features, such as built-in binary numbers, and I/O facilities.  In
this version, the lower-case letter placeholders become just an
alternative way to express character constants, in other words `x` is
just another way of saying `'x`.

## Demos

This [video](https://vimeo.com/431042094) shows a demo of Mini-SK
running on a 16K ZX Spectrum (with approximately 8KB of usable
memory).

## Literals, Arithmetic and I/O

The combinators `S`, `K`, `I`, etc. are literals, but there are 32768
possible literals and all can be entered.  In version 1.2 and beyond,
characters are entered with a leading quote and then the next
character read is the literal even if it is whitespace.  Numbers can
also be entered in their usual decimal form.  

Because the printer used by the REPL knows nothing of what a literal
value is intended to mean, it adopts a heuristic approach for printing
literals: if the value matches the code used for a combinator it is
printed as that combinator, and if it is in the range of printable
ASCII, it is printed as a character literal, otherwise it is shown as
an integer.

Generally, except for the combinators, you should not attempt to
_apply_ a literal to another value and evaluate the result.  That
said, literal values in the range 0..255 are always safe to apply to
values -- they don't reduce and reduction will stop at that point.
However, to assist in understanding what functions do, in the REPL
when output results are printed, it _will_ reduce the arguments to
character/byte literals, causing `(Y 'x)` to expand to `(x (x (x
(x...` until it runs out of space.  Attempting to apply other numbers
will have unspecified results if the number does not correspond to an
actual combinator.

### Arithmetic

Arithmetic and comparison operators are provided for literals via the
combiators `+`, `-`, `*`, `/`, `=`, and `<`, however they are somewhat
unusual in that they take _three_ arguments, because the first
argument is a _continuation function_ that the result will be passed
to.

Thus, if you just want to add two and three, you can use the identity
function as a trivial continuation, this `(((+ I) 2) 3)`. You don't
write `((+ 2) 3)`! 

The reason for this continuation-based design is that in the simpler
design, `(K ((+ 2) 3))` would not reduce to `K 5` because of laziness,
but `(((+ K) 2) 3)` _does_ reduce to `(K 5)`. In other words, the
continuation-passing style allows you to control the extent to which
Mini-SK is lazy.

### I/O

The `G` (getchar) and `P` (putchar) combinators provide I/O.

* `(G c)` gets a character and passes it to continuation `c`.
* `(P c x)` puts a character `x` and continues with `c`.

### Examples of I/O and Arithmetic

The code below adds two numbers to produce the number `4254`:
```
(((+ I) 4200) 54)
```

This code converts a Church numeral for 729 (3^6) into its literal
form by using an increment function applied to zero
```
(((#6 #3) ((+ I) 1)) 0)
```
however, the above code uses space proportional to the size of the
number because laziness means that none of the additions are done
until the end and thus they are saved up.

In contrast, the code below uses the continuation-passing feature of
the addition operator to perform the addition as we go.
```
((((#6 #3) ((C +) 1)) I 0))
```

The code below gets a character from input and prints it out, and then
returns the identity function.
```
(G (P I))
```
and this version adds a newline afterwards
```
(G (P ((P I) 10)))
```

The code below runs forever, copying standard input to standard output:
```
(Y ((B G) P))
```
and this code (which can be entered as `@Y@@BG@C$cons`) turns input
into in infinite list of characters that can be read using `$hd`,
`$tail`, etc.
```
(Y ((B G) (C ((B C) J))))
```

In subsequent examples, we'll use the `@` shorthand for most applications.


The code below prints 243 (3^5) 'x's, followed by a newline (ASCII 10), and
returns the identity function as its result.
```
@@(#5 #3) @@CP'x @@PI10
```
whereas this version reads the character to be printed multiple times
from the user
```
@@G@@B(#5 #3) @CP @@PI10
```

Finally, this code outputs a number of 'x's counted not using Church
numerals but built in numbers.
```
@@@@@BY@@B@B@S@@C@@C@=I0I@@C@@BC@@B@BC@@B@B-@C@@BC@BP1'x243@@PI10
```
Note that this version has to go to more effort than the Church
numeral version, with explicit recursion, decrementing, and tests
against zero for completion.  It does, however, run in constant space
regardless of the size of the number.

### Additional Combinators

For convenience the system provides two additional combinators:

* `F` (false) is equivalent to `(K I)`, such that `((F x) y)` → `y`.
* `J` (jump) is equivalent to `(C I)`, such that `((J x) y)` → `(y x)`.

## Installing

Prebuilt binaries are provided with releases for ZX Spectrum variants and
for macOS and x86 Linux.

## Building

Optional defines

* `-DTINY_VERSION`

    Eliminate built-in values and other extraneous features to make a
    smaller executable for machines with limited memory.

* `-DUSE_MINILIB`

    Under z88dk (Z80), [MiniLib](https://github.com/imneme/spectrum-minilib)
    provides an alternative crt and stdio library to minimize space usage.

* `-DDEBUG`

    Produce voluminous debugging output.

* `-DNDEBUG`

    Disable sanity checking and assert statements.

## Supported compilers and suggested command lines

### Linux/macOS -- GCC & Clang
```
clang -O3 -DNDEBUG -DMAX_APPS=32767 -DMAX_STACK=32767 -Wall -o mini-sk  mini-sk.c
```
### CP/M -- Z88DK
```
zcc +cpm -DNDEBUG -SO3 --max-allocs-per-node200000 -startup=0 -clib=sdcc_iy mini-sk.c -o mini-sk -create-app
```

### CP/M -- Hi Tech C v3.09
```
c -DNDEBUG -O mini-sk.c
```

### ZX Spectrum (16k) -- Z88DK & MINILIB
```
zcc +zx -DUSE_MINILIB -DNDEBUG -DTINY_VERSION -SO3 --max-allocs-per-node200000 -clib=sdcc_ix --reserve-regs-iy -pragma-define:CRT_ZX_INIT=1 mini-sk.oc -o mini-sk -Ispectrum-minilib -Lspectrum-minilib -lmini -startup=" -1" -zorg:27136 -create-app
```

### ZX Spectrum (48k) -- Z88DK & MINILIB
```
zcc +zx -DUSE_MINILIB -DNDEBUG -SO3 --max-allocs-per-node200000 -clib=sdcc_ix --reserve-regs-iy -pragma-define:CRT_ZX_INIT=1 mini-sk.c -o mini-sk -Ispectrum-minilib -Lspectrum-minilib -lmini -startup=" -1" -zorg:31232 -create-app
```

### ZX Spectrum (48k) -- Z88DK
```
zcc +zx -DNDEBUG -SO3 --max-allocs-per-node200000 -startup=8 -clib=sdcc_iy mini-sk.c -o mini-sk -zorg:24064 -pragma-define:CRT_ITERM_INKEY_REPEAT_START=8000 -pragma-define:CRT_ITERM_INKEY_REPEAT_RATE=250 -pragma-redirect:CRT_OTERM_FONT_FZX=_ff_dkud1_Sinclair -create-app
```

### ZX Spectrum Next -- Z88DK & MINILIB
```
zcc +zxn -no-cleanup -DUSE_MINILIB -DNDEBUG -SO3 --max-allocs-per-node200000 -clib=sdcc_ix --reserve-regs-iy -pragma-define:CRT_ZXN_INIT=1 mini-sk.c -o mini-sk -Ispectrum-minilib -Lspectrum-minilib -lmini -startup=" -1" -zorg:30720 -create-app
```

### ZX Spectrum Next -- Z88DK
```
zcc +zxn -DNDEBUG --max-allocs-per-node200000 -SO3 -startup=8 -clib=sdcc_iy mini-sk.c -o mini-sk -zorg:24064 -pragma-define:CRT_ITERM_INKEY_REPEAT_START=8000 -pragma-define:CRT_ITERM_INKEY_REPEAT_RATE=250 -pragma-redirect:CRT_OTERM_FONT_FZX=_ff_dkud1_Sinclair -create-app
```

[1]: https://en.wikipedia.org/wiki/Moses_Schönfinkel "Wikipedia"
[2]: https://courses.engr.illinois.edu/cs522/sp2016/OnTheBuildingBlocksOfMathematicalLogic.pdf "English translation, PDF"