/*
 * Mini-SK, a S/K/I/B/C combinator reduction machine.
 *
 * Copyright 2020, Melissa O'Neill.  Distributed under the MIT License.
 *
 * This program implements an evaluator to evaluate combinator expressions, as
 * originally suggested by Moses Shoenfinkel in his 1924 paper _On the
 * Building Blocks of Mathematical Logic_, where:
 *
 *    (((S f) g) x) -> ((f x) (g x))    -- Fusion      [S]
 *    ((K x) y)     -> x                -- Constant    [C]
 *    (I x)         -> x                -- Identity    [I]
 *    (((B f) g) x) -> (f (g x))        -- Composition [Z] 
 *    (((C f) x) y) -> ((f y) x)        -- Interchange [T]
 *
 * The letters in square brackets are the ones used by Schoenfinkel.
 *
 * Mini-SK does not support the omitting implied parentheses, thus to evaluate
 * for example, S K K S, it must be entered as
 *
 *    (((S K) K) S)
 * or
 *    @@@SKKS
 *
 * In addition, the implementation supports placeholders a..z that can be
 * passed into expressions, and church numerals, entered as # followed by the
 * number, such as #10.  A number of pre-written expressions are provided,
 * enter as $name, such as $true.
 *
 * Mini-SK is designed to be simple and minimal, specifically targetting
 * “small” machines of the past.  The code is written in C89 (ANSI C) to allow
 * it to be compiled with older compilers on ancient systems.
 *
 * Optional defines
 *
 * -DTINY_VERSION
 *     Eliminate built-in values and other extraneous features to make a
 *     smaller executable for machines with limited memory.
 * -DUSE_MINILIB
 *     Under z88dk (Z80), Minilib provides an alternative crt and stdio
 *     library to minimize space usage.
 * -DDEBUG
 *     Produce voluminous debugging output.
 * -DNDEBUG
 *     Disable sanity checking and assert statements.
 *
 * Supported compilers and suggested command lines:
 *
 * Linux/macOS -- GCC & Clang
 *     clang -O3 -DNDEBUG -DMAX_APPS=32767 -DMAX_STACK=32767 -Wall -o mini-sk  mini-sk.c
 *
 * CP/M -- Z88DK
 *     zcc +cpm -DNDEBUG -SO3 --max-allocs-per-node500000 -startup=0 -clib=sdcc_iy mini-sk.c -o mini-sk -create-app
 *
 * CP/M -- Hi Tech C v3.09
 *     c -DNDEBUG -O mini-sk.c
 *
 * ZX Spectrum (16k) -- Z88DK & MINILIB
 *     zcc +zx -DUSE_MINILIB -DNDEBUG -DTINY_VERSION -SO3 --max-allocs-per-node500000 -clib=sdcc_ix --reserve-regs-iy -pragma-define:CRT_ZX_INIT=1 mini-sk.oc -o mini-sk -Ispectrum-minilib -Lspectrum-minilib -lmini -startup=" -1" -zorg:27136 -create-app
 * 
 * ZX Spectrum (48k) -- Z88DK & MINILIB
 *     zcc +zx -DUSE_MINILIB -DNDEBUG -SO3 --max-allocs-per-node500000 -clib=sdcc_ix --reserve-regs-iy -pragma-define:CRT_ZX_INIT=1 mini-sk.c -o mini-sk -Ispectrum-minilib -Lspectrum-minilib -lmini -startup=" -1" -zorg:31232 -create-app
 *
 * ZX Spectrum (48k) -- Z88DK
 *     zcc +zx -DNDEBUG  --max-allocs-per-node500000 -SO3 -startup=8 -clib=sdcc_iy mini-sk.c -o mini-sk -zorg:24064 -pragma-define:CRT_ITERM_INKEY_REPEAT_START=8000 -pragma-define:CRT_ITERM_INKEY_REPEAT_RATE=250 -pragma-redirect:CRT_OTERM_FONT_FZX=_ff_dkud1_Sinclair -create-app
 *
 * ZX Spectrum Next -- Z88DK & MINILIB
 *     zcc +zxn -no-cleanup -DUSE_MINILIB -DNDEBUG -SO3 -clib=sdcc_ix --reserve-regs-iy -pragma-define:CRT_ZXN_INIT=1 mini-sk.c -o mini-sk -Ispectrum-minilib -Lspectrum-minilib -lmini -startup=" -1" -zorg:30720 -create-app
 * 
 * ZX Spectrum Next -- Z88DK
 *     zcc +zxn -DNDEBUG --max-allocs-per-node500000 -SO3 -startup=8 -clib=sdcc_iy mini-sk.c -o mini-sk -zorg:24064 -pragma-define:CRT_ITERM_INKEY_REPEAT_START=8000 -pragma-define:CRT_ITERM_INKEY_REPEAT_RATE=250 -pragma-redirect:CRT_OTERM_FONT_FZX=_ff_dkud1_Sinclair -create-app
 *
 */

