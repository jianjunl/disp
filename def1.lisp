(define (f x) (g x))
(define (g y) (f y))   ; 相互尾递归，不会爆栈
(f 1000000)
