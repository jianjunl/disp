(define (fact n acc)
  (if (= n 0)
      acc
      (fact (- n 1) (* n acc))))

(fact 80000 1)   ;; 不会再栈溢出
