; (apply <func> <expr1> <expr2> ... <list>) -- applies <func> to <expr> and <list> as its arguments
; Requires init.lisp
; This is a redundant function because in most cases the dot operator can be used to accomplish the same
; The '_ symbol is used as a temporary let* variable in the constructed macro code to avoid name clashes
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
