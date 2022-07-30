# Lisp in 1k lines of readable C, explained

This project is a continuation of the [tinylisp](https://github.com/Robert-van-Engelen/tinylisp) project _"Lisp in 99 lines of C and how to write one yourself."_  If you're interested in writing a Lisp interpreter of your own, then you may want to check out the [tinylisp](https://github.com/Robert-van-Engelen/tinylisp) project first.  If you already did, then welcome back!

_Spoiler alert: do not read the C code of this project if you want to give [tinylisp](https://github.com/Robert-van-Engelen/tinylisp) a try first to implement some of these features yourself._

**TL;DR** what are [the language features of this Lisp?](#lisp-language-features)

I've documented this project's C source code more extensively to explain the inner workings of the interpreter.

This project introduces a Lisp interpreter with modern features, including a [tracing garbage collector](https://en.wikipedia.org/wiki/Tracing_garbage_collection) to recycle unused cons pair cells and unused atoms and strings.  There are different methods of garbage collection that can be used by a Lisp interpreter.  I chose the simple mark-sweep method, because it is fairly easy to understand.  By contrast, a copying garbage collector uses double the memory, but has the advantage of being free of recursion and can be interrupted.  However, we can implement mark-sweep with pointer reversal to eliminate recursive calls entirely.  Either way, it is fairly easy to replace the garbage collector.  In addition to mark-sweep, a compacting garbage collector is used to remove unused atoms and strings from the heap.

A quick glance at the Lisp interpreter's features:

- Lisp with double floating point, atoms, strings, lists, closures, macros
- over 40 built-in Lisp primitives
- lexically-scoped locals, like tinylisp
- exceptions and error handling with safe return to REPL after an error
- execution tracing to display Lisp evaluation steps
- REPL with GNU readline (optional)
- break execution with CTRL-C (optional)
- mark-sweep garbage collector to recycle unused cons pair cells
- compacting garbage collector to recycle unused atoms and strings
- Lisp memory is a single `cell[]` array, no `malloc()`-`free()` calls
- easily customizable and extendable to add new special features
- integrates with C and C++ code by calling C functions for Lisp primitives, e.g. for embedding a Lisp interpreter

## Compilation

Just one source code file [lisp.c](src/lisp.c) to compile:

    cc -o lisp lisp.c -DHAVE_SIGNAL_H -DHAVE_READLINE_H -lreadline

Without CTRL-C to break execution and without the [GNU readline](https://en.wikipedia.org/wiki/GNU_Readline) library:

    cc -o lisp lisp.c

## Running Lisp

Initialization imports `init.lisp` first, when located in the working directory.  Otherwise this step is skipped.  You can load Lisp source files with `(load "name.lisp")`, for example

    bash# ./lisp
    ...
    defun
    6552+1933>(load "nqueens.lisp")
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
    5718+1904>

The prompt displays the number of free cons pair cells + free stack cells available.  The heap and stack are located in the same memory space.  Therefore, the second number is also indicative of the size of the heap available to store new atoms and strings.

## Lisp language features

Symbols (atoms) consist of any sequence of non-space characters, excluding `(`, `)`, `'` and `"`.  Strings are quoted and may contain `\a`, `\b`, `\t`, `\n`, `\v`, `\f` and `\r` escapes.  Use `\"` to escape quote.

    (<function> <expr1> <expr2> ... <exprn>)

Applies a function to expresssions as its arguments.  The following are all built-in functions called "primitives" and "special forms".

    (type <expr>)

Returns a value between 0 and 9 to identify the type of `<expr>`.

    (eval <quoted-expr>)

Evaluates `<uoted-expr>` and returns its value.  For example, `(eval '(+ 1 2))` is 3.

    (quote <expr>)

Protects `<expr>` from evaluation, same as `'<expr>`.  For example, `'(1 () foo (bar 7))` is a list, not a function application.

    (cons x y)

Constructs a pair `(x . y)`.  Lists are formed by chaining more pairs, with the empty list `()` as the last `y`.  For example, `(cons 1 (cons 2 ()))` is the same as `'(1 2)`.

    (car <pair>)

Returns the first part `x` of a pair `(x . y)`.

    (cdr <pair>)

Returns the second part `y` of a pair `(x . y)`.

    (+ n1 n2 ... nk)
    (- n1 n2 ... nk)
    (* n1 n2 ... nk)
    (/ n1 n2 ... nk)

 Add, substract, multiply and divide `n1` to `nk`.  Note that `(- 2)` is 2, not -2.
 
    (int n)

Returns the integer part of a number `n`.

    (< n1 n2)

Return `#t` (true) if numbers `n1` < `n2`.  Otherwise, return `()` (empty list means false).

    (eq? x y)

Returns `#t` (true) if values `x` and `y` are identical.  Otherwise, returns `()` (empty list means false).  Numbers and atoms with the same value are always identical, but strings and non-empty lists may not be identical even when their values are the same.

    (or x1 x2 ... xk)

Returns `#t` if any of the `x` is not `()`.  Otherwise, returns `()` (empty list means false).

    (and x1 x2 ... xk)

Returns `#t` if all of the `x` are not `()`.  Otherwise, returns `()` (empty list means false).

    (not x)

Returns `#t` if `x` is not `()`.  Otherwise, returns `()` (empty list means false).

    (cond (x1 y1) (x2 y2) ... (xk yk))

Returns the `y` corresponding to the first `x` that is not `()` (meaning not false, i.e. true).

    (if x y z)

If `x` is not `()` (meaning not false, i.e. true), then return `y` else return `z`.

    (lambda <parameters> <expr>)

An anonymous function with a list of parameters and a body expression.  For example, `(lambda (n) (* n n))` squares its argument.

    (macro <parameters> <expr>)

A macro is like a function, except that it does not evaluate its arguments.  Macros typically construct Lisp code that is evaluated when the macro is expanded.

    (define <symbol> <expr>)

Globally defines a symbol associated with the value of an expression.  If the expression is a function or macro, this globally defines the function or macro.

    (assoc <quoted-symbol> <environment>)

Returns the value associated with the quoted symbol in the given environment.

    (env)

Returns the current environment.  When executed at the REPL, returns the global environment.

    (let (v1 x1) (v2 x2) ... (vk xk) y)
    (let* (v1 x1) (v2 x2) ... (vk xk) y)

Evaluates `y` with a local scope of bindings for symbols `v` bound to the values of `x`.  The star versions sequentially bind the symbols from the first to the last, the non-star simultaneously bind.

    (letrec (v1 x1) (v2 x2) ... (vk xk) y)
    (letrec* (v1 x1) (v2 x2) ... (vk xk) y)

Evaluates `y` with a local scope of recursive bindings for symbols `v` bound to the values of `x`.  The star versions sequentially bind the symbols from the first to the last, the non-star simultaneously bind.

    (setq <symbol> x)

Assigns a bound symbol a new value.

    (set-car! <pair> x)
    (set-cdr! <pair> y)

Assigns a pair a new car or cdr value.

    (read)

Returns the Lisp expression read from input.

    (print x1 x2 ... xk)

Prints the expressions.  Strings are quoted.

    (write x1 x2 ... xk)

Prints the expressions.  Strings are unquoted.

    (string x1 x2 ... xk)

Returns a string concatenation of symbols, strings and numbers.  Lists may contain character codes that are converted to a string.

    (load <name>)

Loads the specified file name specified as a string or symbol.

    (trace <0|1|2>)

Disables tracing (0), enables (1) and enables with key presses (2).

    (catch <expr>)

Catch exceptions in the evaluation of an expression, returns the value of the expression or `(ERR . n)` for error `n`.

    (throw n)

Throws error `n`, which must be a positive constant.

    (begin x1 x2 ... xk)

Sequentially valuates expressions, returns the value of the last expression.

    (while x y1 y2 ... yk)

While `x` is not `()` (meaning true), evaluates expressions `y`.  Returns the last value of `y` or `()` when the loop never ran.

    (quit)

Exits Lisp.

Additional Lisp functions and macros are defined in `init.lisp`.

## More?

Yes, there will be more to come soon.  Stay tuned.
