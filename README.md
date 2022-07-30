# Lisp in 1k lines of readable C, explained

This project is a continuation of the [tinylisp](https://github.com/Robert-van-Engelen/tinylisp) project _"Lisp in 99 lines of C and how to write one yourself."_  If you're interested in writing a Lisp interpreter of your own, then you may want to check out the [tinylisp](https://github.com/Robert-van-Engelen/tinylisp) project first.  If you already did, then welcome back!

_Spoiler alert:_ do not read the C code if you want to give [tinylisp](https://github.com/Robert-van-Engelen/tinylisp) a try and implement some of these features yourself.

The goal of this project is to introduce a Lisp interpreter with modern features, including garbage collection of unused cons pair cells and unused atoms and strings.  There are different ways garbage collection can be performed in Lisp.  I chose mark-sweep, because it is less demanding on memory use and is fairly easy to understand.  By contrast, a copying garbage collector uses double the memory, but has the advantage of being free of recursion and can be interrupted.  However, we can implement mark-sweep with pointer reversal to eliminate recursive calls entirely.  In addition, a compacting garbage collector is used to remove unused atoms and strings from the heap.

A quick glance at the Lisp interpreter's features:

- Lisp with double floating point, atoms, strings, lists, closures, macros
- over 40 built-in Lisp primitives
- lexically-scoped locals, like tinylisp
- exceptions and error handling with safe return to REPL after an error
- execution tracing to display Lisp evaluation steps
- REPL with readline (optional)
- break execution with CTRL-C (optional)
- mark-sweep garbage collector to recycle unused cons pair cells
- compacting garbage collector to recycle unused atoms and strings
- Lisp memory is a single `cell[]` array, no `malloc()`-`free()` calls

## Compilation

    cc -o lisp lisp.c -DHAVE_SIGNAL_H -DHAVE_READLINE_H -lreadline

Without CTRL-C to break and without readline:

    cc -o lisp lisp.c

## Running Lisp

Initialization imports `init.lisp` when located in the working directory.  Otherwise this step is skipped.

You can load Lisp source files with `(load "name")`, for example

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

The prompt displays the number of free cons pair cells + free stack cells.  The heap and stack are located in the same memory space.  Therefore, the second number is also indicative of the size of the heap available to store new atoms and strings.