#ifdef USE_MINILIB
#include <minilib.h>
#else
#include <stdio.h>
#endif
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#ifdef HI_TECH_C
#define const
#define signed
typedef unsigned short uint16_t;
typedef unsigned char uint8_t;
#else
#include <stdint.h>
#endif

#ifdef __Z88DK
#ifdef __SPECTRUM
#include <arch/zx.h>
#endif
#else
#define __z88dk_fastcall
#endif

#define ARRAY_SIZE(array) sizeof(array)/sizeof(*(array))

/* Note, we can't use varargs macros in this vanilla C89-style code */
#ifdef DEBUG
#define debug_printf(args) printf args
#else
#define debug_printf(args)
#endif

#ifdef TINY_VERSION
/*
 * No switchable I/O.
 */

#define getch()    getchar()
#define ungetch(c) ungetchar(c)

#else
/*
 * Switchable I/O.
 *
 * We can read either from stdin or from memory.
 */

void* input_context;

short fgetchar(void)
{
    return (short)getc(input_context);
}

short fungetchar(char c)
{
    return (short)ungetc(c, (FILE*) input_context);
}

short sgetchar(void)
{
    char c = *(*((char**)input_context))++;
    if (c == '\0')
	return -1;
    else
	return c;
}

short sungetchar(char c) 
{
    char* cp = --(*((char**)input_context));
    assert(*cp == c);
    return 1;
}

short(*getch)(void) = fgetchar;
short(*ungetch)(char c) = fungetchar;
#endif

typedef unsigned short literal;

/* An atom is a 16-bit value that is either a 15-bit integer (i.e., it holds a
 * literal), typically representing a combinator, or it is a reference to an
 * app node (an app). We use different representations for different systems.
 */
#ifdef CPM
/*
 * On CP/M, we assume that the apps array will line in the low 32 KB of RAM.
 * If it overflows a little, it will limit the size of integers we can
 * represent.
 */
struct app_node;
typedef struct app_node* atom;
#define IS_LIT(x) (((unsigned short) x) >= ((unsigned short) &apps[MAX_APPS]))
#define NODE_FUNC(n)     ((n)->func)
#define NODE_ARG(n)      ((n)->arg)
#define NODE_REFCOUNT(n) ((n)->refcount)
#define INDEX_TO_ATOM(i) &apps[i]
#define LIT_TO_ATOM(l)   ((atom) (((unsigned short) l)+(unsigned short) &apps[MAX_APPS]))
#define ATOM_TO_LIT(a)   ((unsigned short) (a) - (unsigned short) &apps[MAX_APPS])
#define LIT_REQARGS(l)   ((unsigned char) ((l) >> 8))
#define LIT_SUBTYPE(l)   ((unsigned char) (l))

#else

#ifdef USE_MINILIB
/* This version intended for the ZX Spectrum.  For the normal version, it is
 * required that apps lies in the high 32K of memory, as we use the high bit
 * set to detect app nodes.  (This rule is relaxed for the TINY_VERSION
 * version, since it is intended for the 16K Spectrum which does not have a
 * high 32K of memory, but at the cost of only allowing 14-bit literals.)
 */
struct app_node;
typedef struct app_node* atom;
#define NODE_FUNC(n)     ((n)->func)
#define NODE_ARG(n)      ((n)->arg)
#define NODE_REFCOUNT(n) ((n)->refcount)
#define INDEX_TO_ATOM(i) &apps[i]
#ifndef TINY_VERSION
#define IS_LIT(x) (!(((unsigned short) x) & 0x8000))
#else
#define IS_LIT(x) (((unsigned short) x) < 0x4000)
#endif
#define LIT_TO_ATOM(l)   ((atom) (l))
#define ATOM_TO_LIT(a)   ((unsigned short) (a))
#define LIT_REQARGS(l)   ((unsigned char) ((l) >> 8))
#define LIT_SUBTYPE(l)   ((unsigned char) (l))

#else
/* This version should run on anything. It uses array indexing rather than
 * pointers.  As such, the address calculations are more complex (but
 * can utilize fancy addressing modes on the x86).  It also has the advantage
 * that it can handle 32768 app nodes, whereas the pointers approach can
 * handle at most 5461 (i.e., 2^15/sizeof(struct app_node)).
 */
typedef uint16_t atom;
#define IS_LIT(x) ((x & 0x8000) == 0)
#define NODE_FUNC(n) apps[(n) & 0x7fff].func
#define NODE_ARG(n)  apps[(n) & 0x7fff].arg
#define NODE_REFCOUNT(n) apps[(n) & 0x7fff].refcount
#define INDEX_TO_ATOM(i) ((i) | 0x8000)
#define LIT_TO_ATOM(l)   (l)
#define ATOM_TO_LIT(a)   (a)
#define LIT_REQARGS(l)   ((unsigned char) ((l) >> 8))
#define LIT_SUBTYPE(l)   ((unsigned char) (l))
#endif
#endif

