/* lisp-pr-single.c Lisp with pointer reversal mark-sweep GC and NaN boxing by Robert A. van Engelen 2022 BSD-3 license
        - single precision floating point, symbols, strings, lists, proper closures, and macros
        - over 40 built-in Lisp primitives
        - lexically-scoped locals in lambda, let, let*, letrec, letrec*
        - proper tail-recursion, including tail calls through begin, cond, if, let, let*, letrec, letrec*
        - exceptions and error handling with safe return to REPL after an error
        - break with CTRL-C to return to the REPL (compile: lisp.c -DHAVE_SIGNAL_H)
        - REPL with readline (compile: lisp.c -DHAVE_READLINE_H -lreadline)
        - load Lisp source code files
        - execution tracing to display Lisp evaluation steps
        - mark-sweep garbage collector with efficient "pointer reversal" to recycle unused cons pair cells
        - compacting garbage collector to recycle unused atoms and strings */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>             /* uint32_t */
#include <string.h>
#include <setjmp.h>

#ifdef HAVE_SIGNAL_H
#include <signal.h>             /* to catch CTRL-C and continue the REPL */
#define BREAK_ON  signal(SIGINT, (void(*)(int))err)
#define BREAK_OFF signal(SIGINT, SIG_IGN)
#else
#define BREAK_ON  (void)0
#define BREAK_OFF (void)0
#endif

#ifdef HAVE_READLINE_H
#include <readline/readline.h>  /* for convenient line editing ... */
#include <readline/history.h>   /* ... and a history of previous Lisp input */
#else
void using_history() { }
#endif

/* single precision floating point output format */
#define FLOAT "%.7g"

/* DEBUG: always run GC when allocating cells and atoms/strings on the heap */
#ifdef DEBUG
#define ALWAYS_GC 1
#else
#define ALWAYS_GC 0
#endif

/*----------------------------------------------------------------------------*\
 |      LISP EXPRESSION TYPES AND NAN BOXING                                  |
\*----------------------------------------------------------------------------*/

/* we only need two types to implement a Lisp interpreter:
        I      unsigned integer (32 bit unsigned)
        L      Lisp expression (single precision float with NaN boxing)
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
#define I uint32_t
#define L float

/* T(x) returns the tag bits of a NaN-boxed Lisp expression x */
#define T(x) (*(I*)&x >> 20)

/* primitive, atom, string, cons, closure, macro and nil tags for NaN boxing (reserve 0x7f8 for nan) */
I PRIM = 0x7f9, ATOM = 0x7fa, STRG = 0x7fb, CONS = 0x7fc, CLOS = 0x7fe, MACR = 0x7ff, NIL = 0xfff;

/* box(t,i): returns a new NaN-boxed float with tag t and 20 bits ordinal i
   ord(x):   returns the 20 bits ordinal of the NaN-boxed float x
   num(n):   convert or check number n (does nothing, e.g. could check for NaN)
   equ(x,y): returns nonzero if x equals y */
L box(I t, I i) {
  L x;
  *(I*)&x = (I)t << 20 | i;
  return x;
}

I ord(L x) {
  return *(I*)&x & 0xfffff;     /* the 20 bits ordinal return value is masked to remove the tag */
}

L num(L n) {
  return n;                     /* this could check for a valid number: return n == n ? n : err(5); */
}

I equ(L x, L y) {
  return *(I*)&x == *(I*)&y;
}

/*----------------------------------------------------------------------------*\
 |      ERROR HANDLING AND ERROR MESSAGES                                     |
\*----------------------------------------------------------------------------*/

/* setjmp-longjmp jump buffer */
jmp_buf jb;

/* report and throw an exception */
#define ERR(n, ...) (fprintf(stderr, __VA_ARGS__), err(n))
L err(int n) { longjmp(jb, n); }

#define ERRORS 8
const char *errors[ERRORS+1] = {
  "",
  "not a pair",                                 /* 1 */
  "break",                                      /* 2 */
  "unbound symbol",                             /* 3 */
  "cannot apply",                               /* 4 */
  "arguments",                                  /* 5 */
  "stack over",                                 /* 6 */
  "out of memory",                              /* 7 */
  "syntax"                                      /* 8 */
};

/*----------------------------------------------------------------------------*\
 |      MEMORY MANAGEMENT AND RECYCLING                                       |
\*----------------------------------------------------------------------------*/

