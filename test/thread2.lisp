;; 使用互斥锁保护共享变量
(define counter 0)
(define m (make-mutex))

(define inc (lambda ()
  (dotimes (i 1000)
    (lock m)
    (set! counter (+ counter 1))
    (unlock m))()))

(define t1 (make-thread inc))
(define t2 (make-thread inc))
(thread-join t1)
(thread-join t2)
(println "Counter:" counter)   ; 应为 2000