/*
 * Literals representing the provided combinators are represented using
 * a coding where the high byte represents the required number of arguments
 * and the low byte represents the operation number and corresponds to an
 * entry in the reducers array declared later in the file.
 */

#define LIT_I   0x0100
#define LIT_K   0x0201
#define LIT_S   0x0302
#define LIT_B   0x0303
#define LIT_C   0x0304
#define LIT_Y   0x0105
#define LIT_P   0x0206
#define LIT_pl  0x0307
#define LIT_mi  0x0308
#define LIT_tm  0x0309
#define LIT_dv  0x030a
#define LIT_F   0x020b  /* (K I) */
#define LIT_J   0x020c  /* (C I) */
#define LIT_eq  0x030d
#define LIT_lt  0x030e
#define LIT_G   0x010f
#define LIT_END 0x0400

struct repr {
    char key;
    literal value;
};

struct repr reps[] = {
    {'I', LIT_I},
    {'K', LIT_K},
    {'S', LIT_S},
    {'B', LIT_B},
    {'C', LIT_C},
    {'Y', LIT_Y},
    {'P', LIT_P},
    {'+', LIT_pl},
    {'-', LIT_mi},
    {'*', LIT_tm},
    {'/', LIT_dv},
    {'F', LIT_F},
    {'J', LIT_J},
    {'=', LIT_eq},
    {'<', LIT_lt},
    {'G', LIT_G}
};

struct app_node {
    atom func;
    atom arg;
#ifdef TINY_VERSION
    uint8_t refcount;
#else
    uint16_t refcount;
#endif
};

#ifndef MAX_APPS
#ifndef TINY_VERSION
#define MAX_APPS 3072
#else
#define MAX_APPS 525
#endif
#endif

#define NOT_REDUCED ((atom) 0xFFFF)

#ifdef NDEBUG

#define SANITY_CHECK 
#define SANITY_CHECKING(x) 

#else

#define SANITY_CHECK assert(NODE_REFCOUNT(INDEX_TO_ATOM(MAX_APPS)) == 0x9e37);\
    assert(!IS_LIT(app_freelist));
#define SANITY_CHECKING(x) x

#endif


#ifdef USE_MINILIB
/* Must ensure this is the last variable in memory */
extern char beyond_end;
#define apps ((struct app_node*) (&beyond_end))
#else
static struct app_node apps[MAX_APPS+1];
#endif
atom app_freelist;

static unsigned int reductions = 0;

atom alloc_app(atom func, atom arg);
char free_app_all(atom a) __z88dk_fastcall;
atom copy_atom(atom a) __z88dk_fastcall;

void init_apps(void)
{
    uint16_t i;
    app_freelist = INDEX_TO_ATOM(0);
    for (i = 0; i < MAX_APPS; ++i) {
	atom i_atom = INDEX_TO_ATOM(i);
	NODE_FUNC(i_atom) = INDEX_TO_ATOM(i+1);
	SANITY_CHECKING(NODE_REFCOUNT(i_atom) = 0x8888;)
    }
    SANITY_CHECKING(NODE_REFCOUNT(INDEX_TO_ATOM(MAX_APPS)) = 0x9e37;)
    SANITY_CHECK
}

atom reduce(atom curr) __z88dk_fastcall;

uint8_t print_reduced = 0;

void print_lit(literal lit) __z88dk_fastcall
{
    unsigned char i;
    i = LIT_SUBTYPE(lit);
    if (i < ((unsigned char) ARRAY_SIZE(reps))
	&& LIT_REQARGS(reps[i].value) == LIT_REQARGS(lit)) {
	putchar(reps[i].key);
    } else {
	if (lit >= 32 && lit < 127) {
	    putchar('\'');
	    putchar(i);
	} else {
	    printf("%u", lit);
	}
    }
}

void print_atom(atom a) __z88dk_fastcall
{
    if (IS_LIT(a)) {
	print_lit(ATOM_TO_LIT(a));
    } else {
	assert(NODE_REFCOUNT(a) != 0x8888);
	assert(NODE_REFCOUNT(a) != 0x9e37);
        assert(NODE_REFCOUNT(a) > 0);

	putchar('(');
	print_atom(NODE_FUNC(a));
	putchar(' ');
	if (print_reduced && IS_LIT(NODE_FUNC(a)) 
	    && LIT_REQARGS(ATOM_TO_LIT(NODE_FUNC(a))) == 0) {
	    NODE_ARG(a) = reduce(NODE_ARG(a));
	}
	print_atom(NODE_ARG(a));
	putchar(')');
    }
}

void print_atom_reduced(atom a) __z88dk_fastcall
{
    print_reduced = 1;
    print_atom(a);
    print_reduced = 0;
}

uint16_t current_apps = 0;
uint16_t max_apps = 0;

#define App alloc_app

atom read_atom();

