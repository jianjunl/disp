
(define define-macro (macro (name args . body)
  (list 'define name (list 'macro args (cons 'begin body)))))

(println "macro outside (gc) begin")
(gc) 
(println "macro outside (gc) end")
(setq *tracker* nil)
(define-macro test-macro (name)
  ;; 在展开期间修改全局变量，并分配一些对象
  (setq *tracker* (cons name *tracker*))
  ;; 产生一些垃圾
  (progn (gc) nil)   ; 如果 gc 可用则触发
  `',name)
