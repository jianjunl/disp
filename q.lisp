
;; 简单 unquote
(define x 5)
`(1 ,x 2)          ; 应返回 (1 5 2)

;; 嵌套 quasiquote
`(a `(b ,(+ 1 2) ,,x))   ; 应返回 (a (quasiquote (b (unquote (+ 1 2)) 5)))

;; 多层 unquote（需确保 x 在正确层级求值）
(let ((x 10)) `(outer ,x `(inner ,,x)))  ; 外层 x=10，内层 x 应在外层环境中求值（10）

`(1 ,(+ 2 3) 4)          ; => (1 5 4)
`(1 ,@(list 2 3) 4)      ; => (1 2 3 4)
(let ((x 10)) `(,x ,x))  ; => (10 10)
`(a `(b ,(+ 1 2) ,,x))   ; => (a (quasiquote (b (unquote (+ 1 2)) 5))) （假设 x=5）
