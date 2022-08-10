; (case <expr> (<key1> <expr1>) (<key2> <expr2>) ... (<keyn> <exprn>)) -- return value of the expression matching the key (matches with eq?)
; Requires init.lisp
; The '_ symbol is used as a temporary let* variable in the constructed macro code to avoid name clashes

(defmacro case (x . args)
    (letrec*
        (c (lambda (t)
               (if t
                   (list 'if (list 'eq? '_ (car (car t))) (car (cdr (car t))) (c (cdr t)))
                   ())))
        (list 'let* (list '_ x) (c args))))
