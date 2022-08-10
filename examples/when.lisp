; (when <test> <expr1> <expr2> ... <exprn>) -- if <test> is not () then evaluate all <expr>
; Requires init.lisp

(defmacro when (x . args) (list 'if (list 'not x) () . args))
