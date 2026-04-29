;; 测试 1: 基础单层
(qq-expand '`(1 ,(+ 1 2) 3) 0)
;; 预期展开: (cons 1 (cons (+ 1 2) (cons 3 nil))) 或等价形式

;; 测试 2: 双重嵌套与反引号嵌套
(let ((x 'a))
  (eval (qq-expand '``(,,x) 0)))
;; 预期结果: (A)

;; 测试 3: 复杂拼接 (常见于宏定义宏)
(qq-expand '`(begin (define-macro behead (x) `(set! ,',head ,x))) 0)
;; 预期: 内层 `,',head` 展开为 `head` 而非 `,head`