#ifndef TINY_VERSION
atom string_to_atom(const char *cp) __z88dk_fastcall
{
    atom a;
    void* saved_context = input_context;
    short(*saved_getch)(void) = getch;
    short(*saved_ungetch)(char c) = ungetch;
    /*  printf("input_context := %p, (was %p)\n", input_context, &cp); */
    input_context = (void*) &cp;
    getch = sgetchar;
    ungetch = sungetchar;
    a = read_atom();
    input_context = saved_context;
    getch = saved_getch;
    ungetch = saved_ungetch;
    return a;
}

const char* builtins[][2] = {
    { "t", "K" },
    { "f", "F" },
    { "and", "@@CCF" },
    { "or", "@JK" },
    { "not", "@@C@JFK" },
    { "pair", "@@BCJ" },
    { "fst", "@JK" },
    { "snd", "@JF" },
    { "succ", "@SB" },
    { "pred", "@@C@@BC@@B@BC@@C@@BC@@B@BB@@CB@@B@BJJKI" },
    { "iszero", "@@C@J@KFK" },
    { "plus", "@@BS@BB" },
    { "sub", "@@C@@BB@@C@J@J@@BJ@SB@JF@@B@S@@C@J@@B@C@@BBS@@B@S@@BBB@@B@BCC@KF@@C@@BB@J@@C@JKI@@C@J@@BKJK" },
    { "times", "B" },
    { "div2", "@@BC@@C@@BC@@C@@BB@J@@B@SBC@@BKKI" },
    { "cdiv", "@@C@@BB@@C@J@J@@BJ@SB@JF@@B@S@@C@J@J@@BJ@@BKJ@JK@@C@@BC@@BJ@@B@B@C@@BBS@@B@B@S@@BBB@@B@B@BC@@B@BC@@B@CB@@C@@BB@J@@C@JKI@@BKJ@KF" },
    { "fdiv", "@@B@B$pred@@B$ceiling$div$succ" },
    { "divrem2", "@@C@J@J@@C@@BS@@B@B$pair@@S@@BC@@BJ$succ I$not@@$pair#0$f" },
    { "tobinle", "@Y@@B@C$divrem2@@B@B@C$cons@S@@C$iszero$nil" },
    { "tobinbe", "@@B$rev$tobinle" },
    { "eq", "@@C@@BC@@C@@BC@@C@@BB@J@@C@J@@@SII@@BK@@BJ@@SIII@@C@J@@BKJKK@KF" },
    { "lesseq", "@@B@B$iszero$sub" },
    { "less", "@@B@B$not@@B@B$iszero@C$sub" },
    { "greatereq", "@C$lesseq" },
    { "greater", "@C$less" },
    { "cons", "$pair" },
    { "nil", "@KK" },
    { "hd", "$fst" },
    { "tl", "$snd" },
    /*  { "case", "@@S@@BC@@B@BB@J@K@KFI" }, */
    { "case", "@@C@@BC@@B@BC@@BC@@CB@@B@B@BK@B@BKI" },
    { "take", "@@C@@BC@@C@@BC@@C@@BB@J@@SI@@C@@BC@@B@BC@C@@BC@@BJ@@B@B@BK@@B@B@BK@@B@BC@@B@BJ@@C@@BBB@@BCJI@C@JIK@KK" },
    { "drop", "@J$tl" },
    { "nth", "@@B@B$hd$drop" },
    { "zipwith", "@Y@@B@B@C@@BB@@C$case$nil@@B@B@C@@BB@@BB@@C$case$nil@S@@BC@@B@BB@@B@BC@@B@B@BB@B@B$cons" },
    { "zipapp", "@Y@@B@C@@BB@@C$case$nil@@B@C@@BB@@BB@@C$case$nil@C@@BB@@BC@@B@BB@B$cons" },
    { "zip", "@$zipwith$pair" },
    { "last", "@$foldr1F" },
    { "isempty", "@J@K@KF" },
    { "length", "@@$foldr@K$succ#0" },
    { "foldl", "@@BY@@B@B@S@@BC@C$case@C@@BBB" },
    { "foldl1", "@@C@@BS@@C@@BB$foldl$hd$tl" },
    { "foldr", "@@B@BY@@B@C@@BB@@BC@C$case@@BC@BB" },
    { "foldr1", "@@BY@@B@BJ@@B@B@S@@BS@C$isempty@@BC@BB" },
    { "map", "@@BY@@B@B@C@@C$case$nil@@BC@@B@BB@B$cons" },
    { "filter", "@@BY@@B@B@C@@C$case$nil@@BC@@B@BB@@C@@BC@@CS$cons I" },
    { "append", "@Y@@B@C@@BS$case@@B@B@C@@BB$cons C" },
    { "partition", "@Y@@B@B@S@@C@J@K@KF@@C@J@KK@KK@@B@BJ@@C@@BS@@B@BB@BC@@C@@BS@@B@BS@@B@B@BS@@C@@BS@@B@BB@BB@@B@BC@@B@BJ@@BCJ@@B@C@@BB@@BCJ@@BCJ" },
    { "quicksort", "@@BY@@B@B@C@@C$case@KK@@C@@BB@@BS@@B@BC@B$partition@@S@@BB@@BB@@BC@B$append@C@@BB$cons" },
    { "rev", "@@$foldl@C$cons$nil" },
    { "natsfrom", "@Y@@B@S$cons@@CB$succ" },
    { "sum", "@@$foldr$plus#0" },
    { "neval", "@@C@@C@J@@CB@SBIF" },
    { "leval", "@@B$rev$rev" },
    { "exlist1", "@@$cons#0@@$cons#1@@$cons#2$nil" },
    { "exlist2", "@@$cons#2@@$cons#0@@$cons#7@@$cons#5@@$cons#1@@$cons#3@@$cons#6$nil" },
    { "fib", "@@C@@C@J@@S@@BC@@BJ@JF@@S@@BS@@B@BB@JK@JF@@C@JFIK" },
    { "fact", "@@C@@C@J@@B@SB@@CB@SBFI" },
    { "tnpo", "@@B@Y@@BJ@@C@@BC@@B@BC@@B@C@@BB@J@@CB@SB@@B@S@@BS@C@@C@@C@@C@J@@BKJK#0@@C@JK@K#0@@C@@BBB@@B@C@@BC@@BJ@@S@@S@@C@J@@C@J#0KK@@BC@@C@@BC@@C@@BB@J@@B@SBC@@BKKI@@B@SB@@S@@BS@BB@@S@@BS@BBI@SB#0@@C@@BCJ#0" },
    { "blc", "@Y@@B@BJ@@B@B@B@SI@@S@@BS@@B@BC@@B@B@BB@@B@B@BS@@B@B@CB@@S@@BBB@@B@S@@BC@@B@BS@@B@CB@@CB@@C@@BBB@C$pair@@C@@BBB@@C@@BBBS@@B@S@@BB@@BS@@B@SI@@CBJ@@B@B@B@BK@@B@BC@@C@@BBB@@C@@BBB@@B@CBJ" },
    { "runblc", "@$blc K" },
    { "rjot", "@Y@@B@C@@C$case I@@S@@BC@@B@BS@@B@CB@@B@BS@BK@@C@@BC@@CCSK" },
    { "jot", "@@B$rjot$rev" },
    { "diag", "@@C@@BC@@B@BY@@C@@BC@@B@BB@@B@BS@@B@B@B$append@@C@@BS@@B@BB$zipwith@@B@B$rev@C$take@@CB$succ I" },
    { "diagapp", "@@C@@BY@@C@@BB@@BS@@B@B$append@@S@@BB$zipapp@@B@B$rev@C$take@@CB$succ I" },
    { "allsk", "@Y@@B@$cons K@@B@$cons S$diagapp" },
    { "allski", "@Y@@B@$cons I@@B@$cons K@@B@$cons S$diagapp" },
    { "allskibc", "@Y@@B@$cons I@@B@$cons K@@B@$cons B@@B@$cons C@@B@$cons S$diagapp" }
};
#endif

