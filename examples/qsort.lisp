; (qsort <list>) -- quicksort (not the most efficient implementation possible)
; For example, (qsort '(3 4 1 2 5)) => (1 2 3 4 5)
; Requires init.lisp

(define qsort
    (lambda (t)
        (if t
            (let*
                (p (partition (car t) (cdr t)))
                (append (qsort (car p)) (cons (car t) (qsort (cdr p)))))
            ())))

; (partition <pivot> <list>) => (<less-than-pivot> . <greater-or-equal-to-pivot>) -- partitions <list> on <pivot>
(define partition
    (lambda (x t)
        (if t
            (let*
                (p (partition x (cdr t)))
                ; less-than-pivot x
                (l (car p))
                ; greater-or-equal-to-pivot x
                (g (cdr p))
                ; put (car t) in front of l or g then return (l . g)
                (if (< (car t) x)
                    (cons (cons (car t) l) g)
                    (cons l (cons (car t) g))))
            (cons () ()))))

; this version uses destructive assignments
(define quick
    (lambda (t)
        (if t
            (let*
                ; pivot x
                (x (car t))
                ; less-than-pivot x accumulated in the while loop
                (l ())
                ; greater-or-equal-to-pivot x accumulated in the while loop
                (g ())
                (begin
                    (while (setq t (cdr t))
                        (if (< (car t) x)
                            (setq l (cons (car t) l))
                            (setq g (cons (car t) g))))
                    (append (quick l) (cons x (quick g))))))))
