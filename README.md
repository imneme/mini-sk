# Mini-SK

Mini-SK, a S/K/I/B/C combinator reduction machine.

Copyright 2020, Melissa O'Neill.  Distributed under the MIT License.

This program implements an evaluator to evaluate combinator expressions, as
originally suggested by Moses Shoenfinkel in his 1924 paper _On the
Building Blocks of Mathematical Logic_, where:

```
(((S f) g) x) -> ((f x) (g x))    -- Fusion      [S]
((K x) y)     -> x                -- Constant    [C]
(I x)         -> x                -- Identity    [I]
(((B f) g) x) -> (f (g x))        -- Composition [Z] 
(((C f) x) y) -> ((f y) x)        -- Interchange [T]
```

The letters in square brackets are the ones used by Schoenfinkel.

Mini-SK does not support the omitting implied parentheses, thus to evaluate
for example, S K K S, it must be entered as
```
(((S K) K) S)
```
or
```
@@@SKKS
```

In addition, the implementation supports placeholders a..z that can be
passed into expressions, and church numerals, entered as # followed by the
number, such as `#10`.  A number of pre-written expressions are provided,
enter as $name, such as `$t` for true.

Mini-SK is designed to be simple and minimal, specifically targetting
“small” machines of the past.  The code is written in C89 (ANSI C) to allow
it to be compiled with older compilers on ancient systems.

## Demos

This is a demo of Mini-SK running on a 16K ZX Spectrum (with
approximately 8KB of usable memory).

<iframe src="https://player.vimeo.com/video/431042094" width="640" height="480" frameborder="0" allow="autoplay; fullscreen" allowfullscreen></iframe>

## Building

Optional defines

* `-DTINY_VERSION`

    Eliminate built-in values and other extraneous features to make a
    smaller executable for machines with limited memory.

* `-DUSE_MINILIB`

    Under z88dk (Z80), Minilib provides an alternative crt and stdio
    library to minimize space usage.

* `-DRECURSIVE_REDUCE`

    An alternative (easier to understand perhaps) implementation of
    reduce that uses recursion rather than iteration and an explicit
    stack.

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
zcc +cpm -DNDEBUG -SO3 --max-allocs-per-node500000 -startup=0 -clib=sdcc_iy mini-sk.c -o mini-sk -create-app
```

### CP/M -- Hi Tech C v3.09
```
c -v -DNDEBUG -O mini-sk.c
```

### ZX Spectrum (16k) -- Z88DK & MINILIB
```
zcc +zx -DUSE_MINILIB -DNDEBUG -DTINY_VERSION -SO3 --max-allocs-per-node500000 -clib=sdcc_ix --reserve-regs-iy -pragma-define:CRT_ZX_INIT=1 mini-sk.oc -o mini-sk -Iz88dk-minilib -Ld88dk-minilib -lmini -startup=" -1" -zorg:27136 -create-app
```

### ZX Spectrum (48k) -- Z88DK & MINILIB
```
zcc +zx -DUSE_MINILIB -DNDEBUG -SO3 --max-allocs-per-node500000 -clib=sdcc_ix --reserve-regs-iy -pragma-define:CRT_ZX_INIT=1 mini-sk.c -o mini-sk -I../Spectrum/APIs/spectrum-rom -L../Spectrum/APIs/spectrum-rom -lmini -startup=" -1" -zorg:31232 -create-app
```

### ZX Spectrum (48k) -- Z88DK
```
zcc +zx -DNDEBUG  --max-allocs-per-node500000 -SO3 -startup=8 -clib=sdcc_iy mini-sk.c -o mini-sk -zorg:24064 -pragma-define:CRT_ITERM_INKEY_REPEAT_START=8000 -pragma-define:CRT_ITERM_INKEY_REPEAT_RATE=250 -pragma-redirect:CRT_OTERM_FONT_FZX=_ff_dkud1_Sinclair -create-app
```

### ZX Spectrum Next -- Z88DK & MINILIB
```
zcc +zxn -no-cleanup -v -s -m -DUSE_MINILIB -DNDEBUG -SO3 -clib=sdcc_ix --reserve-regs-iy -pragma-define:CRT_ZXN_INIT=1 -s -m mini-sk.c -o mini-sk -I../Spectrum/APIs/spectrum-rom -L../Spectrum/APIs/spectrum-rom -lmini -startup=" -1" -zorg:30720 -create-app
```

### ZX Spectrum Next -- Z88DK
```
zcc +zxn -DNDEBUG --max-allocs-per-node500000 -SO3 -startup=8 -clib=sdcc_iy mini-sk.c -o mini-sk -zorg:24064 -pragma-define:CRT_ITERM_INKEY_REPEAT_START=8000 -pragma-define:CRT_ITERM_INKEY_REPEAT_RATE=250 -pragma-redirect:CRT_OTERM_FONT_FZX=_ff_dkud1_Sinclair -create-app
```
