;;; qq-expand.sps — R6RS Scheme 实现的 quasiquote 展开器

;; 辅助：判断是否为指定符号
(define (tagged? x tag)
  (and (pair? x) (eq? (car x) tag)))

;; 核心展开函数
(define (qq-expand x level)
  (cond
    ;; 1. 原子：符号、数字、布尔、字符、空表等
    ((not (pair? x))
     `',x)

    ;; 2. (quasiquote <expr>)
    ((tagged? x 'quasiquote)
     (qq-expand `(quote ,(qq-expand (cadr x) (+ level 1))) level))

    ;; 3. (unquote <expr>)
    ((tagged? x 'unquote)
     (if (= level 0)
         (cadr x)                         ; 最外层：直接返回内部表达式
         (qq-expand `(unquote ,(cadr x)) (- level 1))))

    ;; 4. (unquote-splicing <expr>)
    ((tagged? x 'unquote-splicing)
     (if (= level 0)
         (error 'qq-expand "unquote-splicing 出现在非法上下文")
         (qq-expand `(unquote-splicing ,(cadr x)) (- level 1))))

    ;; 5. 普通对 / 列表
    (else
     (qq-expand-list x level))))

;; 处理列表的构造逻辑
(define (qq-expand-list x level)
  (cond
    ;; 空表
    ((null? x) ''())

    ;; 头部是 (unquote-splicing ...)
    ((and (pair? (car x))
          (tagged? (car x) 'unquote-splicing))
     (if (= level 0)
         ;; 最外层：拼接该列表与剩余部分
         `(append ,(cadar x) ,(qq-expand (cdr x) level))
         ;; 内层：降低嵌套等级后继续
         (qq-expand `((unquote-splicing ,(cadar x)) . ,(cdr x)) (- level 1))))

    ;; 普通情况
    (else
     (let ((head-exp (qq-expand (car x) level))
           (tail-exp (qq-expand (cdr x) level)))
       ;; 若尾部是一个 quoted 常量，直接用 cons 即可
       (if (and (pair? tail-exp) (eq? (car tail-exp) 'quote))
           `(cons ,head-exp ,tail-exp)
           `(cons ,head-exp ,tail-exp))))))   ; 统一使用 cons，避免依赖 list*

;; 顶层包装：从 level 0 开始展开
(define (qq-expand-top expr)
  (qq-expand expr 0))

#|
;;; 测试用例

;; 测试 1：基本单层
(pretty-print (qq-expand-top '`(1 ,(+ 1 2) 3)))
;; 预期输出（简化）: (cons '1 (cons (+ 1 2) (cons '3 '())))

;; 测试 2：双重嵌套
(pretty-print (qq-expand-top '``(,,x)))
;; 输出大致为: (cons 'quasiquote (cons (cons 'unquote (cons (cons 'unquote (cons 'x '())) '())) '()))

;; 测试 3：拼接用法
(pretty-print (qq-expand-top '`(1 ,@(list 2 3) 4)))
;; 预期输出: (append (list 2 3) (cons '4 '()))

;; 测试 4：内层 unquote-splicing 的处理
(pretty-print (qq-expand-top '`(a `(b ,@c))))
;; 此处内层 level=1，会正确保留结构
|#
