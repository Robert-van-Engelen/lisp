; (apply <func> <expr1> <expr2> ... <list>) -- applies <func> to <expr> and <list> as its arguments
; Requires init.lisp
; This is a redundant function because in most cases the list dot operator can be used to accomplish the same
; The '_ symbol is used as a temporary let* variable in the constructed macro code to avoid name clashes
; For example: (apply + 0 1 (list 2 3)) => (let* (_ (list 0 1 3 4)) (+ . _))

(defmacro apply (f . args)
    (letrec*
        (c (lambda (t)
               (if (cdr t)
                   (cons (car t) (c (cdr t)))
                   (eval (car t)))))
        (list 'let* (list '_ (c (cons 'list args))) (cons f '_))))
