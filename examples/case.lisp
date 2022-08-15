; (case <expr> (<key1> <expr1>) (<key2> <expr2>) ... (<keyn> <exprn>)) -- return value of the expression matching the key (matches with eq?)
; Requires init.lisp
; This macro constructs a (let* (_ <expr>) (cond (eq? _ <key1>) <expr1> ... (#t))) to expand (case ...)
; The '_ symbol is used as a temporary let* variable in the constructed macro code to avoid name clashes

(defmacro case (x . args)
    (letrec*
        (c (lambda (t)
            (if t
                (cons (cons (list 'eq? '_ (car (car t))) (cdr (car t))) (c (cdr t)))
                (cons (cons #t ())()))))
        (list 'let* (list '_ x) (cons 'cond (c args)))))
