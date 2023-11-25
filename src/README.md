# Lisp interpreter project source code

- The Lisp interpreter in 1k lines of C: [lisp.c](lisp.c)
- Alternative version with non-recursive mark-sweep pointer reversal: [lisp-pr.c](lisp-pr.c)
- Alternative version with single precision float and non-recursive mark-sweep pointer reversal: [lisp-pr-single.c](lisp-pr-single.c)
- C++17 header-file-only version: [lisp.hpp](lisp.hpp) and C++17 REPL main [lisp-repl.cpp](lisp-repl.cpp)
- Optional Lisp functions and macros imported by the Lisp interpreter: [init.lisp](init.lisp)

# Compiling C

    $ cc -o lisp lisp.c -O2 -DHAVE_SIGNAL_H -DHAVE_READLINE_H -lreadline

Without CTRL-C to break and without the [GNU readline](https://en.wikipedia.org/wiki/GNU_Readline) library:

    $ cc -o lisp lisp.c -O2

# Compiling C++17

    $ c++ -std=c++17 -o lisp lisp-repl.cpp -O2 -DHAVE_SIGNAL_H -DHAVE_READLINE_H -lreadline

Without CTRL-C to break and without the [GNU readline](https://en.wikipedia.org/wiki/GNU_Readline) library:

    $ c++ -std=c++17 -o lisp lisp-repl.cpp -O2
