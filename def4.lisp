(define (f x) (g x))
(define (g y) (f y))   ; 相互尾递归，不会爆栈
;; 多线程同时运行尾递归
(define t1 (make-thread (lambda () (f 1000000))))
(define t2 (make-thread (lambda () (g 1000000))))
(thread-join t1)
(thread-join t2)
