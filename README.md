# Lisp in under 1k lines of C, explained

This project is a continuation of the [tinylisp](https://github.com/Robert-van-Engelen/tinylisp) project _"Lisp in 99 lines of C and how to write one yourself."_  If you're interested in writing a Lisp interpreter of your own, then you may want to check out the [tinylisp](https://github.com/Robert-van-Engelen/tinylisp) project first.  If you already did, then welcome back!

_Spoiler alert: do not read the C code of this project if you want to give [tinylisp](https://github.com/Robert-van-Engelen/tinylisp) a try first to implement some of these features yourself._

A quick glance at this Lisp interpreter's features:

- Lisp with double floating point, atoms, strings, lists, closures, macros
- over 40 built-in Lisp primitives
- lexically-scoped locals, like tinylisp
- exceptions and error handling with safe return to REPL after an error
- execution tracing to display Lisp evaluation steps
- load Lisp source code files
- REPL with GNU readline (optional)
- break execution with CTRL-C (optional)
- mark-sweep garbage collector to recycle unused cons pair cells
- compacting garbage collector to recycle unused atoms and strings
- Lisp memory is a single `cell[]` array, no `malloc()` and `free()` calls
- easily customizable and extendable to add new special features
- integrates with C and C++ code by calling C functions for Lisp primitives, e.g. for embedding a Lisp interpreter

I've documented this project's C source code extensively to explain the inner workings of the interpreter.  This Lisp interpreter includes a [tracing garbage collector](https://en.wikipedia.org/wiki/Tracing_garbage_collection) to recycle unused cons pair cells and unused atoms and strings.  There are different methods of garbage collection that can be used by a Lisp interpreter.  I chose the simple mark-sweep method, because it is fairly easy to understand.  By contrast, a copying garbage collector requires double the memory, but has the advantage of being free of recursion (no call stack) and can be interrupted.  However, we can implement mark-sweep with pointer reversal to eliminate recursive calls entirely.  An advantage of mark-sweep is that Lisp data is never moved in memory and can be consistently referenced by other C/C++ code.  In addition to mark-sweep, a compacting garbage collector is used to remove unused atoms and strings from the heap.

## Is it really Lisp?

Like [tinylisp](https://github.com/Robert-van-Engelen/tinylisp), this project preserves the original meaning and flavor of McCarthy's Lisp as much as possible.

    > (define curry
          (lambda (f x)
              (lambda args
                  (f x . args))))
    > ((curry + 1) 2 3)
    6

If your Lisp can't curry like this, it isn't classic Lisp.

## Compilation

Just one source code file [lisp.c](src/lisp.c) to compile:

    $ cc -o lisp lisp.c -DHAVE_SIGNAL_H -DHAVE_READLINE_H -lreadline

Without enabling CTRL-C to break execution and without the [GNU readline](https://en.wikipedia.org/wiki/GNU_Readline) library:

    $ cc -o lisp lisp.c

## Running Lisp

Initialization imports `init.lisp` first, when located in the working directory.  Otherwise this step is skipped.  You can load Lisp source files with `(load "name.lisp")`, for example

    $ ./lisp
    ...
    defun
    6568+1934>(load "nqueens.lisp")
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
    ()
    5734+1906>

The prompt displays the number of free cons pair cells + free stack cells available.  The heap and stack are located in the same memory space.  Therefore, the second number is also indicative of the size of the heap space available to store new atoms and strings.

## Lisp language features

Lisp symbols consist of a sequence of non-space characters, excluding `(`, `)`, `'` and `"`.  When used in a Lisp expression, a symbol is looked-up for its value, like a variable typically refers to its value.  Symbols can be quoted (see below) to use symbols literally.

Strings are quoted and may contain `\a`, `\b`, `\t`, `\n`, `\v`, `\f` and `\r` escapes.  Use `\"` to escape the quote and `\\` to escape the backslash.

Lists are code and data in Lisp.  Syntactically, a dot may be used for the last list element to construct a pair rather than a list that ends with a nil `()`.  For example, `'(1 . 2)` is a pair, whereas `'(1 2)` is a list.  By the nature of linked lists, a list after a dot creates a list, not a pair.  For example, `'(1 . (2 . ()))` is the same as `'(1 2)`.

    (<function> <expr1> <expr2> ... <exprn>)

applies a function to the rest of the list of expresssions as its arguments.  The following are all built-in functions, called "primitives" and "special forms".

    (type <expr>)

returns a value between 0 and 9 to identify the type of `<expr>`.

    (eval <quoted-expr>)

evaluates a quoted expression and returns its value.  For example, `(eval '(+ 1 2))` is 3.

    (quote <expr>)

protects `<expr>` from evaluation by quoting, same as `'<expr>`.  For example, `'(1 () foo (bar 7))` is a list containing unevaluated expressions protected by the quote.

    (cons x y)

constructs a pair `(x . y)` for expressions `x` and `y`.  Lists are formed by chaining sevaral cons pairs, with the empty list `()` as the last `y`.  For example, `(cons 1 (cons 2 ()))` is the same as `'(1 2)`.

    (car <pair>)

returns the first part `x` of a pair `(x . y)`.

    (cdr <pair>)

returns the second part `y` of a pair `(x . y)`.

    (+ n1 n2 ... nk)
    (- n1 n2 ... nk)
    (* n1 n2 ... nk)
    (/ n1 n2 ... nk)

add, substract, multiply and divide `n1` to `nk`.  Subtraction and division with only one value are treated as special cases such that `(- 2)` is -2 and `(/ 2)` is 0.5.
 
    (int n)

returns the integer part of a number `n`.

    (< n1 n2)

returns `#t` (true) if numbers `n1` < `n2`.  Otherwise, returns `()` (empty list means false).

    (eq? x y)

returns `#t` (true) if values `x` and `y` are identical.  Otherwise, returns `()` (empty list means false).  Numbers and atoms with the same value are always identical, but strings and non-empty lists may not be identical even when their values are the same.

    (or x1 x2 ... xk)

returns `#t` if any of the `x` is not `()`.  Otherwise, returns `()` (empty list means false).  Only evaluates the `x` until the first is not `()`, i.e. the `or` is conditional.

    (and x1 x2 ... xk)

returns `#t` if all of the `x` are not `()`.  Otherwise, returns `()` (empty list means false).  Only evaluates the `x` until the first is `()`, i.e. the `and` is conditional.

    (not x)

returns `#t` if `x` is not `()`.  Otherwise, returns `()` (empty list means false).

    (cond (x1 y1) (x2 y2) ... (xk yk))

returns the `y` corresponding to the first `x` that is not `()` (meaning not false, i.e. true).

    (if x y z)

if `x` is not `()` (meaning not false, i.e. true), then return `y` else return `z`.

    (lambda <parameters> <expr>)

an anonymous function with a list of parameters and an expression as its body.  For example, `(lambda (n) (* n n))` squares its argument.  The parameters may be a single name without list to catch the arguments as a list.  For example, `(lambda args args)` returns its arguments as a list.  The pair dot may be used to indicate the rest of the arguments.  For example, `(lambda (f x . args) (f . args))` applies a function argument`f` to the arguments `args`, while ignoring `x`.

    (macro <parameters> <expr>)

a macro is like a function, except that it does not evaluate its arguments.  Macros typically construct Lisp code that is evaluated when the macro is expanded.

    (define <symbol> <expr>)

globally defines a symbol associated with the value of an expression.  If the expression is a function or a macro, then this globally defines the function or macro.

    (assoc <quoted-symbol> <environment>)

returns the value associated with the quoted symbol in the given environment.

    (env)

returns the current environment.  When executed in the REPL, returns the global environment.

    (let (v1 x1) (v2 x2) ... (vk xk) y)
    (let* (v1 x1) (v2 x2) ... (vk xk) y)

evaluates `y` with a local scope of bindings for symbols `v` bound to the values of `x`.  The star versions sequentially bind the symbols from the first to the last, the non-star simultaneously bind.  Note that other Lisp implementations may require placing all `(v x)` in a list, but allow multiple `y` (you can use `begin` instead).

    (letrec (v1 x1) (v2 x2) ... (vk xk) y)
    (letrec* (v1 x1) (v2 x2) ... (vk xk) y)

evaluates `y` with a local scope of recursive bindings for symbols `v` bound to the values of `x`.  The star versions sequentially bind the symbols from the first to the last, the non-star simultaneously bind.  Note that other Lisp implementations may require placing all `(v x)` in a list, but allow multiple `y` (you can use `begin` instead).

    (setq <symbol> x)

assigns a globally or locally-bound symbol a new value.

    (set-car! <pair> x)
    (set-cdr! <pair> y)

assigns a pair a new car or cdr value, respectively.

    (read)

returns the Lisp expression read from input.

    (print x1 x2 ... xk)

prints the expressions.  Strings are quoted.

    (write x1 x2 ... xk)

prints the expressions.  Strings are not quoted.

    (string x1 x2 ... xk)

returns a string concatenation of symbols, strings and numbers.  Lists may contain 8-bit character codes (ASCII/UTF-8) that are converted to a string.

    (load <name>)

loads the specified file name specified as a string or symbol.

    (trace <0|1|2>)

disables tracing (0), enables tracing (1) and enables tracing with ENTER press (2).

    (catch <expr>)

catch exceptions in the evaluation of an expression, returns the value of the expression or `(ERR . n)` for positive error code `n`.

    (throw n)

throws error `n`, where `n` must be a positive constant.

    (begin x1 x2 ... xk)

sequentially evaluates expressions, returns the value of the last expression.

    (while x y1 y2 ... yk)

while `x` is not `()` (meaning true), evaluates expressions `y`.  Returns the last value of `yk` or `()` when the loop never ran.

    (quit)

exits Lisp.

Additional Lisp functions and macros are defined in [init.lisp](src/init.lisp).

## More?

Yes, there will be more to come soon.  Stay tuned.