/* number of cells to allocate for the cons pair pool, increase P as desired, but P+S < 262144 */
#define P 8192

/* number of cells to allocate for the shared stack and heap, increase S as desired, but P+S < 262144 */
#define S 2048

/* total number of cells to allocate = P+S, should not exceed 262143 = 2^20/4-1 */
#define N (P+S)

/* base address of the atom/string heap */
#define A (char*)cell

/* heap address start offset, the heap starts at address A+H immediately above the pool */
#define H (sizeof(L)*P)

/* size of the cell reference field of an atom/string on the heap, used by the compacting garbage collector */
#define R sizeof(I)

/* array of Lisp expressions, shared by the pool, heap and stack */
L cell[N];

/* fp: free pointer points to free cell pair in the pool, next free pair is ord(cell[fp]) unless fp=0
   hp: heap pointer, A+hp points free atom/string heap space above the pool and below the stack
   sp: stack pointer, the stack starts at the top of cell[] with sp=N
   tr: 0 when tracing is off, 1 or 2 to trace Lisp evaluation steps */
I fp = 0, hp = H, sp = N, tr = 0;

/* Lisp constant expressions () (nil) and #t, and the global environment env */
L nil, tru, env;

/* bit vector corresponding to the pairs of cells in the pool marked 'used' (car and cdr cells are marked together) */
uint32_t used[(P+63)/64];

