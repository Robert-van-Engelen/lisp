# Lisp in 1k lines of C, explained

A logical continuation of the [tinylisp](https://github.com/Robert-van-Engelen/tinylisp) project _"Lisp in 99 lines of C and how to write one yourself."_  If you're interested in writing a Lisp interpreter of your own, then you may want to check out the [tinylisp](https://github.com/Robert-van-Engelen/tinylisp) project first.  If you already did, then welcome back!

A quick glance at this small Lisp interpreter's features:

- Lisp with _floating point_, _strings_, proper _closures_, and _macros_
- over _40 built-in Lisp primitives_
- _lexically-scoped_ locals, like tinylisp
- proper _tail recursion_, including tail calls through `begin`, `cond`, `if`, `let`, `let*`, `letrec`, `letrec*`
- _exceptions_ and error handling with safe return to REPL after an error
- _break with CTRL-C_ to return to the REPL (optional)
- REPL with GNU _readline_ for convenient Lisp input (optional)
- _load Lisp_ source code files
- _execution tracing_ to display Lisp evaluation steps
- _mark-sweep garbage collector_ to recycle unused cons pair cells
- plus an alternative _non-recursive garbage collector_ (mark-sweep using pointer reversal)
- _compacting garbage collector_ to recycle unused atoms and strings
- Lisp memory is a _single `cell[]` array_, no `malloc()` and `free()` calls
- easily _customizable and extensible_ to add new special features
- _integrates with C (and C++)_ code by calling C (C++) functions for Lisp primitives, for example to [embed a Lisp interpreter](#embedding)

I've documented this project's source code extensively to explain the inner workings of the Lisp interpreter, which should make it easy to use and to modify the code.  This small Lisp interpreter includes a tracing garbage collector to recycle unused cons pair cells and unused atoms and strings.  There are different methods of garbage collection that can be used by a Lisp interpreter.  I chose the simple [mark-sweep method](#classic-mark-sweep-garbage-collection).  By contrast, a [copying garbage collector](https://github.com/Robert-van-Engelen/lisp-cheney) requires double the heap memory, but has the advantage of being free of recursion (no call stack).  In this project I've included a small and efficient [mark-sweep method with pointer reversal](#alternative-non-recursive-mark-sweep-garbage-collection-using-pointer-reversal) as an alternative method to eliminate recursive calls.  An advantage of mark-sweep is that Lisp data is never moved in memory and can be consistently referenced by other C/C++ code.  In addition to mark-sweep, a [compacting garbage collector](#compacting-garbage-collection-to-recycle-the-atomstring-heap) removes unused atoms and strings from the heap.

## Is it really Lisp?

Like [tinylisp](https://github.com/Robert-van-Engelen/tinylisp), this Lisp preserves the original meaning and flavor of [John McCarthy](https://en.wikipedia.org/wiki/John_McCarthy_(computer_scientist))'s [Lisp](https://en.wikipedia.org/wiki/Lisp_(programming_language)) as much as possible:

    > (define curry
          (lambda (f x)
              (lambda args
                  (f x . args))))
    > ((curry + 1) 2 3)
    6

If your Lisp can't [curry](https://en.wikipedia.org/wiki/Currying) like this, it isn't classic Lisp!

## Proper tail recursive

Tail-recursive calls and tail calls in general are optimized.  For example, `(forever inf)` infinite recursion:

    > (define forever
          (lambda (n)
              (if (eq? 0 n)
                  'done
                  (write "forever\n") (forever (- n 1)))))
    > (forever inf)
    forever
    forever
    ...

Tail call optimization is applied to the last function evaluated when its return value is not used as an argument to another function to operate on.  Tail call optimization is also applied to the tail calls made through the `begin`, `cond`, `if`, `let`, `let*`, `letrec`, and `letrec*` special forms.

## Running Lisp

Initialization imports `init.lisp` first, when located in the working directory.  Otherwise this step is skipped.  You can load Lisp source files with `(load "name.lisp")`, for example

    $ ./lisp
    ...
    defun
    6322+1929>(load "nqueens.lisp")
    ...
    (- - - - - - - @)
    (- - - @ - - - -)
    (@ - - - - - - -)
    (- - @ - - - - -)
    (- - - - - @ - -)
    (- @ - - - - - -)
    (- - - - - - @ -)
    (- - - - @ - - -)

    done

The prompt displays the number of free cons pair cells + free stack cells available.  The heap and stack are located in the same memory space.  Therefore, the second number is also indicative of the size of the heap space available to store new atoms and strings.

## Execution tracing

An execution trace displays the stack depth with each evaluation step:

    6322+1929>(trace)
    1
    6322+1929>((curry + 1) 2 3)
       9: curry => {1636}
       9: + => <+>
       9: 1 => 1
       9: lambda => <lambda>
       5: (curry + 1) => {1886}
       6: 2 => 2
       6: 3 => 3
       5: f => <+>
       6: x => 1
       1: ((curry + 1) 2 3) => 6
    6

Note: the origin of a tail call may not be displayed.

## Compilation

Just one source code file [lisp.c](src/lisp.c) to compile:

    $ cc -o lisp lisp.c -O2 -DHAVE_SIGNAL_H -DHAVE_READLINE_H -lreadline

Without CTRL-C to break and without the [GNU readline](https://en.wikipedia.org/wiki/GNU_Readline) library:

    $ cc -o lisp lisp.c -O2

A C++ REPL with [lisp.hpp](src/lisp.hpp) header-only Lisp interpreter:

    $ c++ -std=c++17 lisp-repl.cpp -O2 -DHAVE_SIGNAL_H -DHAVE_READLINE_H -lreadline

## Testing

    cd tests && ./runtests.sh

## Lisp language features

### Numbers

Double precision floating point numbers, including `inf`, `-inf` and `nan`.  Numbers may also be entered in hexadecimal `0xh...h` format.

### Symbols

Lisp symbols consist of a sequence of non-space characters, excluding `(`, `)` and quotes.  When used in a Lisp expression, a symbol is looked-up for its value, like a variable typically refers to its value.  Symbols can be '-quoted like `'foo` to use symbols literally and to pass them to functions.

### Booleans

Well, Lisp doesn't need Booleans.  An `()` empty list (called nil) is considered false and anything not `()` is considered true.  For convenience, `#t` is a symbol representing true (`#t` evaluates to itself, i.e. quoting is not needed.)

### Strings

Strings are "-quoted and may contain `\a`, `\b`, `\t`, `\n`, `\v`, `\f` and `\r` escapes.  Use `\"` to escape the quote and `\\` to escape the backslash.  For example, `"\"foo\tbar\"\n"` includes quotes at the start and end, a tab `\t` and a newline `\n`.

    (string x1 x2 ... xk)

returns a string concatenation of the specified symbols, strings and/or numbers.  Arguments can be lists containing a sequence of 8-bit character codes (ASCII/UTF-8) to construct a string.

### Lists

Lists are code and data in Lisp.  Syntactically, a dot may be used for the last list element to construct a pair rather than a list.  For example, `'(1 . 2)` is a pair, whereas `'(1 2)` is a list.  By the nature of linked lists, a list after a dot creates a list, not a pair.  For example, `'(1 . (2 . ()))` is the same as `'(1 2)`.  Note that lists form a chain of pairs ending in a `()` nil.  

### Function calls

    (<function> <expr1> <expr2> ... <exprn>)

applies a function to the rest of the list of expresssions as its arguments.  The following are all built-in functions, called "primitives" and "special forms".

### Quoting and unquoting

    (quote <expr>)

protects `<expr>` from evaluation by quoting, same as `'<expr>`.  For example, `'(1 () foo (bar 7))` is a list containing unevaluated expressions protected by the quote.

    (eval <quoted-expr>)

evaluates a quoted expression and returns its value.  For example, `(eval '(+ 1 2))` is 3.

### Constructing and deconstructing pairs and lists

    (cons x y)

constructs a pair `(x . y)` for expressions `x` and `y`.  Lists are formed by chaining sevaral cons pairs, with the empty list `()` as the last `y`.  For example, `(cons 1 (cons 2 ()))` is the same as `'(1 2)`.

    (car <pair>)

returns the first part `x` of a pair `(x . y)` or list.

    (cdr <pair>)

returns the second part `y` of a pair `(x . y)`.  For lists this returns the rest of the list after the first part.

### Arithmetic

    (+ n1 n2 ... nk)
    (- n1 n2 ... nk)
    (* n1 n2 ... nk)
    (/ n1 n2 ... nk)

add, substract, multiply or divide the `n1` by `n2` to `nk`.  Subtraction and division with only one value are treated as special cases such that `(- 2)` is -2 and `(/ 2)` is 0.5.

    (int n)

returns the integer part of a number `n`.

### Logic

    (< x y)

returns `#t` (true) if `x` < `y`.  Otherwise, returns `()` (empty list means false).  The ordering among values of different types is as follows: () < number < primitive < symbol < string < pair/list < closure < macro.  Non-atomic pair/list, closure and macro values are ordered by their cell index, i.e. the location of the first cell pair in the pool, which is consistent during program execution but may differ between program executions.

    (eq? x y)

returns `#t` (true) if values `x` and `y` are identical.  Otherwise, returns `()` (empty list means false).  Numbers, symbols and strings of the same value are always identical, but non-empty lists may or may not be identical even when their values are the same.

    (not x)

returns `#t` if `x` is not `()`.  Otherwise, returns `()` (empty list means false).

    (or x1 x2 ... xk)

returns the value of the first `x` that is not `()`.  Otherwise, returns `()` (empty list means false).  Only evaluates the `x` until the first is not `()`, i.e. the `or` is conditional.

    (and x1 x2 ... xk)

returns the value of the last `x` if all `x` are not `()`.  Otherwise, returns `()` (empty list means false).  Only evaluates the `x` until the first is `()`, i.e. the `and` is conditional.

Note that `(or (and <test> <then>) <else>)` forms an if-then-else.  However, like Lua for example, this is not correct when the `<test>` evluates to true but `<then>` is nil (the `()` empty list.)  In that case `<else>` is evaluated.

### Conditionals

    (cond (x1 y1) (x2 y2) ... (xk yk))

returns the value of `y` corresponding to the first `x` that is not `()` (meaning not false, i.e. true.)  If an `y` is missing then `y` defaults to `()`.  If the `y` are multiple expressions, then all such expressions are evaluated and the value of the last expression is returned.

    (if x y z)

if `x` is not `()` (meaning not false, i.e. true), then return `y` else return `z`.  If `z` is missing then `()` is returned.  If `z` are multiple expressions, then all such expressions are evaluated and the value of the last expression is returned.

### Lambdas

    (lambda <variables> <expr>)

returns an anonymous function "closure" with a list of variables and an expression as its body.  For example, `(lambda (n) (* n n))` squares its argument.  The variables of a lambda may be a single name (not placed in a list) to pass all arguments as a named list.  For example, `(lambda args args)` returns its arguments as a list.  The pair dot may be used to indicate the rest of the arguments.  For example, `(lambda (f x . args) (f . args))` applies a function argument`f` to the arguments `args`, while ignoring `x`.  The closure includes the lexical scope of the lambda, i.e. local names defined in the outer scope can be used in the body.  For example, `(lambda (f x) (lambda args (f x . args)))` is a function that takes function `f` and argument `x` to return a [curried function](https://en.wikipedia.org/wiki/Currying).

### Macros

    (macro <variables> <expr>)

a macro is like a function, except that it does not evaluate its arguments.  Macros typically construct Lisp code that is evaluated when the macro is expanded.  For example, the `defun` macro (see init.lisp) simplifies function definitions `(define defun (macro (f v x) (list 'define f (list 'lambda v x))))` such that `(defun fun (vars...) body)` expands to `(define fun (lambda (vars...) body))` using the convenient Lisp `list` function (see init.lisp) to construct the Lisp code list.

### Globals

    (define <symbol> <expr>)

globally defines a symbol associated with the value of an expression.  If the expression is a function or a macro, then this globally defines the function or macro.

    (assoc <quoted-symbol> <environment>)

returns the value associated with the quoted symbol in the given environment.

    (env)

returns the current environment.  When executed in the REPL, returns the global environment.

### Locals

Locals are declared with the following `let` special forms.  These forms differ slightly in syntax from other Lisp and Scheme implementations, with the aim to make let-forms more intuitive to use (I spent a lot of time debugging my student's Scheme programs as many of them mistakingly forgot to use a list of pairs in the let-forms, so it's time to get rid of that once and for all, but if you don't like it then change this Lisp implementation as you wish):

    (let (v1 x1) (v2 x2) ... (vk xk) y)
    (let* (v1 x1) (v2 x2) ... (vk xk) y)

evaluates `y` with a local scope of bindings for symbols `v` bound to the corresponding values of `x`.  The star versions sequentially bind the symbols from the first to the last, the non-star simultaneously bind.  Note that other Lisp implementations may require placing all `(v x)` in a list, but allow multiple `y` (you can use `begin` instead).

    (letrec (v1 x1) (v2 x2) ... (vk xk) y)
    (letrec* (v1 x1) (v2 x2) ... (vk xk) y)

evaluates `y` with a local scope of recursive bindings for symbols `v` bound to the corresponding values of `x`.  The star versions sequentially bind the symbols from the first to the last, the non-star simultaneously bind.  Note that other Lisp implementations may require placing all `(v x)` in a list, but allow multiple `y` (you can use `begin` instead).

If an `x` is missing then `x` defaults to `()`.  If the `x` are multiple expressions, then all such expressions are evaluated and the value of the last expression is bound to the corresponding `v`.

### Assignments

    (setq <symbol> x)

destructively assigns a globally or locally-bound symbol a new value.

    (set-car! <pair> x)
    (set-cdr! <pair> y)

destructively assigns a pair a new car or cdr value, respectively.

### Input and output

    (load <name>)

loads the specified file name (name is a string or a symbol.)

    (read)

returns the Lisp expression read from input.

    (print x1 x2 ... xk)

prints the expressions.  Strings are quoted.

    (write x1 x2 ... xk)

prints the expressions.  Strings are not quoted.

### Debugging

    (trace <0|1|2>)
    (trace <0|1|2> <expr>)

disables tracing (0), enables tracing (1), and enables tracing with ENTER key press (2).  The first form enables or disables tracing of expression evaluation.  The second form enables or disables tracing of `<expr>` specifically.

### Exceptions

    (catch <expr>)

catch exceptions in the evaluation of an expression, returns the value of the expression or `(ERR . n)` for nonzero error code `n`.

    (throw n)

throws error `n`, where `n` is a nonzero integer.

### Statement sequencing and repetition

    (begin x1 x2 ... xk)

sequentially evaluates expressions, returns the value of the last expression.

    (while x y1 y2 ... yk)

while `x` is not `()` (meaning true), evaluates expressions `y`.  Returns the last value of `yk` or `()` when the loop never ran.

### Type checking

    (type <expr>)

returns a value -1 (nil), 0 (number), 1 (primitive), 2 (symbol), 3 (string), 4 (cons pair), 6 (closure) and 7 (macro) to identify the type of `<expr>`.

### Quit

    (quit)

exits Lisp.

## Library functions and macros

Additional Lisp functions and macros are defined in [init.lisp](src/init.lisp).

    (defun <symbol> <variables> <expr>)

defines a named function with variables and a function body.  A shorthand for `(define <symbol> (lambda <variables> <expr>))`.

    (defmacro <symbol> <variables> <expr>)

defines a named macro with variables and a body.  A shorthand for `(define <symbol> (macro <variables> <expr>))`.

    (null? x)
    (number? x)
    (symbol? x)
    (string? x)
    (pair? x)
    (atom? x)
    (list? x)

returns `#t` if `x` is of a specific type or structure.

    (equal? x y)

returns `#t` if values `x` and `y` are identical or structurally equal.

    (list x1 x2 ... xn)

returns the list of evaluated `x1`, `x2`, ... `xn`.

    (seq n1 n2)
    (range n1 n2 [n3])

returns a list of numbers `n1` up to but excluding `n2`, with an optional step `n3` when specified.

    (length t)
    (append t1 t2)
    (reverse t)
    (member x t)
    (foldr f x t)
    (foldl f x t)
    (min t)
    (max t)
    (filter f t)
    (all? f t)
    (any? f t)
    (mapcar f t)
    (map f t1 t2 ... tn)
    (zip t1 t2 ... tn)

which are the common non-destructive list operations on lists `t` with functions `f` and values `x`.

    (Y f)

is the fixed-point [Y combinator](https://en.wikipedia.org/wiki/Fixed-point_combinator#Fixed-point_combinators_in_lambda_calculus).

    (reveal f)

reveals the contents of `f` by displaying the `lambda` of a closure `f` and the body of a `macro` `f`.

## Lisp memory management

### Memory layout

Like [tinylisp](https://github.com/Robert-van-Engelen/tinylisp), this Lisp interpreter's memory is a single `cell[N]` array of Lisp expressions `L`.  The difference is that this interpreter's memory `cell[]` contains a pool of free and used cons pairs to recycle using garbage collection, a heap to store atoms and strings, and a stack:

    /* array of Lisp expressions, shared by the pool, heap and stack */
    L cell[N];

The size of the pool is parameterized as constant `P`:

    /* number of cells to allocate for the cons pair pool, increase P as desired */
    #define P 8192

The size of the stack with the heap below it is parameterized as constant `S`:

    /* number of cells to allocate for the shared stack and heap, increase S as desired */
    #define S 2048

The total size is constant `N`:

    /* total number of cells to allocate = P+S */
    #define N (P+S)

To access the heap with atoms and strings, we use a byte-addressable address `A`:

    /* base address of the atom/string heap */
    #define A (char*)cell

and an offset `H` such that byte-addressable address `A+H` points to the bottom of the heap immediately above the pool `cell[P]`:

    /* heap address start offset, the heap starts at address A+H immediately above the pool */
    #define H (8*P)

Each atom and string stored in the heap has a special reference field located in front of it.  A reference is an unsigned integer.  We will use this reference field to construct a linked list pointing to `cell[]` with `ATOM` and `STRG` values that point to the same atom or string.  If the linked list is empty, then the atom or string is not used and can be removed from the heap.  The size of this reference field is `R`:

    /* size of the cell reference field of an atom/string on the heap, used by the compacting garbage collector */
    #define R sizeof(I)

The free cells in the pool form a linked list `fp` with the list ending as zero.  The atom and string heap pointer `hp` points to available heap space above the allocated atoms and strings, initially `hp = H`.  The stack grows down from the top of `cell[]` towards the heap, starting with stack pointer `sp = N`.  We also define a `tr` tracing flag:

    /* fp: free pointer points to free cell pair in the pool, next free pair is ord(cell[fp]) unless fp=0
       hp: heap pointer, A+hp points free atom/string heap space above the pool and below the stack
       sp: stack pointer, the stack starts at the top of cell[] with sp=N
       tr: 0 when tracing is off, 1 or 2 to trace Lisp evaluation steps */
    I fp = 0, hp = H, sp = N, tr = 0;

### Consing

To construct a new cons pair `(x . y)` is easy, but we must guard against two potential problems.  First, what to do if the pool is already full?  Second, if it is full and we invoke garbage collection, how do we make sure we do not lose the unprotected `x` and `y` values when garbage collection recycles them?  The `x` and `y` values may be temporary lists for example.  A simple and safe approach is to assume that we have at least one cell pair free to construct `(x . y)`.  If there are no free cells left after that, we invoke garbage collection while protecting the pair `(x . y)` and its constituent `x` and `y`:

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

### Allocating atoms and strings

To allocate bytes on the heap to store atoms and strings, we just need to make sure we have sufficient free heap space available between `hp` and `sp`.  Note that `hp` is single byte addressable and `sp` is 8-byte addressable since `L` is a `double`.  The atom/string bytes are stored after the reference field of width `R`:

    /* allocate n+1 bytes on the heap, returns heap offset of the allocated space */
    I alloc(I n) {
      I i = hp+R;                                   /* free atom/heap is located at hp+R */
      n += R+1;                                     /* n+R+1 is the space we need to reserve */
      if (hp+n > (sp-1) << 3 || ALWAYS_GC) {        /* if insufficient heap space is available, then GC */
        gc();                                       /* GC */
        if (hp+n > (sp-1) << 3)                     /* GC did not free up sufficient heap/stack space */
          err(6);;
        i = hp+R;                                   /* new atom/string is located at hp+R on the heap */
      }
      hp += n;                                      /* update heap pointer to the available space above the atom/string */
      return i;
    }

### Classic mark-sweep garbage collection

Unused cell pairs in the pool are recycled using garbage collection.  One of the simplest algorithms is mark-sweep, developed by John McCarthy.  We need a bit vector to mark cells as used.  Because we mark cell pairs, the number of bits we need is half the pool size:

    /* bit vector corresponding to the pairs of cells in the pool marked 'used' (car and cdr cells are marked together) */
    uint32_t used[(P+63)/64];

To check if the i'th cell is used, its bit is checked with `used[i/64] & 1 << i/2%32)`.  To set the bit for the pair i and i+1 (remember we mark both cells with one bit), we execute `used[i/64] |= 1 << i/2%32`.  With these two ingredients, our mark stage is pretty simple given a root cell i to mark all cells reachable transitively via car `cell[i]` and cdr `cell[i+1]`:

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

The second stage then sweeps the pool to keep the used cell pairs only.  The free list of cell pairs `fp` is constructed from scratch, such that next free pairs are located upwards from the previous free cell pair:

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

The garbage collector resets the bit vector (all cell pairs are initially considered unused).  The first stage marks all cell pairs reachable from the global environment `env` and all cell pairs reachable from the stack.  The second stage sweeps all unusued cell pairs.  A third stage compacts the heap by removing unused atoms and strings:

    /* garbage collector, returns number of free cells in the pool or raises err(7): out of memory */
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

The compacting stage is performed as follows.

### Compacting garbage collection to recycle the atom/string heap

To compact the heap, we construct a linked list of cells that refer to the same atom or string.  This serves two purposes.  First, if the linked list is empty then there are no cells in use that refer to the atom or string.  The atom or string can be removed.  Second, compacting the heap means moving atoms and strings down to keep only the used atoms and strings stored in the heap.  All holes left by unused atoms and strings are filled.  The linked list is traversed to update each `ATOM` and `STRG` cell to reference the new location of the atom or string on the heap:

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

Since the pool and stack share the same `cell[]` array, the linked lists just contain `cell[]` indices to the `ATOM` and `STRG` cells to update during compaction.  The runtime cost is in the order of the number of `ATOM` and `STRG` cells.  Memory overhead of this method is limited to an index per atom/string on the heap of width `R`.  This space is only used during compaction.  It could serve a dual purpose such as a length field of the atom/string e.g. to store binary data that doesn't end with a \0.  This would require marking the length with a bit so that the length can serve as a sentinel instead of `N` to restore the length after compaction.

### Alternative: non-recursive mark-sweep garbage collection using pointer reversal

Non-recursive mark-sweep with pointer reversal has the advantage that no additional memory (a stack) is required.  This is especially important when the call stack size is limited in practice.  After all, a failure in garbage collection is not recoverable.  By constrast, recursion in Lisp `eval` pushes values on the Lisp cell stack and is therefore practically limited.  When the Lisp stack is full a recoverable exception is thrown, but a failure in garbage collection is fatal when the stack is full.

I couldn't find an acceptable example of a mark-sweep garbage collector using pointer reversal.  After tinkering a bit with different variations of the same theme, I came up with the following algorithm and implementation that is both elegant and efficient:

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

The clever bit is that a car cell has an even index and a cdr cell has an odd index in the pool.  We use this observation when going back up the graph.  When going down (the first loop), we visit car-pointed pairs that are not already marked.  If the car cell does not point to a pair or if it is already marked, then we visit the cdr-pointed pair instead until we run out of pairs to visit.  When going back up in the graph (the second loop) we check if the node is a car cell. If so, we should exit the second loop to descent back down (in the first loop) into the car's sibling cdr cell.  The graph traversal ends when we are back at the root at a cdr cell with an odd index.

No additional code changes are needed to the interpreter.  The `sweep` and `gc` functions remain the same.

### How temporary Lisp data is protected from recycling

One small challenge arises when we recycle unused Lisp data.  Whenever we construct temporary data by calling the corresponding C functions `atom`, `string`, `cons` and `pair`, we do not want the data to be accidentily garbage collected.  The `cons` and `pair` C functions automatically protect its arguments while the temporary data is constructed, but not afterwards.  We must protect temporary data when invoking other functions that construct Lisp data, such as `eval` and `evlis`.  To protect temporary Lisp data we push it on the stack and pop it later:

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

Since `push` returns a pointer to the protected value, we can update this stack cell by dereferencing the pointer.  This allows us to efficiently contruct lists without having the push every list value on the stack, as is illustrated by the example further below.  Later we pop the value from the stack:

    /* pop from the stack and return value */
    L pop() {
      return cell[sp++];
    }

In the REPL we can simply unwind the entire stack:

    /* unwind the stack up to position i, where i==N clears the stack */
    void unwind(I i) {
      sp = i;
    }

For example, the `let` special form extends the list of bindings pointed to by `e` with new pairs of bindings for locals:

    L f_let(L t, L *e) {
      L x, *p;
      for (p = push(*e); more(t); t = cdr(t))
        *p = pair(car(car(t)), eval(f_begin(cdr(car(t)), e), *e), *p);
      *e = *p;
      pop();
      return T(t) == NIL ? nil : car(t);
    }

Note that `push` protects the list of bindings pointed to by `p`.  The bindings `*e` passed to `f_let` are already protected by the interpreter, but the pairs we add to the front of the list using the `pair` function won't be protected when `eval` is called.  We create a protected cell on the stack pointed to by `p` and initialized to `*e` with `p = push(*e)`.  We later update stack cell `*p` to protect the new bindings added to the list.  While the arguments to `cons` and `pair` are automatically protected by these functions, this does not suffice to protect the list when `eval` is called and thus requires protecting `*p`.

Note that there is more going on here.  Because the `let` forms support tail-recursive calls, we return the expression `car(t)` to evaluate next by the interpreter in its evaluation loop.  Before we return, the scope `*e` is extended to `*p` with the local bindings.

A `push` can also be used to protect new symbols and strings added the heap by the `atom` and `string` C functions, for example:

    L name = string("John Doe");
    push(name);
    ... do some work, including eval() ...
    pop();

The string contents may be moved around on the heap by the garbage collector.  `A+ord(name)` points to the current location of the \0-terminated string.  By contrast, the cons pair cells of lists are never moved by the garbage collector.

### Memory debugging

To help with debugging garbage collection and find memory management bugs, compile lisp.c with `-DDEBUG` to force garbage collection after each pair construction and atom/string allocation.  This helps to identify temporary Lisp data that may get removed by the collector by accident and therefore should have been protected.  Note that this configuration significantly slows down the interpreter.

The following `dump` function displays the contents of the pool, e.g. when added to the REPL.  To avoid getting huge dumps, change `P` to a small number (e.g. 64, 128 or 256) and comment out some of the `prim[]` entries and remove the init.lisp input to make sure the important primitives and code you want to check fit in memory.

    void dump() {
      I i;
      for (i = 0; i < P; ++i) {
        printf("\n%c %u: ", used[i/64] & 1 << i/2%32 ? '*' : ' ', i);
        if (T(cell[i]) == NIL && ord(cell[i])) printf("FREE -> %u", ord(cell[i++]));
        else if (T(cell[i]) == NIL) printf("()");
        else if (T(cell[i]) == ATOM) printf("%s", A+ord(cell[i]));
        else if (T(cell[i]) == STRG) printf("\"%s\"", A+ord(cell[i]));
        else if (T(cell[i]) == PRIM) printf("<%s>", prim[ord(cell[i])].s);
        else if (T(cell[i]) == CONS) printf("(%u)", ord(cell[i]));
        else if (T(cell[i]) == CLOS) printf("{%u}", ord(cell[i]));
        else if (T(cell[i]) == MACR) printf("[%u]", ord(cell[i]));
        else printf(FLOAT, cell[i]);
      }
      printf("\nenv=%u fp=%u hp=%u sp=%u\n", ord(env), fp, hp, sp);
    }

## Embedding

Embedding a Lisp interpreter should be straightforward by renaming `main()` to `init()` to initialize the interpreter.  Remove the REPL loop from this function.

To parse and execute Lisp code stored in a string, set `ptr` to this string and set `see` to a space, then call `readlisp`, `eval` and perhaps `print` to show the return value:

    see = ' ';
    ptr = "(+ 1 2 3)";
    print(eval(*push(readlisp()), env));

Compiling with `HAVE_READLINE_H` is assumed to allow `ptr` to consume the given string.  If `readline` is not used, then change the `get()` function to remove the `readline` dependency and keep this part:

    if (see != '\n')
      if (!(see = *ptr++))
        see = '\n';

To clear the stack and garbage collect the heap:

    unwind(N); 
    gc()

To expose C functions in Lisp, define wrapper functions and register them in the `prim[]` array.  Pointers can be stored as Lisp integers.  Arbitrary binary data can be stored in strings.

Some examples to get you started:

    /* some kind of state data of a device */
    struct Data {
      enum { RED, GREEN } state;
      float temperature;
    };

    /* two data items (initialized somewhere else) */
    struct Data data[2];

    /* convert the data[2] items to a Lisp string managed by GC */
    L f_data2lisp(L t, L *_) {
      I size = sizeof(data);
      I index = alloc(size);
      memcpy(A + index, data, size);
      return box(STRG, index);
    }

    /* convert Lisp string data back to data[2] */
    L f_lisp2data(L t, L *_) {
      I size = sizeof(data);
      L arg = car(t);
      if (T(arg) == STRG)
        memcpy(data, A + ord(arg), size);
      return nil;
    }

    /* set colors of data[2] stored as Lisp string, e.g. (set-colors data 'RED 'GREEN) */
    L f_set_color(L t, L *_) {
      L arg1 = car(t);
      L arg2 = car(cdr(t));
      L arg3 = car(cdr(cdr(t)));
      if (T(arg1) == STRG && T(arg2) == ATOM && T(arg3) == ATOM) {
        struct Data *ptr = (struct Data*)(A + ord(arg1));
        if (strcmp(A + ord(arg2), "RED") == 0)
          ptr[0].state = RED;
        else
          ptr[0].state = GREEN;
        if (strcmp(A + ord(arg3), "RED") == 0)
          ptr[1].state = RED;
        else
          ptr[1].state = GREEN;
      }
      return nil;
    }

    /* set temperatures of data[2] stored as Lisp string, e.g. (set-temperatures data 45 51.7) */
    L f_set_temperatures(L t, L *_) {
      L arg1 = car(t);
      L arg2 = car(cdr(t));
      L arg3 = car(cdr(cdr(t)));
      if (T(arg1) == STRG) {
        struct Data *ptr = (struct Data*)(A + ord(arg1));
        ptr[0].temperature = (float)num(arg2); /* note that num() can throw an error, as suggested below */
        ptr[1].temperature = (float)num(arg3);
      }
      return nil;
    }

    /* get temperature of data[2] stored as Lisp string, e.g. (get-temperatures data) */
    L f_get_temperatures(L t, L *e) {
      L arg = car(t);
      L res = nil;
      if (T(arg) == STRG) {
        struct Data *ptr = (struct Data*)(A + ord(arg));
        res = cons(ptr[0].temperature, cons(ptr[1].temperature, nil)); 
      }
      return res;
    }

    /* table of Lisp primitives, each has a name s, a function pointer f, and an evaluation mode m */
    struct {
      ...
    } prim[] = {
      ...
      {"data-to-lisp",     f_data2lisp,        NORMAL},
      {"lisp-to-data",     f_lisp2data,        NORMAL},
      {"set-colors",       f_set_color,        NORMAL},
      {"set-temperatures", f_set_temperatures, NORMAL},
      {"get-temperatures", f_get_temperatures, NORMAL},
      ...
      {0}
    };

The `num()` function does not throw an error in the default implementation, when the Lisp argument is non-numeric.  To throw an error change `num()` as follows:

    L num(L n) {
      return n == n ? n : err(5);
    }
