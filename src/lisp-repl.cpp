// lisp-repl.cpp C++17 REPL demo by Robert A. van Engelen 2022 BSD-3 license
// To enable readline: c++ -std=c++17 -o lisp lisp-repl.cpp -O2 -DHAVE_READLINE_H -lreadline
// To enable break with CTRL-C: c++ -std=c++17 -o lisp lisp-repl.cpp -O2 -DHAVE_SIGNAL_H -DHAVE_READLINE_H -lreadline

#include "lisp.hpp"

// a small Lisp interpreter with 8192 cells pool and 2048 cells stack/heap
typedef Lisp<8192,2048> MySmallLisp;

int main(int argc, char **argv) {
  printf("lisp");
  MySmallLisp lisp;
  lisp.input(argc > 1 ? argv[1] : "init.lisp");
  using_history();
  int i = GETSIGINT(lisp);      // when compiling with -DHAVE_SIGINT_H we must set this handler!
  if (i) {
    lisp.closein();
    printf("ERR %d %s", i, lisp.error(i));
  }
  while (1) {
    putchar('\n');
    lisp.unwind();
    lisp.prompt("%u+%u>");
    try {
      lisp.print(lisp.eval(*lisp.push(lisp.read()), lisp.env));
    }
    catch (int i) {
      lisp.closein();
      printf("ERR %d: %s", i, lisp.error(i));
    }
    catch (MySmallLisp::QUIT) {
      printf("Bye!\n");
      break;
    }
  }
}