atom read_atom()
{
    signed char c;
again:
    SANITY_CHECK
    c = getch();
    if (c == -1)
	return LIT_TO_ATOM(LIT_I);
    switch (c) {
    case ' ':
    case ')':
    case '\n':
	goto again;
    case '(':
    case '@': {
	atom lhs = read_atom();
	atom rhs = read_atom();
	return alloc_app(lhs,rhs);
    }
    case '\'':
	return LIT_TO_ATOM((unsigned char) getch());
    case '#': {
        short n = 0;
	for (;;) {
	    c = getch();
	    if (c < '0' || c > '9')
		break;
	    n = n*10+(c - '0');
	}
	if (c != -1)
	    ungetch(c);
	/*  printf("inserted church numeral %d: ",n); */
	{
	    atom single_succ = alloc_app(LIT_TO_ATOM(LIT_S),
					 LIT_TO_ATOM(LIT_B));
	    atom val = alloc_app(LIT_TO_ATOM(LIT_K),LIT_TO_ATOM(LIT_I));
	    short i;
	    for (i = 0; i < n; ++i) {
		val = alloc_app(copy_atom(single_succ),val);
	    }
	    free_app_all(single_succ);
	    /* print_atom(val); putchar('\n'); */
	    return val;
	}
    }
#ifndef TINY_VERSION
    case '$': {
        char ident[20];
	char* cp = ident;
	short i;
	for (;;) {
	    c = getch();
	    if (c < '0' || c > 'z' || (c < 'A' && c > '9'))
		break;
	    *cp++ = c;
	}
	if (c != -1) 
	    ungetch(c);
	*cp = '\0';
	for (i = 0; i < sizeof(builtins)/sizeof(*builtins); ++i) {
	    if (!strcmp(ident,builtins[i][0]))
		return string_to_atom(builtins[i][1]);
	}
	printf("Unkown macro: %s\n", ident);
	goto again;
    }
#endif
    default:
	if (c >= '0' && c <= '9') {
	    unsigned short num = 0;
	    for (;;) {
		num += c - '0';
		c = getch();
		if (c < '0' || c > '9')
		    break;
		num *= 10;
	    }
	    if (c != -1)
		ungetch(c);
	    return LIT_TO_ATOM(num & 0x7fff);
	}
	if (c >= 'a' && c <= 'z')
	    return LIT_TO_ATOM(c);
	{
	    unsigned char i;
	    for (i = 0; i < (unsigned char) ARRAY_SIZE(reps); ++i)
		if (reps[i].key == (char) c)
		    return LIT_TO_ATOM(reps[i].value);
	}
	printf("Unrecognized char '%c'\n", c);
	goto again;
    }
}

