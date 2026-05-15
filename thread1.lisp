;; 创建线程执行简单计算
(define t (make-thread (lambda () (+ 1 2))))
(println "Result:" (thread-join t))   ; 应输出 3
