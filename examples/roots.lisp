; the quadratic formula
; (quad-solver)
; Requires init.lisp

; (sqrt n) -- solve x^2 - n = 0 with Newton method using the Y combinator to recurse
; ... we could add math.h sqrt() as a Lisp primitive, but what's the fun in that?
(defun sqrt (n)
    ((Y (lambda (f)
            (lambda (x)
                (let*
                    (y (- x (/ (- x (/ n x)) 2)))
                    (if (eq? x y)
                        x
                        (f y))))))
     n))

; compute the roots of ax^2 + bx + c
(defun roots (a b c)
    (cond
        ; numeric coefficients?
        ((not (and (number? a) (number? b) (number? c)))
            (write "invalid coefficients\n"))
        ; a, b, c are zero?
        ((and (eq? a 0) (eq? b 0) (eq? c 0))
            (write "infinite solutions\n"))
        ; if a and b are zero then no x
        ((and (eq? a 0) (eq? b 0))
           (write "no solution\n"))
        ; if a is zero then solve bx + c = 0
        ((eq? a 0)
            (/ (- c) b))
        ; general case
        (#t
            (let
                ; d = b^2 - 4ac
                (d (- (* b b) (* 4 a c)))
                (cond
                    ; b^2 == 4ac
                    ((eq? d 0)
                        (/ (- b) (* 2 a)))
                    ; two complex roots if b^2 < 4ac
                    ((< d 0)
                         (write "complex roots\n"))
                    ; two roots
                    (#t
                        (list
                            (/ (- (sqrt d) b) (* 2 a))
                            (/ (- (+ (sqrt d) b)) (* 2 a)))))))))

; interactive solver
(defun quad-solver ()
    (begin
        (write "Enter coefficients a b and c for ax^2 + bx + c\n")
        (let*
            (a (read))
            (b (read))
            (c (read))
            (res (roots a b c))
            (begin
                (write "The roots for " a "x^2 + " b "x + " c " = 0 are: ")
                (if (pair? res)
                    (write (car res) " and " (car (cdr res)))
                    (write res "\n"))))))

; let's try it out, not that (read) reads this file so it has to be the last line to detect EOF
(quad-solver)
