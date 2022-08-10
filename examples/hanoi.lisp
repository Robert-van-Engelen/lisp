; towers of hanoi, moving n disks from src to dest, with spare available

(define hanoi
    (lambda (n src dest spare)
        (if (eq? n 1)
            (write "Move from " src " to " dest "\n")
            (hanoi (- n 1) src spare dest)
            (hanoi 1 src dest spare)
            (hanoi (- n 1) spare dest src))))


; example calls for testing

(write "moving stack of size 1 from left to right with middle as spare\n")
(hanoi 1 "left" "right" "middle")

(write "towers of hanoi moving stack of size 2 from left to right with middle as spare\n")
(hanoi 2 "left" "right" "middle")

(write "towers of hanoi moving stack of size 3 from left to right with middle as spare\n")
(hanoi 3 "left" "right" "middle")

(write "towers of hanoi moving stack of size 4 from left to right with middle as spare\n")
(hanoi 4 "left" "right" "middle")
