(define (aa n) (if (= n 0) 0 (bb (- n 1))))
(define (bb n) (cc n))
(define (cc n) (aa n))
(aa 500000)