int main()
{
#ifdef __Z88DK
#ifdef __ZXNEXT
    putchar(14);
#else
#ifdef __SPECTRUM
    zx_cls(PAPER_WHITE);
#endif
#endif
#endif
    init_apps();
#ifndef TINY_VERSION
    input_context = (void*) stdin;
#endif
    SANITY_CHECK
    printf("Mini-SK, combinators & more...\n");
#ifndef TINY_VERSION
    printf("\nPredefined macros");
    { 
	int i;
	char comma = ':';
	for (i = 0; i < sizeof(builtins)/sizeof(*builtins); ++i) {
	    printf("%c $%s", comma, builtins[i][0]);
	    comma = ',';
	}
    }
    putchar('\n');
#endif
    for(;;) {
	atom a;
	if (feof(stdin))
	    break;
	SANITY_CHECK
	reductions = 0;
	max_apps = current_apps;
#if defined(USE_MINILIB) && defined(__SPECTRUM)
	input_prompt = "Term> ";
#else
	printf("\nTerm> ");
#endif
	a = read_atom();
	putchar('\n');
	{
	    char c;
	    do {
		c = getch();
	    } while (c == ' ' || c == ')');
	    if (c != '\n')
		ungetch(c);
	}
	print_atom(a); printf("\n--->\n");
    SANITY_CHECK
#if defined(USE_MINILIB) && defined(__SPECTRUM)
	input_prompt += 4;
#endif
	a = reduce(a);
    SANITY_CHECK
	print_atom_reduced(a); putchar('\n');
    SANITY_CHECK
	printf("\n%u reductions, %d max appnodes\n", reductions, max_apps);
	free_app_all(a);
    }
    return 0;
}


atom alloc_app(atom func, atom arg)
{
    atom next_app = app_freelist;
    SANITY_CHECK
    if (next_app == INDEX_TO_ATOM(MAX_APPS)) {
	fprintf(stderr, "out of app space\n");
	exit(2);
    }
    assert(NODE_REFCOUNT(next_app) == 0x8888);
    app_freelist = NODE_FUNC(next_app);
    NODE_FUNC(next_app) = func;
    NODE_ARG(next_app) = arg;
    NODE_REFCOUNT(next_app) = 1;
    debug_printf(("# ALLOC: node= %04x, lhs= %04x, rhs= %04x\n", next_app, func, arg));
    SANITY_CHECK
    ++current_apps;
    if (current_apps > max_apps)
	max_apps = current_apps;
    return next_app;
}

void free_app(atom app) __z88dk_fastcall
{
    SANITY_CHECK
    assert(NODE_REFCOUNT(app) != 0x8888);
    NODE_FUNC(app) = app_freelist;
    app_freelist = app;
    --current_apps;
    SANITY_CHECKING(NODE_REFCOUNT(app) = 0x8888;)
    debug_printf(("# FREE: node= %04x, lhs= %04x, rhs= %04x\n", app, NODE_FUNC(app), NODE_ARG(app)));
}

char free_app_all(atom app) __z88dk_fastcall
{
    SANITY_CHECK
    if (IS_LIT(app))
	return 0;
    debug_printf(("# DEC: node= %04x, lhs= %04x, rhs= %04x\n", app, NODE_FUNC(app), NODE_ARG(app)));
    if (--NODE_REFCOUNT(app))
	return 0;
    free_app_all(NODE_ARG(app));
    free_app_all(NODE_FUNC(app));
    free_app(app);
    return 1;
}

atom copy_atom(atom a) __z88dk_fastcall
{
    SANITY_CHECK
    if (IS_LIT(a))
	return a;
    debug_printf(("# INC: node= %04x, lhs= %04x, rhs= %04x\n", a, NODE_FUNC(a), NODE_ARG(a)));
    assert(NODE_REFCOUNT(a) != 0x8888);
    assert(NODE_REFCOUNT(a) != 0x9e37);
    ++NODE_REFCOUNT(a);
    return a;
}

atom replace(atom orig, atom reduced)
{
    if (!free_app_all(orig)) {
	copy_atom(reduced);
	free_app_all(NODE_FUNC(orig));
	free_app_all(NODE_ARG(orig));
	NODE_FUNC(orig) = LIT_TO_ATOM(LIT_I);
	NODE_ARG(orig)  = reduced;
    }
    return reduced;
}

