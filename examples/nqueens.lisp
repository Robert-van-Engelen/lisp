; n-queens example
; requires init.lisp
; loosely based on: https://github.com/rui314/minilisp/blob/master/examples/nqueens.lisp

; supporting functions

(define nth
    (lambda (t n)
        (if (eq? n 0)
            (car t)
            (nth (cdr t) (- n 1)))))

(define nth-tail
    (lambda (t n)
        (if (eq? n 0)
            t
            (nth-tail (cdr t) (- n 1)))))

(define iota
    (lambda (n)
        (seq 0 n)))

(define make-list
    (lambda (n x)
        (if (< 0 n)
            (cons x (make-list (- n 1) x))
            ())))

(define for-each
    (lambda (f t)
        (if t
            (begin
                (f (car t))
                (for-each f (cdr t)))
            ())))

; n-queens solver

(define make-board
    (lambda (size)
        (map1
            (lambda () (make-list size '-))
            (iota size))))

(define get
    (lambda (board x y)
        (nth (nth board x) y)))

(define set
    (lambda (board x y)
        (set-car! (nth-tail (nth board x) y) '@)))

(define clear
    (lambda (board x y)
        (set-car! (nth-tail (nth board x) y) '-)))

(define set?
    (lambda (board x y)
        (eq? (get board x y) '@)))

(define show
    (lambda (board)
        (if board
            (begin
                (write (car board) "\n")
                (show (cdr board)))
            ())))

(define conflict?
    (lambda (board x y)
        (any?
            (lambda (n)
	        (or ; check if there's no conflicting queen upward
	            (set? board n y)
                    ; upper left
                    (let*
                        (z (+ y (- n x)))
                        (and
                            (not (< z 0))
                            (set? board n z)))
                    ; upper right
                    (let* (z (+ y (- x n)))
                        (and
                            (< z board-size)
                            (set? board n z)))))
            (iota x))))

(define solve-n
    (lambda (board x)
        (if (eq? x board-size)
            ;; Problem solved
            (begin
                (show board)
	        (write "\n"))
            (for-each
	        (lambda (y)
		     (if (not (conflict? board x y))
                         (begin
                             (set board x y)
                             (solve-n board (+ x 1))
                             (clear board x y))
                         ()))
                (iota board-size)))))

(define solve
    (lambda (board)
        (begin
            (write "start\n")
            (solve-n board 0)
            (write "done\n"))))

; create an 8x8 board and solve it

(define board-size 8)
(define board (make-board board-size))
(solve board)
