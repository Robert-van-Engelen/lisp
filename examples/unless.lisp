; (unless <test> <expr1> <expr2> ... <exprn>) -- if <test> is () then evaluate all <expr>
; Requires init.lisp

(defmacro unless (x . args) (list 'if x () . args))