#ifndef MAX_STACK
#define MAX_STACK 512
#endif

#ifdef USE_MINILIB
#ifdef TINY_VERSION
atom* rs_top_ptr = (atom*) 0x8000;
const atom* red_stack  = ((atom*) 0x7000);
#else
atom* rs_top_ptr = (atom*) 0xff20;
const atom* red_stack  = ((atom*) 0xef20);
#endif
#else
atom red_stack[MAX_STACK+1];
atom* rs_top_ptr = &red_stack[MAX_STACK];
#endif

typedef atom (*reducer_fn)(atom curr) __z88dk_fastcall;

atom red_ident(atom curr) __z88dk_fastcall
{
    return replace(curr, copy_atom(NODE_ARG(curr)));
}

atom red_const(atom curr) __z88dk_fastcall
{
    return replace(curr, copy_atom(NODE_ARG(rs_top_ptr[0])));
}

atom red_false(atom curr) __z88dk_fastcall
{
    return replace(curr, copy_atom(NODE_ARG(curr)));
}

atom red_jump(atom curr) __z88dk_fastcall
{
    atom yx = alloc_app(copy_atom(NODE_ARG(curr)), 
			copy_atom(NODE_ARG(rs_top_ptr[0])));
    return replace(curr, yx);
}

atom red_fusion(atom curr) __z88dk_fastcall
{
    atom fx = alloc_app(copy_atom(NODE_ARG(rs_top_ptr[0])),
			copy_atom(NODE_ARG(curr)));
    atom gx = alloc_app(copy_atom(NODE_ARG(rs_top_ptr[1])),
			copy_atom(NODE_ARG(curr)));
    return replace(curr,alloc_app(fx,gx));
}

atom red_compose(atom curr) __z88dk_fastcall
{
    atom f  = copy_atom(NODE_ARG(rs_top_ptr[0]));
    atom gx = alloc_app(copy_atom(NODE_ARG(rs_top_ptr[1])),
			copy_atom(NODE_ARG(curr)));
    return replace(curr,alloc_app(f,gx));
}

atom red_flip(atom curr) __z88dk_fastcall
{
    atom fy = alloc_app(copy_atom(NODE_ARG(rs_top_ptr[0])),
			copy_atom(NODE_ARG(curr)));
    atom x  = copy_atom(NODE_ARG(rs_top_ptr[1]));
    return replace(curr,alloc_app(fy,x));
}

atom red_y(atom curr) __z88dk_fastcall
{
    /*
     * Note: It migtht be tempting to write
     *     return replace(curr,alloc_app(copy_atom(NODE_ARG(curr)),
     *      				       copy_atom(curr))); 
     * but that would make a cycle, and that's not cool given that we use
     * reference countining.  Thus this is one of the few rules where we
     * don't use replace.
     */
    return alloc_app(copy_atom(NODE_ARG(curr)), curr);
}

atom red_putchar(atom curr) __z88dk_fastcall
{
    atom reduced = reduce(NODE_ARG(curr));
    NODE_ARG(curr) = reduced;
    putchar(IS_LIT(reduced) ? LIT_SUBTYPE(ATOM_TO_LIT(reduced)) : '*');
    return replace(curr,copy_atom(NODE_ARG(rs_top_ptr[0])));
}

atom red_getchar(atom curr) __z88dk_fastcall
{
    atom arg0 = NODE_ARG(curr);
    atom result = LIT_TO_ATOM(getchar());
    return replace(curr, alloc_app(copy_atom(arg0), result));
}

literal other_lit;

literal eval_two_lits(atom curr) __z88dk_fastcall
{
    atom reduced_lhs = reduce(NODE_ARG(rs_top_ptr[1]));
    NODE_ARG(rs_top_ptr[1]) = reduced_lhs;
    other_lit = IS_LIT(reduced_lhs) ? ATOM_TO_LIT(reduced_lhs) : 0;
    {
	atom reduced_rhs = reduce(NODE_ARG(curr));
	NODE_ARG(curr) = reduced_lhs;
	return  IS_LIT(reduced_rhs) ? ATOM_TO_LIT(reduced_rhs) : 0;
    }
    /* return (((uint32_t) lhs_lit) << 16) | rhs_lit; */
}

atom builtin_2c_result(atom result) __z88dk_fastcall
{
    atom arg0 = NODE_ARG(rs_top_ptr[0]);
    if (arg0 == LIT_TO_ATOM(LIT_I)) {
	return replace(rs_top_ptr[2], result);
    } else {
	return replace(rs_top_ptr[2], alloc_app(copy_atom(arg0), result));
    }
}

atom red_plus(atom curr) __z88dk_fastcall
{
    literal rhs_lit = eval_two_lits(curr);
    return builtin_2c_result(LIT_TO_ATOM((other_lit+rhs_lit) & 0x7fff));
}

