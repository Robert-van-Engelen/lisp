# Lisp in 1k lines of readable C, explained

This project is a continuation of the [tinylisp](https://github.com/Robert-van-Engelen/tinylisp) project _"Lisp in 99 lines of C and how to write one yourself."_  If you're interested in writing a Lisp interpreter of your own, then you may want to check out the [tinylisp](https://github.com/Robert-van-Engelen/tinylisp) project first.  If you already did, then welcome back!

_Spoiler alert: do not read the C code of this project if you want to give [tinylisp](https://github.com/Robert-van-Engelen/tinylisp) a try first to implement some of these features yourself._

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

## Built-in Lisp primitives

    (type x)
    (eval <quoted-expr>)
    (quote <expr>)
    (cons x y)
    (car <pair>)
    (cdr <pair>)
    (+ n1 n2 ... nk)
    (- n1 n2 ... nk)
    (* n1 n2 ... nk)
    (/ n1 n2 ... nk)
    (int <integer.frac>)
    (< n1 n2)
    (eq? x y)
    (or x1 x2 ... xk)
    (and x1 x2 ... xk)
    (not x)
    (cond (x1 y1) (x2 y2) ... (xk yk))
    (if x y z)
    (lambda <parameters> <expr>)
    (macro <parameters> <expr>)
    (define <symbol> <expr>)
    (assoc <quoted-symbol> <environment>)
    (env)
    (let (v1 x1) (v2 x2) ... (vk xk) y)
    (let* (v1 x1) (v2 x2) ... (vk xk) y)
    (letrec (v1 x1) (v2 x2) ... (vk xk) y)
    (letrec* (v1 x1) (v2 x2) ... (vk xk) y)
    (setq <symbol> x)
    (set-car! <pair> x)
    (set-cdr! <pair> y)
    (read)
    (print x1 x2 ... xk)
    (write x1 x2 ... xk)
    (string x1 x2 ... xk)
    (load <name>)
    (trace <0|1|2>)
    (catch <expr>)
    (throw <number>)
    (begin x1 x2 ... xk)
    (while x y1 y2 ... yk)
    (quit)

## More?

Yes, there will be more to come soon.  Stay tuned.
