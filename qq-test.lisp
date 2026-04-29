;; ======================== 基础工具 ========================
(define null? (lambda (x) (eq? x nil)))

;; 纯 Lisp 实现的 append
(define (append a b)
  (if (null? a)
      b
      (cons (car a) (append (cdr a) b))))

;; ======================== 准引号展开器 ========================
;; 注意：unquote 和 unquote-splicing 仅在宏展开期作为符号标记使用，
;; 它们不是可调用的函数，因此不需要定义它们的函数体。

(define (qq-expand expr level)
  (cond
   ;; 原子直接返回引用的形式
   ((or (symbol? expr) (number? expr) (string? expr) (null? expr))
    (list 'quote expr))

   ((pair? expr)
    (let ((head (car expr)))
      (cond
       ;; quasiquote
       ((and (pair? head) (null? (cdr head)) (eq? (car head) 'quasiquote)) ; 简化判断，实际 head 就是符号 quasiquote
        (if (and (symbol? head) (eq? head 'quasiquote))
            (list 'cons ''quasiquote (qq-expand (cadr expr) (+ level 1)))
            (expand-list expr level)))

       ;; 重新整理判断：head 必须是符号
       ((symbol? head)
        (cond
         ((eq? head 'quasiquote)
          (list 'cons ''quasiquote (qq-expand (cadr expr) (+ level 1))))

         ((eq? head 'unquote)
          (if (= level 1)
              (cadr expr)
              (list 'cons ''unquote (qq-expand (cadr expr) (- level 1)))))

         ((eq? head 'unquote-splicing)
          (if (= level 1)
              (list 'splice (cadr expr))
              (list 'cons ''unquote-splicing (qq-expand (cadr expr) (- level 1)))))

         (else (expand-list expr level))))
       (else (expand-list expr level)))))

   (else (list 'quote expr))))

(define (expand-list lst level)
  (if (null? lst)
      ''()
      (let ((first (qq-expand (car lst) level))
            (rest  (expand-list (cdr lst) level)))
        (if (and (pair? first) (eq? (car first) 'splice))
            (list 'append (cadr first) rest)
            (list 'cons first rest)))))

;; 顶层宏：quasiquote
(define-macro2 quasiquote
  (lambda (tmpl)
    (qq-expand tmpl 1)))

;; 为了使用反引号阅读器，解析器会将 `x 转换为 (quasiquote x)，
;; ,x 转换为 (unquote x)，,@x 转换为 (unquote-splicing x)。
;; 因此用户可以直接写 `(a ,b ,@c)

;; ======================== 测试用例 ========================
(define (test-quasiquote)
  (println "=== 测试 quasiquote 纯 Lisp 实现 ===")

  ;; 测试1：简单替换
  (let ((x 10))
    (let ((result (quasiquote (1 2 (unquote x) 4))))
      (print "测试1 (简单替换): ")
      (println result)
      (assert (equal? result '(1 2 10 4)))))

  ;; 测试2：拼接
  (let ((lst '(3 4)))
    (let ((result (quasiquote (1 2 (unquote-splicing lst) 5))))
      (print "测试2 (拼接): ")
      (println result)
      (assert (equal? result '(1 2 3 4 5)))))

  ;; 测试3：嵌套 quasiquote
  (let ((a 1) (b 2))
    (let ((result (quasiquote
                   (foo (unquote a)
                        (quasiquote (bar (unquote b) (unquote (unquote a))))))))
      (print "测试3 (嵌套): ")
      (println result)
      (assert (equal? result '(foo 1 (quasiquote (bar (unquote b) (unquote 1))))))))

  ;; 测试4：复杂表达式
  (let ((x '(a b)))
    (let ((result (quasiquote
                   (start
                    (quasiquote (inner (unquote-splicing x) (unquote (car x))))
                    end))))
      (print "测试4 (复杂嵌套): ")
      (println result)
      (assert (equal? result '(start (quasiquote (inner (unquote-splicing x) (unquote a))) end)))))

  (println "所有测试通过！"))

;; 辅助断言
(define (assert condition)
  (if condition
      'ok
      (error "Assertion failed")))

;; 简单相等比较（用于测试）
(define (equal? a b)
  (cond
   ((and (null? a) (null? b)) #t)
   ((or (null? a) (null? b)) #f)
   ((and (pair? a) (pair? b))
    (and (equal? (car a) (car b))
         (equal? (cdr a) (cdr b))))
   ((and (symbol? a) (symbol? b)) (eq? a b))
   ((and (number? a) (number? b)) (= a b))
   ((and (string? a) (string? b)) (string=? a b))
   (else #f)))

;; 运行测试
(test-quasiquote)
