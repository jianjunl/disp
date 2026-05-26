(define fact2 (n acc)
  (if (= n 0)
      acc
      (fact2 (- n 1) (* n acc))))

(fact2 80000 1)   ;; 不会再栈溢出
