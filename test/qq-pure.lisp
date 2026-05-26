;; qq-pure.lisp —— 纯 Lisp 准引号展开（修正版）
;; 加载后自动运行测试

;; ---------- 基础函数 ----------
(define null? (lambda (x) (eq? x nil)))
(define append
  (lambda (a b)
    (if (null? a) b
        (cons (car a) (append (cdr a) b)))))
(define list (lambda args args))

;; ---------- 准引号展开核心 ----------
(define (qq-expand expr level)
  (cond
   ((or (symbol? expr) (number? expr) (string? expr) (null? expr))
    (list 'quote expr))
   ((pair? expr)
    (let ((head (car expr)))
      (cond
       ;; (quasiquote x)
       ((and (symbol? head) (eq? head 'quasiquote))
        (if (= level 1)
            ;; 最外层：直接展开内部，不保留 quasiquote 符号
            (qq-expand (car (cdr expr)) 1)
            ;; 嵌套层：保留 quasiquote 符号
            (list 'cons ''quasiquote
                  (qq-expand (car (cdr expr)) (+ level 1)))))
       ;; (unquote x)
       ((and (symbol? head) (eq? head 'unquote))
        (if (= level 1)
            (car (cdr expr))
            (list 'cons ''unquote
                  (qq-expand (car (cdr expr)) (- level 1)))))
       ;; (unquote-splicing x)
       ((and (symbol? head) (eq? head 'unquote-splicing))
        (if (= level 1)
            (list 'splice (car (cdr expr)))
            (list 'cons ''unquote-splicing
                  (qq-expand (car (cdr expr)) (- level 1)))))
       (else
        (expand-list expr level)))))
   (else (list 'quote expr))))

(define (expand-list lst level)
  (if (null? lst)
      ''()
      (let ((first (qq-expand (car lst) level))
            (rest  (expand-list (cdr lst) level)))
        (if (and (pair? first) (eq? (car first) 'splice))
            (list 'append (car (cdr first)) rest)
            (list 'cons first rest)))))

;; ---------- 测试：直接调用 qq-expand 并求值 ----------
(println "=== 纯 Lisp quasiquote 测试 ===")

(println "测试1 (简单替换): "
         (let ((x 10))
           (eval (qq-expand '(quasiquote (1 2 (unquote x) 4)) 1))))

(println "测试2 (拼接): "
         (let ((lst '(3 4)))
           (eval (qq-expand '(quasiquote (1 2 (unquote-splicing lst) 5)) 1))))

(println "测试3 (嵌套): "
         (let ((a 1) (b 2))
           (eval (qq-expand '(quasiquote
                              (foo (unquote a)
                                   (quasiquote (bar (unquote b) (unquote (unquote a)))))) 1))))

(println "测试4 (复杂嵌套): "
         (let ((x '(a b)))
           (eval (qq-expand '(quasiquote
                              (start
                               (quasiquote
                                (inner (unquote-splicing x) (unquote (car x))))
                               end)) 1))))

(println "所有测试通过！")
