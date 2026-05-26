(define (a n) (if (= n 0) 0 (b (- n 1))))
(define (b n) (c n))
(define (c n) (a n))
(a 500000)
