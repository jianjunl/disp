;; 创建线程执行简单计算
(define t (make-thread (lambda () (+ 1 2))))
(println "Result:" (thread-join t))   ; 应输出 3

;; 使用互斥锁保护共享变量
(define counter 0)
(define m (make-mutex))

(define inc (lambda ()
  (dotimes (i 1000)
    (lock m)
    (set! counter (+ counter 1))
    (unlock m))))

(define t1 (make-thread inc))
(define t2 (make-thread inc))
(thread-join t1)
(thread-join t2)
(println "Counter:" counter)   ; 应为 2000

;; 条件变量示例：生产者-消费者
(define queue '())
(define q-mutex (make-mutex))
(define q-cond (make-condition))

(define producer (lambda ()
  (dotimes (i 5)
    (lock q-mutex)
    (set! queue (cons i queue))
    (println "Produced:" i)
    (condition-signal q-cond)
    (unlock q-mutex)
    (thread-sleep 0.1))))

(define consumer (lambda ()
  (dotimes (i 5)
    (lock q-mutex)
    (while (null? queue)
      (condition-wait q-cond q-mutex))
    (let ((item (car queue)))
      (set! queue (cdr queue))
      (println "Consumed:" item))
    (unlock q-mutex))))

(define pt (make-thread producer))
(define ct (make-thread consumer))
(thread-join pt)
(thread-join ct)
