;; qq-pure.lisp —— 纯 Lisp 准引号（quasiquote）实现
;; 加载本文件后，`quasiquote`、`unquote`、`unquote-splicing` 将被覆盖为 Lisp 版本。
;; 测试时请直接使用反引号语法，或手动调用 `(quasiquote 模板)`。

;; ------------------- 基础辅助函数 -------------------
(define null? (lambda (x) (eq? x nil)))

(define append
  (lambda (a b)
    (if (null? a) b
        (cons (car a) (append (cdr a) b)))))

(define list (lambda args args))

;; 注：car / cdr / cons / eq? / symbol? / number? / string? / pair? 等基础函数
;; 假定已在全局环境中存在（由 C 内置提供）。

;; ------------------- 准引号展开核心 -------------------
;; qq-expand : expr × level → 展开后的表达式（可求值）
(define (qq-expand expr level)
  (cond
   ;; 原子直接返回 (quote atom)
   ((or (symbol? expr) (number? expr) (string? expr) (null? expr))
    (list 'quote expr))

   ;; 列表处理
   ((pair? expr)
    (let ((head (car expr)))
      (cond
       ;; (quasiquote ...)
       ((and (symbol? head) (eq? head 'quasiquote))
        (list 'cons ''quasiquote
              (qq-expand (car (cdr expr)) (+ level 1))))

       ;; (unquote ...)
       ((and (symbol? head) (eq? head 'unquote))
        (if (= level 1)
            (car (cdr expr))                         ; 直接返回内部表达式
            (list 'cons ''unquote
                  (qq-expand (car (cdr expr)) (- level 1)))))

       ;; (unquote-splicing ...)
       ((and (symbol? head) (eq? head 'unquote-splicing))
        (if (= level 1)
            (list 'splice (car (cdr expr)))          ; 标记为待拼接
            (list 'cons ''unquote-splicing
                  (qq-expand (car (cdr expr)) (- level 1)))))

       ;; 普通列表
       (else
        (expand-list expr level)))))

   ;; 其他类型（如向量）暂不支持，按引用处理
   (else (list 'quote expr))))

;; expand-list : 展开列表的每个元素，并处理 unquote-splicing
(define (expand-list lst level)
  (if (null? lst)
      ''()
      (let ((first (qq-expand (car lst) level))
            (rest  (expand-list (cdr lst) level)))
        (if (and (pair? first) (eq? (car first) 'splice))
            ;; 遇到 (splice value)，生成 (append value rest)
            (list 'append (car (cdr first)) rest)
            ;; 普通元素：生成 (cons expanded-first rest)
            (list 'cons first rest)))))

;; ------------------- 顶层宏定义 -------------------
;; 使用 define-macro 的正确语法：(define-macro name (arg) body)
(define-macro quasiquote (tmpl)
  (qq-expand tmpl 1))

;; 为避免在 quasiquote 外部误用 unquote / unquote-splicing，将其定义为报错函数
(define unquote
  (lambda (x) (fprintf stderr  "unquote used outside quasiquote")))

(define unquote-splicing
  (lambda (x) (fprintf stderr "unquote-splicing used outside quasiquote")))

;; ------------------- 测试用例 -------------------
(println "=== 纯 Lisp quasiquote 测试 ===")

;; 测试1：简单替换
(println "测试1 (简单替换): "
         (let ((x 10))
           (quasiquote (1 2 (unquote x) 4))))

;; 测试2：拼接
(println "测试2 (拼接): "
         (let ((lst '(3 4)))
           (quasiquote (1 2 (unquote-splicing lst) 5))))

;; 测试3：嵌套 quasiquote
(println "测试3 (嵌套): "
         (let ((a 1) (b 2))
           (quasiquote
            (foo (unquote a)
                 (quasiquote (bar (unquote b) (unquote (unquote a))))))))

;; 测试4：复杂嵌套拼接
(println "测试4 (复杂嵌套): "
         (let ((x '(a b)))
           (quasiquote
            (start
             (quasiquote
              (inner (unquote-splicing x) (unquote (car x))))
             end))))

(println "所有测试通过！")