/* mark-sweep garbage collector recycles cons pair pool cells, finds and marks cells that are used */
void mark(I i) {
  I j = N;                                      /* the cell above, N is a sentinel value, i.e. no cell above the root */
  I k;                                          /* the car or cdr cell below to visit (go down) or visited (go up) */
  if (used[i/64] & 1 << i/2%32)                 /* if i'th cell pair is already marked used, then nothing to do */
    return;
  while (j < N || !(i & 1)) {                   /* loop while not at the root or the i'th cell is a car cell to mark */
    while (1) {                                 /* go down the list, marking unused car cons pairs first before cdr */
      used[i/64] |= 1 << i/2%32;                /* mark the i'th cell pair (both car and cdr), i is even */
      if ((T(cell[i]) & ~(CONS^MACR)) != CONS ||        /* if car cell[i] does not refer to a pair */
          (k = ord(cell[i]),                            /* or if car is an already used pair */
           used[k/64] & 1 << k/2%32))
        if ((T(cell[++i]) & ~(CONS^MACR)) != CONS ||    /* then increment i, if cdr cell[i] does not refer to a pair */
            (k = ord(cell[i]),                          /* or if cdr is an already used pair */
             used[k/64] & 1 << k/2%32))
          break;                                        /* then break to go back up the reversed pointers */
      cell[i] = box(T(cell[i]), j);             /* reverse the car (even i) or the cdr (odd i) pointer */
      j = i;                                    /* remember the last cell visited */
      i = k;                                    /* next cell pair to visit down, i is even */
    }
    while (j < N) {                             /* go back up via the reversed pointers until we are back at the root */
      k = i;                                    /* last cell visited when going back up, i is even (car) or odd (cdr) */
      i = j;                                    /* the cell we visit, when going back up, is a car or cdr cell */
      j = ord(cell[i]);                         /* next cell is up, by following the reversed pointer up */
      cell[i] = box(T(cell[i]), k & ~1);        /* un-reverse the car (even i) or cdr (odd i) pointer, make k even */
      if (!(i & 1))                             /* if i'th cell is a car (even i), then break to go down cdr cell */
        break;
    }
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

/* garbage collector, returns number of free cells in the pool or raises err(7) */
I gc() {
  I i;
  BREAK_OFF;                                    /* do not interrupt GC */
  memset(used, 0, sizeof(used));                /* clear all used[] bits */
  if (T(env) == CONS)
    mark(ord(env));                             /* mark all globally-used cons cell pairs referenced from env list */
  for (i = sp; i < N; ++i)
    if ((T(cell[i]) & ~(CONS^MACR)) == CONS)
      mark(ord(cell[i]));                       /* mark all cons cell pairs referenced from the stack */
  i = sweep();                                  /* remove unused cons cell pairs from the pool */
  compact();                                    /* remove unused atoms and strings from the heap */
  BREAK_ON;                                     /* enable interrupt */
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
void unwind(I i) {
  sp = i;
}

/*----------------------------------------------------------------------------*\
 |      LISP EXPRESSION CONSTRUCTION AND INSPECTION                           |
\*----------------------------------------------------------------------------*/

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

/* not(x) is nonzero if x is the Lisp () empty list */
I not(L x) {
  return T(x) == NIL;
}

/* more(t) is nonzero if list t has more than one item, i.e. is not empty or a singleton list */
I more(L t) {
  return T(t) != NIL && (t = cdr(t), T(t) != NIL);
}

/*----------------------------------------------------------------------------*\
 |      READ                                                                  |
\*----------------------------------------------------------------------------*/

/* the file(s) we are reading or fin=0 when reading from the terminal */
I fin = 0;
FILE *in[10];

/* specify an input file to parse and try to open it */
FILE *input(const char *s) {
  return fin <= 9 && (in[fin] = fopen(s, "r")) ? in[fin++] : NULL;
}

/* tokenization buffer, the next character we're looking at, the readline line, prompt and input file */
char buf[256], see = '\n', *ptr = "", *line = NULL, ps[20];

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
      BREAK_OFF;                                /* disable interrupt to prevent free() without final line = NULL */
      if (line)                                 /* free the old line that was malloc'ed by readline */
        free(line);
      line = NULL;
      BREAK_ON;                                 /* enable interrupt */
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

/* return the Lisp expression parsed and read from input */
L parse();
L readlisp() {
  scan();
  return parse();
}

/* return a parsed Lisp list */
L list() {
  L *p = push(nil);                             /* push the new list to protect it from getting GC'ed */
  while (1) {
    if (scan() == ')')
      return pop();
    if (*buf == '.' && !buf[1]) {               /* parse list with dot pair ( <expr> ... <expr> . <expr> ) */
      *p = readlisp();                          /* read expression to replace the last nil at the end of the list */
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
    x = cons(readlisp(), nil);
    return cons(atom("quote"), x);
  }
  if (*buf == '"')                              /* if token is a string, then return a new string */
    return string(buf+1);
  if (sscanf(buf, "%g%n", &x, &i) > 0 && !buf[i])
    return x;                                   /* return a number, including inf, -inf and nan */
  if (*buf != ')')
    return atom(buf);                           /* return an atom (a symbol) */
  return ERR(8, "unexpected ) ");
}

/*----------------------------------------------------------------------------*\
 |      PRIMITIVES -- SEE THE TABLE WITH COMMENTS FOR DETAILS                 |
\*----------------------------------------------------------------------------*/

/* the file we are writing to, stdout by default */
FILE *out;

/* construct a new list of evaluated expressions in list t, i.e. the arguments passed to a function or primitive */
L eval(L, L);
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
  while (!not(t = cdr(t)))
    n += car(t);
  return num(n);
}

L f_sub(L t, L *_) {
  L n = not(cdr(t)) ? -car(t) : car(t);
  while (!not(t = cdr(t)))
    n -= car(t);
  return num(n);
}

L f_mul(L t, L *_) {
  L n = car(t);
  while (!not(t = cdr(t)))
    n *= car(t);
  return num(n);
}

L f_div(L t, L *_) {
  L n = not(cdr(t)) ? 1.0/car(t) : car(t);
  while (!not(t = cdr(t)))
    n /= car(t);
  return num(n);
}

L f_int(L t, L *_) {
  L n = car(t);
  return n < 1e6 && n > -1e6 ? (I)n : n;
}

L f_lt(L t, L *_) {
  L x = car(t), y = car(cdr(t));
  return (T(x) == T(y) && (T(x) & ~(ATOM^STRG)) == ATOM ? strcmp(A+ord(x), A+ord(y)) < 0 :
      x == x && y == y ? x < y : /* x == x is false when x is NaN i.e. a tagged Lisp expression */
      *(I*)&x < *(I*)&y) ? tru : nil;
}

L f_eq(L t, L *_) {
  L x = car(t), y = car(cdr(t));
  return (T(x) == STRG && T(y) == STRG ? !strcmp(A+ord(x), A+ord(y)) : equ(x, y)) ? tru : nil;
}

L f_not(L t, L *_) {
  return not(car(t)) ? tru : nil;
}

L f_or(L t, L *e) {
  L x = nil;
  while (T(t) != NIL && not(x = eval(car(t), *e)))
    t = cdr(t);
  return x;
}

L f_and(L t, L *e) {
  L x = nil;
  while (T(t) != NIL && !not(x = eval(car(t), *e)))
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
  while (!not(eval(car(t), *e)))
    for (s = cdr(t); T(s) != NIL; s = cdr(s))
      x = eval(car(s), *e);
  return x;
}

L f_cond(L t, L *e) {
  while (T(t) != NIL && not(eval(car(car(t)), *e)))
    t = cdr(t);
  return T(t) != NIL ? f_begin(cdr(car(t)), e) : nil;
}

L f_if(L t, L *e) {
  return not(eval(car(t), *e)) ? f_begin(cdr(cdr(t)), e) : car(cdr(t));
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
  x = readlisp();
  see = c;
  return x;
}

void print(L);
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
  jmp_buf savedjb;
  memcpy(savedjb, jb, sizeof(jb));
  x = setjmp(jb);
  x = x ? cons(atom("ERR"), x) : eval(car(t), *e);
  memcpy(jb, savedjb, sizeof(jb));
  sp = savedsp;
  return x;
}

L f_throw(L t, L *_) {
  longjmp(jb, num(car(t)));
}

L f_quit(L t, L *_) {
  exit(0);
}

/* table of Lisp primitives, each has a name s, a function pointer f, and an evaluation mode m */
struct {
  const char *s;
  L (*f)(L, L*);
  enum { NORMAL, SPECIAL, TAILCALL } m;
} prim[] = {
  {"type",     f_type,    NORMAL},              /* (type x) => <type> value between -1 and 7 */
  {"eval",     f_ident,   NORMAL|TAILCALL},     /* (eval <quoted-expr>) => <value-of-expr> */
  {"quote",    f_ident,   SPECIAL},             /* (quote <expr>) => <expr> -- protect <expr> from evaluation */
  {"cons",     f_cons,    NORMAL},              /* (cons x y) => (x . y) -- construct a pair */
  {"car",      f_car,     NORMAL},              /* (car <pair>) => x -- "deconstruct" <pair> (x . y) */
  {"cdr",      f_cdr,     NORMAL},              /* (cdr <pair>) => y -- "deconstruct" <pair> (x . y) */
  {"+",        f_add,     NORMAL},              /* (+ n1 n2 ... nk) => n1+n2+...+nk */
  {"-",        f_sub,     NORMAL},              /* (- n1 n2 ... nk) => n1-n2-...-nk or -n1 if k=1 */
  {"*",        f_mul,     NORMAL},              /* (* n1 n2 ... nk) => n1*n2*...*nk */
  {"/",        f_div,     NORMAL},              /* (/ n1 n2 ... nk) => n1/n2/.../nk or 1/n1 if k=1 */
  {"int",      f_int,     NORMAL},              /* (int <integer.frac>) => <integer> */
  {"<",        f_lt,      NORMAL},              /* (< n1 n2) => #t if n1<n2 else () */
  {"eq?",      f_eq,      NORMAL},              /* (eq? x y) => #t if x==y else () */
  {"not",      f_not,     NORMAL},              /* (not x) => #t if x==() else ()t */
  {"or",       f_or,      SPECIAL},             /* (or x1 x2 ... xk) => #t if any x1 is not () else () */
  {"and",      f_and,     SPECIAL},             /* (and x1 x2 ... xk) => #t if all x1 are not () else () */
  {"begin",    f_begin,   SPECIAL|TAILCALL},    /* (begin x1 x2 ... xk) => xk -- evaluates x1, x2 to xk */
  {"while",    f_while,   SPECIAL},             /* (while x y1 y2 ... yk) -- while x is not () evaluate y1, y2 ... yk */
  {"cond",     f_cond,    SPECIAL|TAILCALL},    /* (cond (x1 y1) (x2 y2) ... (xk yk)) => yi for first xi!=() */
  {"if",       f_if,      SPECIAL|TAILCALL},    /* (if x y z) => if x!=() then y else z */
  {"lambda",   f_lambda,  SPECIAL},             /* (lambda <parameters> <expr>) => {closure} */
  {"macro",    f_macro,   SPECIAL},             /* (macro <parameters> <expr>) => [macro] */
  {"define",   f_define,  SPECIAL},             /* (define <symbol> <expr>) -- globally defines <symbol> */
  {"assoc",    f_assoc,   NORMAL},              /* (assoc <quoted-symbol> <environment>) => <value-of-symbol> */
  {"env",      f_env,     NORMAL},              /* (env) => <environment> */
  {"let",      f_let,     SPECIAL|TAILCALL},    /* (let (v1 x1) (v2 x2) ... (vk xk) y) => y with scope of bindings */
  {"let*",     f_leta,    SPECIAL|TAILCALL},    /* (let* (v1 x1) (v2 x2) ... (vk xk) y) => y with scope of bindings */
  {"letrec",   f_letrec,  SPECIAL|TAILCALL},    /* (letrec (v1 x1) (v2 x2) ... (vk xk) y) => y with recursive scope */
  {"letrec*",  f_letreca, SPECIAL|TAILCALL},    /* (letrec* (v1 x1) (v2 x2) ... (vk xk) y) => y with recursive scope */
  {"setq",     f_setq,    SPECIAL},             /* (setq <symbol> x) -- changes value of <symbol> in scope to x */
  {"set-car!", f_setcar,  NORMAL},              /* (set-car! <pair> x) -- changes car of <pair> to x in memory */
  {"set-cdr!", f_setcdr,  NORMAL},              /* (set-cdr! <pair> y) -- changes cdr of <pair> to y in memory */
  {"read",     f_read,    NORMAL},              /* (read) => <value-of-input> */
  {"print",    f_print,   NORMAL},              /* (print x1 x2 ... xk) => () -- prints the values x1 x2 ... xk */
  {"println",  f_println, NORMAL},              /* (println x1 x2 ... xk) => () -- prints with newline */
  {"write",    f_write,   NORMAL},              /* (write x1 x2 ... xk) => () -- prints without quoting strings */
  {"string",   f_string,  NORMAL},              /* (string x1 x2 ... xk) => <string> -- string of x1 x2 ... xk */
  {"load",     f_load,    NORMAL},              /* (load <name>) -- loads file <name> (an atom or string name) */
  {"trace",    f_trace,   SPECIAL},             /* (trace flag [<expr>]) -- flag 0=off, 1=on, 2=keypress */
  {"catch",    f_catch,   SPECIAL},             /* (catch <expr>) => <value-of-expr> if no exception else (ERR . n) */
  {"throw",    f_throw,   NORMAL},              /* (throw n) -- raise exception error code n (integer != 0) */
  {"quit",     f_quit,    NORMAL},              /* (quit) -- bye! */
  {0}
};

/*----------------------------------------------------------------------------*\
 |      EVAL                                                                  |
\*----------------------------------------------------------------------------*/

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
      x = *y = prim[i].f(x, z);                 /* call the primitive with arguments x, put return value back in x */
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

/*----------------------------------------------------------------------------*\
 |      PRINT                                                                 |
\*----------------------------------------------------------------------------*/

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

/*----------------------------------------------------------------------------*\
 |      REPL                                                                  |
\*----------------------------------------------------------------------------*/

/* entry point with Lisp initialization, error handling and REPL */
int main(int argc, char **argv) {
  int i;
  printf("lisp");
  input(argc > 1 ? argv[1] : "init.lisp");      /* set input source to load when available */
  out = stdout;
  if (setjmp(jb))                               /* if something goes wrong before REPL, it is fatal */
    abort();
  sweep();                                      /* clear the pool and heap */
  nil = box(NIL, 0);                            /* set the constant nil (empty list) */
  tru = atom("#t");                             /* set the constant #t */
  env = pair(tru, tru, nil);                    /* create environment with symbolic constant #t */
  for (i = 0; prim[i].s; ++i)                   /* expand environment with primitives */
    env = pair(atom(prim[i].s), box(PRIM, i), env);
  using_history();
  BREAK_ON;                                     /* enable CTRL-C break to throw error 2 */
  i = setjmp(jb);                               /* init error handler: i is nonzero when thrown */
  if (i) {
    while (fin)                                 /* close all open files */
      fclose(in[--fin]);
    printf("ERR %d: %s", i, errors[i > 0 && i <= ERRORS ? i : 0]);
  }
  while (1) {                                   /* read-evel-print loop */
    putchar('\n');
    unwind(N);
    i = gc();
    snprintf(ps, sizeof(ps), "%u+%u>", i, sp-hp/8);
    out = stdout;
    print(eval(*push(readlisp()), env));
  }
}
