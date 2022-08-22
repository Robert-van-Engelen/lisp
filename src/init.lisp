(define null? not)
(define number? (lambda (x) (eq? (type x) 0)))
(define symbol? (lambda (x) (eq? (type x) 2)))
(define string? (lambda (x) (eq? (type x) 3)))
(define pair? (lambda (x) (eq? (type x) 4)))
(define atom? (lambda (x) (not (pair? x))))
(define list?
    (lambda (x)
        (or
            (not x)
            (if (pair? x)
                (list? (cdr x))
                ()))))
(define equal?
    (lambda (x y)
        (or
            (eq? x y)
            (and
                (pair? x)
                (pair? y)
                (equal? (car x) (car y))
                (equal? (cdr x) (cdr y))))))
(define > (lambda (x y) (< y x)))
(define <= (lambda (x y) (not (< y x))))
(define >= (lambda (x y) (not (< x y))))
(define = (lambda (x y) (eq? (- x y) 0)))
(define list (lambda args args))

(define abs
    (lambda (n)
        (if (< n 0)
            (- 0 n)
            n)))
(define frac (lambda (n) (- n (int n))))
(define truncate int)
(define floor
    (lambda (n)
        (int
            (if (< n 0)
                (- n 1)
                n))))
(define ceiling (lambda (n) (- 0 (floor (- 0 n)))))
(define round (lambda (n) (floor (+ n 0.5))))
(define mod (lambda (n m) (- n (* m (int (/ n m))))))
(define gcd
    (lambda (n m)
        (if (eq? m 0)
            n
            (gcd m (mod n m)))))
(define lcm (lambda (n m) (/ (* n m) (gcd n m))))
(define even? (lambda (n) (eq? (mod n 2) 0)))
(define odd? (lambda (n) (eq? (mod n 2) 1)))

(define length
    (lambda (t)
        (if t
            (+ 1 (length (cdr t)))
            0)))
(define append1
    (lambda (s t)
        (if s
            (cons (car s) (append1 (cdr s) t))
            t)))
(define append
    (lambda (t . args)
        (if args
            (append1 t (append . args))
            t)))
(define nthcdr
    (lambda (t n)
        (if (eq? n 0)
            t
            (nthcdr (cdr t) (- n 1)))))
(define nth (lambda (t n) (car (nthcdr t n))))
(define rev1
    (lambda (r t)
        (if t
            (rev1 (cons (car t) r) (cdr t))
            r)))
(define reverse (lambda (t) (rev1 () t)))
(define member
    (lambda (x t)
        (if t
            (if (equal? x (car t))
                t
                (member x (cdr t)))
            t)))
(define foldr
    (lambda (f x t)
        (if t
            (f (car t) (foldr f x (cdr t)))
            x)))
(define foldl
    (lambda (f x t)
        (if t
            (foldl f (f (car t) x) (cdr t))
            x)))
(define min
    (lambda args
        (foldl
            (lambda (x y)
                (if (< x y)
                    x
                    y))
            inf
            args)))
(define max
    (lambda args
        (foldl (lambda (x y)
            (if (< x y)
                y
                x))
        -inf
        args)))
(define filter
    (lambda (f t)
        (if t
            (if (f (car t))
                (cons (car t) (filter f (cdr t)))
                (filter f (cdr t)))
            ())))
(define all?
    (lambda (f t)
        (if t
            (and
                (f (car t))
                (all? f (cdr t)))
            #t)))
(define any?
    (lambda (f t)
        (if t
            (or
                (f (car t))
                (any? f (cdr t)))
            ())))
(define mapcar
    (lambda (f t)
        (if t
            (cons (f (car t)) (mapcar f (cdr t)))
            ())))
(define map
    (lambda (f . args)
        (if (any? null? args)
            ()
            (let*
                (x (mapcar car args))
                (t (mapcar cdr args))
                (cons (f . x) (map f . t))))))
(define zip (lambda args (map list . args)))
(define seq
    (lambda (n m)
        (if (< n m)
            (cons n (seq (+ n 1) m))
            ())))
(define seqby
    (lambda (n m k)
        (if (< 0 (* k (- m n)))
            (cons n (seqby (+ n k) m k))
            ())))
(define range
    (lambda (n m . args)
        (if args
            (seqby n m (car args))
            (seq n m))))

(define curry (lambda (f x) (lambda args (f x . args))))
(define compose (lambda (f g) (lambda args (f (g . args)))))
(define Y (lambda (f) (lambda args ((f (Y f)) . args))))

(define reveal
    (lambda (f)
        (cond
            ((eq? (type f) 6) (cons 'lambda (cons (car (car f)) (cons (cdr (car f)) ()))))
            ((eq? (type f) 7) (cons 'macro (cons (car f) (cons (cdr f) ()))))
            (#t  f))))

(define defmacro (macro (f v x) (list 'define f (list 'macro v x))))
(defmacro defun (f v x) (list 'define f (list 'lambda v x)))
