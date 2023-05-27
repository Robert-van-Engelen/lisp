/* lisp.hpp Lisp in C++ with mark-sweep GC and NaN boxing by Robert A. van Engelen 2022 BSD-3 license
   This C++17 version encapsulates the entire Lisp interpreter in a single Lisp class */

#ifndef LISP_HPP
#define LISP_HPP

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <csetjmp>
#include <functional>

#ifdef HAVE_SIGNAL_H
#include <signal.h>             /* to catch CTRL-C and continue the REPL */
#endif

#ifdef HAVE_READLINE_H
#include <readline/readline.h>  /* for convenient line editing ... */
#include <readline/history.h>   /* ... and a history of previous Lisp input */
#else
inline void using_history() { }
#endif

/* floating point output format */
#define FLOAT "%.17lg"

/* DEBUG: always run GC when allocating cells and atoms/strings on the heap */
#ifdef DEBUG
#define ALWAYS_GC 1
#else
#define ALWAYS_GC 0
#endif

/* T(x) returns the tag bits of a NaN-boxed Lisp expression x */
#define T(x) (*(uint64_t*)&x >> 48)

/* Lisp class<P,S> parameterized with pool size P and stack/heap size S */
template<uint32_t P,uint32_t S> class Lisp {

/*----------------------------------------------------------------------------*\
 |      LISP EXPRESSION TYPES AND NAN BOXING                                  |
\*----------------------------------------------------------------------------*/

public:

typedef Lisp<P,S> This;

Lisp<P,S>() {
  A = reinterpret_cast<char*>(cell);
  fp = 0;                                       /* free pointer */
  hp = H;                                       /* heap pointer */
  sp = N;                                       /* stack pointer */
  tr = 0;                                       /* 0 when tracing is off, 1 or 2 to trace Lisp evaluation steps */
  out = stdout;                                 /* the file we are writing to, stdout by default */
  memset(used, 0, sizeof(used));                /* clear the 'used' bit vector */
  sweep();                                      /* clear the pool */
  nil = box(NIL, 0);                            /* set the constant nil (empty list) */
  tru = atom("#t");                             /* set the constant #t */
  env = pair(tru, tru, nil);                    /* create environment with symbolic constant #t */
  for (I i = 0; prim[i].s; ++i)                 /* expand environment with primitives */
    env = pair(atom(prim[i].s), box(PRIM, i), env);
  fin = 0;                                      /* no open files */
  see = '\n';                                   /* input line sentinel \n */
  ptr = "";                                     /* pointer to char in line, init to \0 end of line */
  line = NULL;                                  /* no line read */
  strcpy(ps, ">");                              /* prompt */
  break_on();                                   /* enable interrupt if compiled with -DHAVE_SIGINT_H */
}

~Lisp<P,S>() {
  break_default();                              /* reinstate CTRL-C default if compiled with -DHAVE_SIGINT_H */
  closein();                                    /* close all open input files */
}

/* we only need two types to implement a Lisp interpreter:
        I      unsigned integer (32 bit unsigned)
        L      Lisp expression (double with NaN boxing)
   I variables and function parameters are named as follows:
        i,j,k  any unsigned integer, e.g. a NaN-boxed ordinal value
        t      a NaN-boxing tag
   L variables and function parameters are named as follows:
        x,y    any Lisp expression
        n      number
        t,s    list
        f      function or Lisp primitive
        p      pair, a cons of two Lisp expressions
        e,d    environment, a list of pairs, e.g. created with (define v x)
        v      the name of a variable (an atom) or a list of variables */
typedef uint32_t I;
typedef double L;

protected:

/* primitive, atom, string, cons, closure, macro and nil tags for NaN boxing (reserve 0x7ff8 for nan) */
static const I PRIM = 0x7ff9, ATOM = 0x7ffa, STRG = 0x7ffb, CONS = 0x7ffc, CLOS = 0x7ffe, MACR = 0x7fff, NIL = 0xffff;

/* box(t,i): returns a new NaN-boxed double with tag t and ordinal i
   ord(x):   returns the ordinal of the NaN-boxed double x
   num(n):   convert or check number n (does nothing, e.g. could check for NaN)
   equ(x,y): returns nonzero if x equals y */
static L box(I t, I i) {
  L x;
  *(uint64_t*)&x = (uint64_t)t << 48 | i;
  return x;
}

static I ord(L x) {
  return *(uint64_t*)&x;        /* the return value is narrowed to 32 bit unsigned integer to remove the tag */
}

static L num(L n) {
  return n;                     /* this could check for a valid number: return n == n ? n : err(5); */
}

static I equ(L x, L y) {
  return *(uint64_t*)&x == *(uint64_t*)&y;
}

/*----------------------------------------------------------------------------*\
 |      ERROR HANDLING AND ERROR MESSAGES                                     |
\*----------------------------------------------------------------------------*/

public:

/* report and throw an exception */
#define ERR(n, ...) (fprintf(stderr, __VA_ARGS__), err(n))
static L err(int n) { throw n; }

/* return error string for error code or empty string */
static const char *error(int i) {
  switch (i) {
    case 1: return "not a pair";
    case 2: return "break";
    case 3: return "unbound symbol";
    case 4: return "cannot apply";
    case 5: return "arguments";
    case 6: return "stack over";
    case 7: return "out of memory";
    case 8: return "syntax";
    default: return "";
  }
}

#ifdef HAVE_SIGNAL_H

#define GETSIGINT(obj) setjmp((obj).jb);
static inline jmp_buf jb;
static void sigint(int i) { longjmp(jb, i); }                    /* cannot throw in sig handlers */
static void break_on() { signal(SIGINT, This::sigint); }
static void break_off() { signal(SIGINT, SIG_IGN); }
static void break_default() { signal(SIGINT, SIG_DFL); }

#else

#define GETSIGINT(obj) 0
static void break_on() { }
static void break_off() { }
static void break_default() { }

#endif

/*----------------------------------------------------------------------------*\
 |      MEMORY MANAGEMENT AND RECYCLING                                       |
\*----------------------------------------------------------------------------*/

public:

/* base address of the atom/string heap */
char *A;

/* Lisp constant expressions () (nil) and #t, and the global environment env */
L nil, tru, env;

/* garbage collector, returns number of free cells in the pool or raises err(7) */
I gc() {
  I i;
  break_off();                                  /* do not interrupt GC if compiled with -DHAVE_SIGINT_H */
  memset(used, 0, sizeof(used));                /* clear all used[] bits */
  if (T(env) == CONS)
    mark(ord(env));                             /* mark all globally-used cons cell pairs referenced from env list */
  for (i = sp; i < N; ++i)
    if ((T(cell[i]) & ~(CONS^MACR)) == CONS)
      mark(ord(cell[i]));                       /* mark all cons cell pairs referenced from the stack */
  i = sweep();                                  /* remove unused cons cell pairs from the pool */
  compact();                                    /* remove unused atoms and strings from the heap */
  break_on();                                   /* enable interrupt if compiled with -DHAVE_SIGINT_H */
  return i ? i : err(7);
}

/* push x on the stack to protect it from being recycled, returns pointer to cell pair (e.g. to update the value) */
L *push(L x) {
  cell[--sp] = x;                               /* we must save x on the stack so it won't get GC'ed */
  if (hp > (sp-1) << 3 || ALWAYS_GC) {          /* if insufficient stack space is available, then GC */
    gc();                                       /* GC */
    if (hp > (sp-1) << 3)                       /* GC did not free up heap space to enlarge the stack */
      err(6);
  }
  return &cell[sp];
}

/* pop from the stack and return value */
L pop() {
  return cell[sp++];
}

/* unwind the stack up to position i, where i=N clears the stack */
void unwind(I i = N) {
  sp = i;
}

protected:

/* total number of cells to allocate = P+S */
static const uint32_t N = P+S;

/* heap address start offset, the heap starts at address A+H immediately above the pool */
static const uint32_t H = sizeof(L)*P;

/* size of the cell reference field of an atom/string on the heap, used by the compacting garbage collector */
static const uint32_t R = sizeof(I);

/* array of Lisp expressions, shared by the pool, heap and stack */
L cell[N];

/* fp: free pointer points to free cell pair in the pool, next free pair is ord(cell[fp]) unless fp=0
   hp: heap pointer, A+hp points free atom/string heap space above the pool and below the stack
   sp: stack pointer, the stack starts at the top of cell[] with sp=N
   tr: 0 when tracing is off, 1 or 2 to trace Lisp evaluation steps */
I fp, hp, sp, tr;

/* bit vector corresponding to the pairs of cells in the pool marked 'used' (car and cdr cells are marked together) */
uint32_t used[(P+63)/64];

/* mark-sweep garbage collector recycles cons pair pool cells, finds and marks cells that are used */
void mark(I i) {
  while (!(used[i/64] & 1 << i/2%32)) {         /* while i'th cell pair is not used in the pool */
    used[i/64] |= 1 << i/2%32;                  /* mark i'th cell pair as used */
    if ((T(cell[i]) & ~(CONS^MACR)) == CONS)    /* recursively mark car cell[i] if car refers to a pair */
      mark(ord(cell[i]));
    if ((T(cell[i+1]) & ~(CONS^MACR)) != CONS)  /* if cdr cell[i+1] is not a pair, then break and return */
      break;
    i = ord(cell[i+1]);                         /* iteratively mark cdr cell[i+1] */
  }
}

/* mark-sweep garbage collector recycles cons pair pool cells, returns total number of free cells in the pool */
I sweep() {
  I i, j;
  for (fp = 0, i = P/2, j = 0; i--; ) {         /* for each cons pair (two cells) in the pool, from top to bottom */
    if (!(used[i/32] & 1 << i%32)) {            /* if the cons pair cell[2*i] and cell[2*i+1] are not used */
      cell[2*i] = box(NIL, fp);                 /* then add it to the linked list of free cells pairs as a NIL box */
      fp = 2*i;                                 /* free pointer points to the last added free pair */
      j += 2;                                   /* two more cells freed */
    }
  }
  return j;                                     /* return number of cells freed */
}

/* add i'th cell to the linked list of cells that refer to the same atom/string */
void link(I i) {
  I k = *(I*)(A+ord(cell[i])-R);                /* atom/string reference k is the k'th cell that uses the atom/string */
  *(I*)(A+ord(cell[i])-R) = i;                  /* add k'th cell to the linked list of atom/string cells */
  cell[i] = box(T(cell[i]), k);                 /* by updating the i'th cell atom/string ordinal to k */
}

/* compacting garbage collector recycles heap by removing unused atoms/strings and by moving used ones */
void compact() {
  I i, j;
  for (i = H; i < hp; i += strlen(A+R+i)+R+1)   /* reset all atom/string reference fields to N (end of linked list) */
    *(I*)(A+i) = N;
  for (i = 0; i < P; ++i)                       /* add each used atom/string cell in the pool to its linked list */
    if (used[i/64] & 1 << i/2%32 && (T(cell[i]) & ~(ATOM^STRG)) == ATOM)
      link(i);
  for (i = sp; i < N; ++i)                      /* add each used atom/string cell on the stack to its linked list */
    if ((T(cell[i]) & ~(ATOM^STRG)) == ATOM)
      link(i);
  for (i = H, j = hp, hp = H; i < j; ) {        /* for each atom/string on the heap */
    I k = *(I*)(A+i), n = strlen(A+R+i)+R+1;
    if (k < N) {                                /* if its linked list is not empty, then we need to keep it */
      while (k < N) {                           /* traverse linked list to update atom/string cells to hp+R */
        I l = ord(cell[k]);
        cell[k] = box(T(cell[k]), hp+R);        /* hp+R is the new location of the atom/string after compaction */
        k = l;
      }
      if (hp < i)
        memmove(A+hp, A+i, n);                  /* move atom/string further down the heap to hp+R to compact the heap */
      hp += n;                                  /* update heap pointer to the available space above the atom/string */
    }
    i += n;
  }
}

/*----------------------------------------------------------------------------*\
 |      LISP EXPRESSION CONSTRUCTION AND INSPECTION                           |
\*----------------------------------------------------------------------------*/

public:

/* allocate n+1 bytes on the heap, returns heap offset of the allocated space */
I alloc(I n) {
  I i = hp+R;                                   /* free atom/heap is located at hp+R */
  n += R+1;                                     /* n+R+1 is the space we need to reserve */
  if (hp+n > (sp-1) << 3 || ALWAYS_GC) {        /* if insufficient heap space is available, then GC */
    gc();                                       /* GC */
    if (hp+n > (sp-1) << 3)                     /* GC did not free up sufficient heap/stack space */
      err(6);
    i = hp+R;                                   /* new atom/string is located at hp+R on the heap */
  }
  hp += n;                                      /* update heap pointer to the available space above the atom/string */
  return i;
}

/* copy string s to the heap, returns heap offset of the string on the heap */
I copy(const char *s) {
  return strcpy(A+alloc(strlen(s)), s)-A;       /* copy string+\0 to the heap */
}

/* interning of atom names (symbols), returns a unique NaN-boxed ATOM */
L atom(const char *s) {
  I i = H+R;
  while (i < hp && strcmp(A+i, s))              /* search the heap for matching atom (or string) s */
    i += strlen(A+i)+R+1;
  if (i >= hp)                                  /* if not found, then copy s to the heap for the new atom */
    i = copy(s);
  return box(ATOM, i);                          /* return unique NaN-boxed ATOM */
}

/* store string s on the heap, returns a NaN-boxed STRG with heap offset */
L string(const char *s) {
  return box(STRG, copy(s));                    /* copy string+\0 to the heap, return NaN-boxed STRG */
}

/* construct pair (x . y) returns a NaN-boxed CONS */
L cons(L x, L y) {
  L p; I i = fp;                                /* i'th cons cell pair car cell[i] and cdr cell[i+1] is free */
  fp = ord(cell[i]);                            /* update free pointer to next free cell pair, zero if none are free */
  cell[i] = x;                                  /* save x into car cell[i] */
  cell[i+1] = y;                                /* save y into cdr cell[i+1] */
  p = box(CONS, i);                             /* new cons pair NaN-boxed CONS */
  if (!fp || ALWAYS_GC) {                       /* if no more free cell pairs */
    push(p);                                    /* save new cons pair p on the stack so it won't get GC'ed */
    gc();                                       /* GC */
    pop();                                      /* rebalance the stack */
  }
  return p;                                     /* return NaN-boxed CONS */
}

/* construct a pair to add to environment e, returns the list ((v . x) . e) */
L pair(L v, L x, L e) {
  return cons(cons(v, x), e);
}

/* construct a closure, returns a NaN-boxed CLOS */
L closure(L v, L x, L e) {
  return box(CLOS, ord(pair(v, x, equ(e, env) ? nil : e)));
}

/* construct a macro, returns a NaN-boxed MACR */
L macro(L v, L x) {
  return box(MACR, ord(cons(v, x)));
}

/* return the car of a cons/closure/macro pair; CAR(p) provides direct memory access */
#define CAR(p) cell[ord(p)]
L car(L p) {
  return (T(p) & ~(CONS^MACR)) == CONS ? CAR(p) : err(1);
}

/* return the cdr of a cons/closure/macro pair; CDR(p) provides direct memory access */
#define CDR(p) cell[ord(p)+1]
L cdr(L p) {
  return (T(p) & ~(CONS^MACR)) == CONS ? CDR(p) : err(1);
}

/* look up a symbol in an environment, returns its value */
L assoc(L v, L e) {
  while (T(e) == CONS && !equ(v, car(car(e))))
    e = cdr(e);
  return T(e) == CONS ? cdr(car(e)) : T(v) == ATOM ? ERR(3, "unbound %s ", A+ord(v)) : err(3);
}

/* Not(x) is nonzero if x is the Lisp () empty list */
I Not(L x) {
  return T(x) == NIL;
}

/* more(t) is nonzero if list t has more than one item, i.e. is not empty or a singleton list */
I more(L t) {
  return T(t) != NIL && (t = cdr(t), T(t) != NIL);
}

/*----------------------------------------------------------------------------*\
 |      READ                                                                  |
\*----------------------------------------------------------------------------*/

public:

/* specify an input file to parse and try to open it */
FILE *input(const char *s) {
  return fin <= 9 && (in[fin] = fopen(s, "r")) ? in[fin++] : NULL;
}

/* close all open input files */
void closein() {
  while (fin)
    fclose(in[--fin]);
}

/* return the Lisp expression parsed and read from input */
L read() {
  scan();
  return parse();
}

/* specify a REPL prompt, where the first %u shows free pool space and second %u shows free stack/heap space */
void prompt(const char *s) {
  I i = gc();
  snprintf(ps, sizeof(ps), s, i, sp-hp/8);
}

protected:

/* the file(s) we are reading or fin=0 when reading from the terminal */
I fin;
FILE *in[10];

/* tokenization buffer and the next character that we see */
char buf[256], see;

/* readline pointer into the last line */
const char *ptr, *line;

/* prompt string */
char ps[20];

/* return the character we see, advance to the next character */
char get() {
  int c, look = see;
  if (fin) {                                    /* if reading from a file */
    see = c = getc(in[fin-1]);                  /* read a character */
    if (c == EOF) {
      fclose(in[--fin]);                        /* if end of file, then close the file */
      see = '\n';                               /* pretend we see a newline at eof */
    }
  }
  else {
#ifdef HAVE_READLINE_H
    if (see == '\n') {                          /* if looking at the end of the current readline line */
      break_off();                              /* disable interrupt to prevent free() without final line = NULL */
      if (line)                                 /* free the old line that was malloc'ed by readline */
        free(const_cast<char*>(line));
      line = NULL;
      break_on();                               /* enable interrupt if compiled with -DHAVE_SIGINT_H */
      while (!(ptr = line = readline(ps)))      /* read new line and set ptr to start of the line */
        freopen("/dev/tty", "r", stdin);        /* try again when line is NULL after EOF by CTRL-D */
      add_history(line);                        /* make it part of the history */
      strcpy(ps, "?");                          /* change prompt to ? */
    }
    if (!(see = *ptr++))                        /* look at the next character in the readline line */
      see = '\n';                               /* but when it is \0, replace it with a newline \n */
#else
    if (see == '\n') {
      printf("%s", ps);
      strcpy(ps, "?");
    }
    if ((c = getchar()) == EOF) {
      freopen("/dev/tty", "r", stdin);
      c = '\n';
    }
    see = c;
#endif
  }
  return look;                                  /* return the previous character we were looking at */
}

/* return nonzero if we are looking at character c, ' ' means any white space */
I seeing(char c) {
  return c == ' ' ? see > 0 && see <= c : see == c;
}

/* tokenize into buf[], return first character of buf[] */
char scan() {
  I i = 0;
  while (seeing(' ') || seeing(';'))            /* skip white space and ;-comments */
    if (get() == ';')
      while (!seeing('\n'))                     /* skip ;-comment until newline */
        get();
  if (seeing('"')) {                            /* tokenize a quoted string */
    do {
      buf[i++] = get();
      while (seeing('\\') && i < sizeof(buf)-1) {
        static const char *abtnvfr = "abtnvfr"; /* \a, \b, \t, \n, \v, \f, \r escape codes */
        const char *esc;
        get();
        esc = strchr(abtnvfr, see);
        buf[i++] = esc ? esc-abtnvfr+7 : see;   /* replace \x with an escaped code or x itself */
        get();
      }
    }
    while (i < sizeof(buf)-1 && !seeing('"') && !seeing('\n'));
    if (get() != '"')
      ERR(8, "missing \" ");
  }
  else if (seeing('(') || seeing(')') || seeing('\''))
    buf[i++] = get();                           /* ( ) ' are single-character tokens */
  else                                          /* tokenize a symbol or a number */
    do
      buf[i++] = get();
    while (i < sizeof(buf)-1 && !seeing('(') && !seeing(')') && !seeing(' '));
  buf[i] = 0;
  return *buf;                                  /* return first character of token in buf[] */
}

/* return a parsed Lisp list */
L list() {
  L *p = push(nil);                             /* push the new list to protect it from getting GC'ed */
  while (1) {
    if (scan() == ')')
      return pop();
    if (*buf == '.' && !buf[1]) {               /* parse list with dot pair ( <expr> ... <expr> . <expr> ) */
      *p = read();                              /* read expression to replace the last nil at the end of the list */
      if (scan() != ')')
        ERR(8, "expecing ) ");
      return pop();                             /* pop list and return it */
    }
    *p = cons(parse(), nil);                    /* add parsed expression to end of the list by replacing the last nil */
    p = &CDR(*p);                               /* p points to the cdr nil to replace it with the rest of the list */
  }
}

/* return a parsed Lisp expression */
L parse() {
  L x; I i;
  if (*buf == '(')                              /* if token is ( then parse a list */
    return list();
  if (*buf == '\'') {                           /* if token is ' then parse an expression x to return (quote x) */
    x = cons(read(), nil);
    return cons(atom("quote"), x);
  }
  if (*buf == '"')                              /* if token is a string, then return a new string */
    return string(buf+1);
  if (sscanf(buf, "%lg%n", &x, &i) > 0 && !buf[i])
    return x;                                   /* return a number, including inf, -inf and nan */
  if (*buf != ')')
    return atom(buf);                           /* return an atom (a symbol) */
  return ERR(8, "unexpected ) ");
}

/*----------------------------------------------------------------------------*\
 |      PRIMITIVES -- SEE THE TABLE WITH COMMENTS FOR DETAILS                 |
\*----------------------------------------------------------------------------*/

public:

/* construct a new list of evaluated expressions in list t, i.e. the arguments passed to a function or primitive */
L evlis(L t, L e) {
  L *p = push(nil);                             /* push the new list to protect it from getting GC'ed */
  for (; T(t) == CONS; t = cdr(t)) {            /* for each expression in list t */
    *p = cons(eval(car(t), e), nil);            /* evaluate it and add it to the end of the list replacing last nil */
    p = &CDR(*p);                               /* p points to the cdr nil to replace it with the rest of the list */
  }
  if (T(t) == ATOM)                             /* if the list t ends in a symbol */
    *p = assoc(t, e);                           /* evaluate t to replace the last nil at the end of the new list */
  return pop();                                 /* pop new list and return it */
}

L f_type(L t, L *_) {
  L x = car(t);
  return T(x) == NIL ? -1.0 : T(x) >= PRIM && T(x) <= MACR ? T(x) - PRIM + 1 : 0.0;
}

L f_ident(L t, L *_) {
  return car(t);
}

L f_cons(L t, L *_) {
  return cons(car(t), car(cdr(t)));
}

L f_car(L t, L *_) {
  return car(car(t));
}

L f_cdr(L t, L *_) {
  return cdr(car(t));
}

L f_add(L t, L *_) {
  L n = car(t);
  while (!Not(t = cdr(t)))
    n += car(t);
  return num(n);
}

L f_sub(L t, L *_) {
  L n = Not(cdr(t)) ? -car(t) : car(t);
  while (!Not(t = cdr(t)))
    n -= car(t);
  return num(n);
}

L f_mul(L t, L *_) {
  L n = car(t);
  while (!Not(t = cdr(t)))
    n *= car(t);
  return num(n);
}

L f_div(L t, L *_) {
  L n = Not(cdr(t)) ? 1.0/car(t) : car(t);
  while (!Not(t = cdr(t)))
    n /= car(t);
  return num(n);
}

L f_int(L t, L *_) {
  L n = car(t);
  return n < 1e16 && n > -1e16 ? (int64_t)n : n;
}

L f_lt(L t, L *_) {
  L x = car(t), y = car(cdr(t));
  return (T(x) == T(y) && (T(x) & ~(ATOM^STRG)) == ATOM ? strcmp(A+ord(x), A+ord(y)) < 0 :
      x == x && y == y ? x < y : /* x == x is false when x is NaN i.e. a tagged Lisp expression */
      *(int64_t*)&x < *(int64_t*)&y) ? tru : nil;
}

L f_eq(L t, L *_) {
  L x = car(t), y = car(cdr(t));
  return (T(x) == STRG && T(y) == STRG ? !strcmp(A+ord(x), A+ord(y)) : equ(x, y)) ? tru : nil;
}

L f_not(L t, L *_) {
  return Not(car(t)) ? tru : nil;
}

L f_or(L t, L *e) {
  L x = nil;
  while (T(t) != NIL && Not(x = eval(car(t), *e)))
    t = cdr(t);
  return x;
}

L f_and(L t, L *e) {
  L x = nil;
  while (T(t) != NIL && !Not(x = eval(car(t), *e)))
    t = cdr(t);
  return x;
}

L f_begin(L t, L *e) {
  for (; more(t); t = cdr(t))
    eval(car(t), *e);
  return T(t) == NIL ? nil : car(t);
}

L f_while(L t, L *e) {
  L s, x = nil;
  while (!Not(eval(car(t), *e)))
    for (s = cdr(t); T(s) != NIL; s = cdr(s))
      x = eval(car(s), *e);
  return x;
}

L f_cond(L t, L *e) {
  while (T(t) != NIL && Not(eval(car(car(t)), *e)))
    t = cdr(t);
  return T(t) != NIL ? f_begin(cdr(car(t)), e) : nil;
}

L f_if(L t, L *e) {
  return Not(eval(car(t), *e)) ? f_begin(cdr(cdr(t)), e) : car(cdr(t));
}

L f_lambda(L t, L *e) {
  return closure(car(t), car(cdr(t)), *e);
}

L f_macro(L t, L *_) {
  return macro(car(t), car(cdr(t)));
}

L f_define(L t, L *e) {
  env = pair(car(t), eval(car(cdr(t)), *e), env);
  return car(t);
}

L f_assoc(L t, L *_) {
  return assoc(car(t), car(cdr(t)));
}

L f_env(L _, L *e) {
  return *e;
}

L f_let(L t, L *e) {
  L d = *e;
  for (; more(t); t = cdr(t))
    *e = pair(car(car(t)), eval(f_begin(cdr(car(t)), &d), d), *e);
  return T(t) == NIL ? nil : car(t);
}

L f_leta(L t, L *e) {
  for (; more(t); t = cdr(t))
    *e = pair(car(car(t)), eval(f_begin(cdr(car(t)), e), *e), *e);
  return T(t) == NIL ? nil : car(t);
}

L f_letrec(L t, L *e) {
  L s;
  for (s = t; more(s); s = cdr(s))
    *e = pair(car(car(s)), nil, *e);
  for (s = *e; more(t); s = cdr(s), t = cdr(t))
    CDR(car(s)) = eval(f_begin(cdr(car(t)), e), *e);
  return T(t) == NIL ? nil : car(t);
}

L f_letreca(L t, L *e) {
  for (; more(t); t = cdr(t)) {
    *e = pair(car(car(t)), nil, *e);
    CDR(car(*e)) = eval(f_begin(cdr(car(t)), e), *e);
  }
  return T(t) == NIL ? nil : car(t);
}

L f_setq(L t, L *e) {
  L x = eval(car(cdr(t)), *e), v = car(t), d = *e;
  while (T(d) == CONS && !equ(v, car(car(d))))
    d = cdr(d);
  return T(d) == CONS ? CDR(car(d)) = x : T(v) == ATOM ? ERR(3, "unbound %s ", A+ord(v)) : err(3);
}

L f_setcar(L t, L *_) {
  L p = car(t);
  return T(p) == CONS ? CAR(p) = car(cdr(t)) : err(1);
}

L f_setcdr(L t, L *_) {
  L p = car(t);
  return T(p) == CONS ? CDR(p) = car(cdr(t)) : err(1);
}

L f_read(L t, L *_) {
  L x; char c = see;
  see = ' ';
  *ps = 0;
  x = read();
  see = c;
  return x;
}

L f_print(L t, L *_) {
  for (; T(t) != NIL; t = cdr(t))
    print(car(t));
  return nil;
}

L f_println(L t, L *e) {
  f_print(t, e);
  putc('\n', out);
  return nil;
}

L f_write(L t, L *_) {
  L x;
  for (; T(t) != NIL; t = cdr(t)) {
    x = car(t);
    if (T(x) == STRG)
      fprintf(out, "%s", A+ord(x));
    else
      print(x);
  }
  return nil;
}

L f_string(L t, L *_) {
  I i, j; L s;
  for (i = 0, s = t; T(s) != NIL; s = cdr(s)) {
    L x = car(s);
    if ((T(x) & ~(ATOM^STRG)) == ATOM)
      i += strlen(A+ord(x));
    else if (T(x) == CONS)
      for (; T(x) == CONS; x = cdr(x))
        ++i;
    else if (x == x) /* false when x is NaN i.e. a tagged Lisp expression */
      i += snprintf(buf, sizeof(buf), FLOAT, x);
  }
  push(t);
  i = j = alloc(i);
  pop();
  for (s = t; T(s) != NIL; s = cdr(s)) {
    L x = car(s);
    if ((T(x) & ~(ATOM^STRG)) == ATOM)
      i += strlen(strcpy(A+i, A+ord(x)));
    else if (T(x) == CONS)
      for (; T(x) == CONS; x = cdr(x))
        *(A+i++) = car(x);
    else if (x == x) /* false when x is NaN i.e. a tagged Lisp expression */
      i += snprintf(A+i, sizeof(buf), FLOAT, x);
  }
  *(A+i) = 0;
  return box(STRG, j);
}

L f_load(L t, L *e) {
  L x = f_string(t, e);
  return input(A+ord(x)) ? cons(atom("load"), cons(x, nil)) : ERR(5, "cannot read %s ", A+ord(x));
}

L f_trace(L t, L *e) {
  I savedtr = tr;
  tr = T(t) == NIL ? 1 : car(t);
  return more(t) ? t = eval(car(cdr(t)), *e), tr = savedtr, t : tr;
}

L f_catch(L t, L *e) {
  L x; I savedsp = sp;
  try {
    x = eval(car(t), *e);
  }
  catch (int n) {
    x = cons(atom("ERR"), n);
  }
  sp = savedsp;
  return x;
}

L f_throw(L t, L *_) {
  throw static_cast<int>(num(car(t)));
}

struct QUIT { };
L f_quit(L t, L *_) {
  throw QUIT();
}

/* the file we are writing to, stdout by default */
FILE *out;

protected:

/* evaluation mode of a primitive */
static const uint8_t NORMAL = 0, SPECIAL = 1, TAILCALL = 2;

/* table of Lisp primitives, each has a name s, a function pointer f, and an evaluation mode m */
inline static const struct {
  const char *s;
  std::function<L(This&,L,L*)> f;
  uint8_t m;
} prim[43] = {
  {"type",     &This::f_type,    NORMAL},           /* (type x) => <type> value between -1 and 7 */
  {"eval",     &This::f_ident,   NORMAL|TAILCALL},  /* (eval <quoted-expr>) => <value-of-expr> */
  {"quote",    &This::f_ident,   SPECIAL},          /* (quote <expr>) => <expr> -- protect <expr> from evaluation */
  {"cons",     &This::f_cons,    NORMAL},           /* (cons x y) => (x . y) -- construct a pair */
  {"car",      &This::f_car,     NORMAL},           /* (car <pair>) => x -- "deconstruct" <pair> (x . y) */
  {"cdr",      &This::f_cdr,     NORMAL},           /* (cdr <pair>) => y -- "deconstruct" <pair> (x . y) */
  {"+",        &This::f_add,     NORMAL},           /* (+ n1 n2 ... nk) => n1+n2+...+nk */
  {"-",        &This::f_sub,     NORMAL},           /* (- n1 n2 ... nk) => n1-n2-...-nk or -n1 if k=1 */
  {"*",        &This::f_mul,     NORMAL},           /* (* n1 n2 ... nk) => n1*n2*...*nk */
  {"/",        &This::f_div,     NORMAL},           /* (/ n1 n2 ... nk) => n1/n2/.../nk or 1/n1 if k=1 */
  {"int",      &This::f_int,     NORMAL},           /* (int <integer.frac>) => <integer> */
  {"<",        &This::f_lt,      NORMAL},           /* (< n1 n2) => #t if n1<n2 else () */
  {"eq?",      &This::f_eq,      NORMAL},           /* (eq? x y) => #t if x==y else () */
  {"not",      &This::f_not,     NORMAL},           /* (not x) => #t if x==() else ()t */
  {"or",       &This::f_or,      SPECIAL},          /* (or x1 x2 ... xk) => #t if any x1 is not () else () */
  {"and",      &This::f_and,     SPECIAL},          /* (and x1 x2 ... xk) => #t if all x1 are not () else () */
  {"begin",    &This::f_begin,   SPECIAL|TAILCALL}, /* (begin x1 x2 ... xk) => xk -- evaluates x1, x2 to xk */
  {"while",    &This::f_while,   SPECIAL},          /* (while x y1 y2 ... yk) -- while x is not () eval y1, y2 ... yk */
  {"cond",     &This::f_cond,    SPECIAL|TAILCALL}, /* (cond (x1 y1) (x2 y2) ... (xk yk)) => yi for first xi!=() */
  {"if",       &This::f_if,      SPECIAL|TAILCALL}, /* (if x y z) => if x!=() then y else z */
  {"lambda",   &This::f_lambda,  SPECIAL},          /* (lambda <parameters> <expr>) => {closure} */
  {"macro",    &This::f_macro,   SPECIAL},          /* (macro <parameters> <expr>) => [macro] */
  {"define",   &This::f_define,  SPECIAL},          /* (define <symbol> <expr>) -- globally defines <symbol> */
  {"assoc",    &This::f_assoc,   NORMAL},           /* (assoc <quoted-symbol> <environment>) => <value-of-symbol> */
  {"env",      &This::f_env,     NORMAL},           /* (env) => <environment> */
  {"let",      &This::f_let,     SPECIAL|TAILCALL}, /* (let (v1 x1) (v2 x2) ... (vk xk) y) => y with scope */
  {"let*",     &This::f_leta,    SPECIAL|TAILCALL}, /* (let* (v1 x1) (v2 x2) ... (vk xk) y) => y with scope */
  {"letrec",   &This::f_letrec,  SPECIAL|TAILCALL}, /* (letrec (v1 x1) (v2 x2) ... (vk xk) y) => y recursive scope */
  {"letrec*",  &This::f_letreca, SPECIAL|TAILCALL}, /* (letrec* (v1 x1) (v2 x2) ... (vk xk) y) => y recursive scope */
  {"setq",     &This::f_setq,    SPECIAL},          /* (setq <symbol> x) -- changes value of <symbol> in scope to x */
  {"set-car!", &This::f_setcar,  NORMAL},           /* (set-car! <pair> x) -- changes car of <pair> to x in memory */
  {"set-cdr!", &This::f_setcdr,  NORMAL},           /* (set-cdr! <pair> y) -- changes cdr of <pair> to y in memory */
  {"read",     &This::f_read,    NORMAL},           /* (read) => <value-of-input> */
  {"print",    &This::f_print,   NORMAL},           /* (print x1 x2 ... xk) => () -- prints the values x1 x2 ... xk */
  {"println",  &This::f_println, NORMAL},           /* (println x1 x2 ... xk) => () -- prints with newline */
  {"write",    &This::f_write,   NORMAL},           /* (write x1 x2 ... xk) => () -- prints without quoting strings */
  {"string",   &This::f_string,  NORMAL},           /* (string x1 x2 ... xk) => <string> -- string of x1 x2 ... xk */
  {"load",     &This::f_load,    NORMAL},           /* (load <name>) -- loads file <name> (an atom or string name) */
  {"trace",    &This::f_trace,   SPECIAL},          /* (trace flag [<expr>]) -- flag 0=off, 1=on, 2=keypress */
  {"catch",    &This::f_catch,   SPECIAL},          /* (catch <expr>) => <value-of-expr> if no except. else (ERR . n) */
  {"throw",    &This::f_throw,   NORMAL},           /* (throw n) -- raise exception error code n (integer != 0) */
  {"quit",     &This::f_quit,    NORMAL},           /* (quit) -- bye! */
  {0}
};

/*----------------------------------------------------------------------------*\
 |      EVAL                                                                  |
\*----------------------------------------------------------------------------*/

public:

/* trace the evaluation of x in environment e, returns its value */
L eval(L x, L e) {
  L y;
  if (!tr)
    return step(x, e);                          /* eval() -> step() tail call when not tracing */
  y = step(x, e);
  printf("%4u: ", N-sp); print(x);              /* <stack depth>: unevaluated expression */
  printf(" => ");        print(y);              /* => value of the expression */
  if (tr > 1)                                   /* wait for ENTER key or other CTRL */
    while (getchar() >= ' ')
      continue;
  else
    putchar('\n');
  return y;
}

protected:

/* step-wise evaluate x in environment e, returns value of x, tail-call optimized */
L step(L x, L e) {
  L *f, v, *d, *y, *z; I k = sp;                /* save sp to unwind the stack back to sp afterwards */
  f = push(nil);                                /* protect closure f from getting GC'ed */
  d = push(nil);                                /* protect new bindings d from getting GC'ed */
  y = push(nil);                                /* protect alias y of new x from getting GC'ed */
  z = push(nil);                                /* protect alias z of new e from getting GC'ed */
  while (1) {
    if (T(x) == ATOM) {                         /* if x is an atom, then return its associated value */
      x = assoc(x, e);
      break;
    }
    if (T(x) != CONS)                           /* if x is not a list or pair, then return x itself */
      break;
    *f = eval(car(x), e);                       /* the function/primitive is at the head of the list */
    x = cdr(x);                                 /* ... and its actual arguments are the rest of the list */
    if (T(*f) == PRIM) {                        /* if f is a primitive, then apply it to the actual arguments x */
      I i = ord(*f);
      if (!(prim[i].m & SPECIAL))               /* if the primitive is NORMAL mode, */
        x = evlis(x, e);                        /* ... then evaluate actual arguments x */
      *z = e;
      x = *y = prim[i].f(*this, x, z);          /* call the primitive with arguments x, put return value back in x */
      e = *z;                                   /* the new environment e is d to evaluate x, put in *z to protect */
      if (prim[i].m & TAILCALL)                 /* if the primitive is TAILCALL mode, */
        continue;                               /* ... then continue evaluating x */
      break;                                    /* else break to return value x */
    }
    if ((T(*f) & ~(CLOS^MACR)) != CLOS)         /* if f is not a closure or macro, then we cannot apply it */
      err(4);
    if (T(*f) == CLOS) {                        /* if f is a closure, then */
      *d = cdr(*f);                             /* construct an extended local environment d from f's static scope */
      if (T(*d) == NIL)                         /* if f's static scope is nil, then use global env as static scope */
        *d = env;
      v = car(car(*f));                         /* get the parameters v of closure f */
      while (T(v) == CONS && T(x) == CONS) {    /* bind parameters v to argument values x to extend the local scope d */
        *d = pair(car(v), eval(car(x), e), *d); /* add new binding to the front of d */
        v = cdr(v);
        x = cdr(x);
      }
      if (T(v) == CONS) {                       /* condinue binding v if x is after a dot (... . x) by evaluating x */
        *y = eval(x, e);                        /* evaluate x and save its value y to protect it from getting GC'ed */
        while (T(v) == CONS && T(*y) == CONS) {
          *d = pair(car(v), car(*y), *d);       /* add new binding to the front of d */
          v = cdr(v);
          *y = cdr(*y);
        }
        if (T(v) == CONS)                       /* error if insufficient actual arguments x are provided */
          err(4);
        x = *y;
      }
      else if (T(x) == CONS)                    /* if more arguments x are provided then evaluate them all */
        x = evlis(x, e);
      else if (T(x) != NIL)                     /* else if last argument x is after a dot (... . x) then evaluate x */
        x = eval(x, e);
      if (T(v) != NIL)                          /* if last parameter v is after a dot (... . v) then bind it to x */
        *d = pair(v, x, *d);
      x = *y = cdr(car(*f));                    /* tail recursion optimization: evaluate the body x of closure f next */
      e = *z = *d;                              /* the new environment e is d to evaluate x, put in *z to protect */
    }
    else {                                      /* else if f is a macro, then */
      *d = env;                                 /* construct an extended local environment d from global env */
      v = car(*f);                              /* get the parameters v of macro f */
      while (T(v) == CONS && T(x) == CONS) {    /* bind parameters v to arguments x to extend the local scope d */
        *d = pair(car(v), car(x), *d);
        v = cdr(v);
        x = cdr(x);
      }
      if (T(v) == CONS)                         /* error if insufficient actual arguments x are provided */
        err(4);
      if (T(v) != NIL)                          /* if last parameter v is after a dot (... . v) then bind it to x */
        *d = pair(v, x, *d);
      x = *y = eval(cdr(*f), *d);               /* evaluated body of the macro to evaluate next, put in *z to protect */
    }
  }
  unwind(k);                                    /* unwind the stack to allow GC to collect unused temporaries */
  return x;                                     /* return x evaluated */
}

/*----------------------------------------------------------------------------*\
 |      PRINT                                                                 |
\*----------------------------------------------------------------------------*/

public:

/* output Lisp expression x */
void print(L x) {
  if (T(x) == NIL)
    fprintf(out, "()");
  else if (T(x) == PRIM)
    fprintf(out, "<%s>", prim[ord(x)].s);
  else if (T(x) == ATOM)
    fprintf(out, "%s", A+ord(x));
  else if (T(x) == STRG)
    fprintf(out, "\"%s\"", A+ord(x));
  else if (T(x) == CONS)
    printlist(x);
  else if (T(x) == CLOS)
    fprintf(out, "{%u}", ord(x));
  else if (T(x) == MACR)
    fprintf(out, "[%u]", ord(x));
  else
    fprintf(out, FLOAT, x);
}

protected:

/* output Lisp list t */
void printlist(L t) {
  putc('(', out);
  while (1) {
    print(car(t));
    t = cdr(t);
    if (T(t) == NIL)
      break;
    if (T(t) != CONS) {
      fprintf(out, " . ");
      print(t);
      break;
    }
    putc(' ', out);
  }
  putc(')', out);
}

};

#endif
