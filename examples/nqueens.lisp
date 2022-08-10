; n-queens example
; requires init.lisp

; supporting functions

(define make-list
    (lambda (n x)
        (if (< 0 n)
            (cons x (make-list (- n 1) x))
            ())))

; n-queens solver

(define make-board
    (lambda (size)
        (map1
            (lambda () (make-list size '-))
            (seq 0 size))))

(define at
    (lambda (board x y)
        (nth (nth board x) y)))

(define queen!
    (lambda (board x y)
        (set-car! (nthcdr (nth board x) y) '@)))

(define clear!
    (lambda (board x y)
        (set-car! (nthcdr (nth board x) y) '-)))

(define queen?
    (lambda (board x y)
        (eq? (at board x y) '@)))

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
	        (or ; check if there's no conflicting queen up
	            (queen? board n y)
                    ; check upper left
                    (let*
                        (z (+ y (- n x)))
                        (and
                            (not (< z 0))
                            (queen? board n z)))
                    ; check upper right
                    (let* (z (+ y (- x n)))
                        (and
                            (< z board-size)
                            (queen? board n z)))))
            (seq 0 x))))

(define solve-n
    (lambda (board x)
        (if (eq? x board-size)
            ; show solution
            (begin
                (show board)
	        (write "\n"))
            ; continue searching for solutions
            (map1
	        (lambda (y)
		     (if (not (conflict? board x y))
                         (begin
                             (queen! board x y)
                             (solve-n board (+ x 1))
                             (clear! board x y))
                         ()))
                (seq 0 board-size)))))

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
