; (dolist (<var> <list>) <expr1> ... <exprn>) -- assigns <var> each value of <list> to execute <expr1> to <exprn>
; Requires init.lisp
; (dolist (v '(1 2)) (print v)) => (let* (v) (_ '(1 2)) (while _ (setq v (car _)) (setq _ (cdr _)) (print v))) 
; The '_ symbol is used as a temporary let* variable in the constructed macro code to avoid name clashes

(defmacro dolist (x . args)
    (list 'let*
        (list (car x))
        (list '_ (car (cdr x)))
        (list 'while '_
            (list 'setq (car x) (list 'car '_))
            (list 'setq '_ (list 'cdr '_))
            . args)))