atom red_minus(atom curr) __z88dk_fastcall
{
    literal rhs_lit = eval_two_lits(curr);
    return builtin_2c_result(LIT_TO_ATOM((other_lit-rhs_lit) & 0x7fff));
}

atom red_times(atom curr) __z88dk_fastcall
{
    literal rhs_lit = eval_two_lits(curr);
    return builtin_2c_result(LIT_TO_ATOM((other_lit*rhs_lit) & 0x7fff));
}

atom red_div(atom curr) __z88dk_fastcall
{
    literal rhs_lit = eval_two_lits(curr);
    return builtin_2c_result(LIT_TO_ATOM((other_lit/rhs_lit) & 0x7fff));
}

atom red_eq(atom curr) __z88dk_fastcall
{
    literal rhs_lit = eval_two_lits(curr);
    return builtin_2c_result(LIT_TO_ATOM(other_lit==rhs_lit ? LIT_K : LIT_F));
}

atom red_lt(atom curr) __z88dk_fastcall
{
    literal rhs_lit = eval_two_lits(curr);
    return builtin_2c_result(LIT_TO_ATOM(other_lit < rhs_lit ? LIT_K : LIT_F));
}

reducer_fn reducers[] = {
    red_ident,
    red_const,
    red_fusion,
    red_compose,
    red_flip,
    red_y,
    red_putchar,
    red_plus,
    red_minus,
    red_times,
    red_div,
    red_false,
    red_jump,
    red_eq,
    red_lt,
    red_getchar
};

atom reduce(atom curr) __z88dk_fastcall
{
    uint16_t stack_len;
    assert(rs_top_ptr >= red_stack);
    stack_len = 0;
    debug_printf(("# START: stack_len= %d, curr= %04x, rs_top_ptr= %p, red_stack= %p\n", stack_len, curr, rs_top_ptr, red_stack));
again:
    while(!IS_LIT(curr)) {
	atom next;
	assert(NODE_REFCOUNT(curr) != 0x8888);
	next = NODE_FUNC(curr);
	debug_printf(("# DOWN: stack_len= %d, curr= %04x, lhs= %04x, rhs= %04x, rs_top_ptr= %p\n", stack_len, curr, next, NODE_ARG(curr), rs_top_ptr));
	if (next == LIT_TO_ATOM(LIT_I)) {
	    next = curr;
	    do {
		debug_printf(("# INDIRECT1: next= %04x, lhs= %04x, rhs= %04x\n", next, NODE_FUNC(next), NODE_ARG(next)));
		next = NODE_ARG(next);
	    } while (!IS_LIT(next) 
		     && NODE_FUNC(next) == LIT_TO_ATOM(LIT_I));
	    debug_printf(("# INDIRECT3: next= %04x\n", next));
	    do {
		++reductions;
		copy_atom(next);
		if (free_app_all(curr)) {
		    curr = next;
		    break;
		}
		{
		    atom temp = NODE_ARG(curr);
		    NODE_ARG(curr) = next;
		    curr = temp;
		}
	    } while (!IS_LIT(curr)
			 && NODE_FUNC(curr) == LIT_TO_ATOM(LIT_I));
	    assert(curr == next);
	    if (stack_len > 0) {
		NODE_FUNC(*rs_top_ptr) = curr;
	    }
	    continue;
	}
	--rs_top_ptr;
	*rs_top_ptr = curr;
	++stack_len;
	debug_printf(("# DOWN2: stack_len= %d, curr= %04x, lhs= %04x, rhs= %04x, rs_top_ptr= %p, red_stack= %p\n", stack_len, curr, next, NODE_ARG(curr), rs_top_ptr, red_stack));
	assert(rs_top_ptr >= red_stack);
	curr = next;		
    }
    debug_printf(("# SELECT: stack_len = %d, curr = %04x, rs_top_ptr = %p\n", stack_len, curr, rs_top_ptr));
    {
    uint8_t reqargs;
    reqargs = LIT_REQARGS(ATOM_TO_LIT(curr));
    if (reqargs == 0)
	goto not_reduced;
    if (reqargs <= stack_len) {
	uint8_t subtype;
	debug_printf(("# ARGMATCH: stack_len= %u, reqargs= %u\n", stack_len, (short) reqargs));
	++reductions;
	subtype = LIT_SUBTYPE(ATOM_TO_LIT(curr));
	curr = rs_top_ptr[reqargs-1];
	curr = (reducers[subtype])(curr);
	rs_top_ptr += reqargs;
	stack_len -= reqargs;
	debug_printf(("# COMPLETE: stack_len = %d, curr = %04x, rs_top_ptr = %p\n", stack_len, curr, rs_top_ptr));
	if (stack_len > 0) {
	    NODE_FUNC(*rs_top_ptr) = curr;
	}
	goto again;
    }
    }
not_reduced:
    if (stack_len == 0)
	return curr;
    rs_top_ptr += stack_len;
    return *(rs_top_ptr - 1);
}


