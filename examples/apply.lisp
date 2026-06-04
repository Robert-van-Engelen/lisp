; (apply <func> <expr1> <expr2> ... <list>) -- applies <func> to <expr> and <list> as its arguments
; Requires init.lisp
; Whereas other Lisp require an apply function, our Lisp does not, but we
; provide one here as a macro.
;
; The dot operator accomplishes the same as the apply function when combined
; with a let* to bind the list to the variable after the dot, which this macro
; constructs.  In the macro body we use the '_ symbol as a temporary let*
; variable to avoid potential clashes with names in the macro arguments.
;
; For example: (apply + 0 1 (list 2 3)) => (let* (_ (list 3 4)) (+ 0 1 . _))

(defmacro apply fargs
    (letrec*
        (last (lambda (t)
            (if (cdr t)
                (last (cdr t))
                t)))
        (app (lambda (t s)
            (if (cdr t)
                (cons (car t) (app (cdr t) s))
                s)))
        (list 'let* (cons '_ (last fargs)) (app fargs '_))))
