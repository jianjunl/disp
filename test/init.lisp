
(define (caar x) (car (car x)))
(define (cadr x) (car (cdr x)))
(define (cdar x) (cdr (car x)))
(define (cddr x) (cdr (cdr x)))
(define (caaar x) (car (car (car x))))
(define (caadr x) (car (car (cdr x))))
(define (cadar x) (car (cdr (car x))))
(define (caddr x) (car (cdr (cdr x))))
(define (cdaar x) (cdr (car (car x))))
(define (cdadr x) (cdr (car (cdr x))))
(define (cddar x) (cdr (cdr (car x))))
(define (cdddr x) (cdr (cdr (cdr x))))
(define (caaaar x) (car (car (car (car x)))))
(define (caaadr x) (car (car (car (cdr x)))))
(define (caadar x) (car (car (cdr (car x)))))
(define (caaddr x) (car (car (cdr (cdr x)))))
(define (cadaar x) (car (cdr (car (car x)))))
(define (cadadr x) (car (cdr (car (cdr x)))))
(define (caddar x) (car (cdr (cdr (car x)))))
(define (cadddr x) (car (cdr (cdr (cdr x)))))
(define (cdaaar x) (cdr (car (car (car x)))))
(define (cdaadr x) (cdr (car (car (cdr x)))))
(define (cdadar x) (cdr (car (cdr (car x)))))
(define (cdaddr x) (cdr (car (cdr (cdr x)))))
(define (cddaar x) (cdr (cdr (car (car x)))))
(define (cddadr x) (cdr (cdr (car (cdr x)))))
(define (cdddar x) (cdr (cdr (cdr (car x)))))
(define (cddddr x) (cdr (cdr (cdr (cdr x)))))

## 比较运算符扩展
(define >= (lambda (a b) (not (< a b))))
(define <= (lambda (a b) (not (> a b))))
(define = (lambda (a b) (eq? a b)))   # 数值相等已有，但为了完整性
(define != (lambda (a b) (not (= a b))))

## 列表辅助函数
(define null? (lambda (x) (eq? x nil)))
(define cadr (lambda (x) (car (cdr x))))
(define caddr (lambda (x) (car (cdr (cdr x)))))
(define length (lambda (lst)
  (if (null? lst) 0 (+ 1 (length (cdr lst))))))
;(define append (lambda (a b)
;  (if (null? a) b (cons (car a) (append (cdr a) b)))))
(define map (lambda (f lst)
  (if (null? lst) nil (cons (f (car lst)) (map f (cdr lst))))))

## 常量
(define pi 3.14159)
(define e 2.71828)

## 一元函数
(define square (lambda (x) (* x x)))
(define cube (lambda (x) (* x x x)))
(define abs (lambda (n) (if (< n 0) (- n) n)))

## 多元函数
(define avg (lambda (a b) (/ (+ a b) 2)))
#(define hypotenuse (lambda (a b) (sqrt (+ (square a) (square b)))))  # sqrt 需要定义，见下文

## 符号比较（eq? 用于指针比较，对于符号很安全）
(define eq-symbol? (lambda (x y) (if (eq? x y) "yes" "no")))

## 嵌套 if
(define max3 (lambda (a b c)
  (if (> a b)
      (if (> a c) a c)
      (if (> b c) b c))))

#(define define-macro (macro (name args . body)
#  (list 'define (cons name args) (cons 'macro (cons args body)))))
#(define-macro begin (lambda (body) (cons 'lambda (cons '() body))))
#(define when (macro (test . body)
#              (list 'if test (cons 'begin body))))
#(define define-macro (macro (name args . body)
#  (list 'define (cons name args) (cons 'macro (cons args body)))))
#(define define-macro (macro (name args . body)
#  (list 'define (cons name args) (cons 'macro (cons args body)))))

(define define-macro (macro (name args . body)(list 'define name (cons 'macro (cons args body)))))
#示例 1：使用 quasiquote（`）和 unquote（,）
(define define-macro1 (macro (name args . body) `(define ,name (macro ,args ,@body))))
#示例 2：使用 list 但分开构造
(define define-macro2 (macro (name args . body) (list 'define name (list 'macro args (if (null? (cdr body)) (car body) (cons 'begin body))))))
#（这个例子处理了 body 多表达式的情况，将它们包装成 begin）
#示例 3：支持点参数（rest）的更通用版本
(define define-macro3 (macro (name args . body) (let ((macro-body (if (null? (cdr body)) (car body) (cons 'begin body)))) (list 'define name (list 'macro args macro-body)))))
#示例 4：直接使用 quote 避免 list（不推荐，仅展示）
(define define-macro4 (macro (name args . body) (cons 'define (cons name (cons (cons 'macro (cons args body)) '())))))
#示例 5：使用 eval 在宏展开时计算（危险，仅用于测试）
(define define-macro5 (macro (name args . body) (eval (list 'define name (cons 'macro (cons args body))))))

## 定义 when 宏（标准形式）
(define-macro when (test . body) (list 'if test (cons 'begin body)))

## 测试
#(when (> 3 2) (print "yes") (print "ok"))
## 输出: yes ok
## 返回值: ok

## 定义 unless 宏
(define-macro unless (test . body)(list 'if (list 'not test) (cons 'begin body)))

## 测试
#(unless (< 1 2) (print "should not print"))  # 不打印
#(unless (> 1 2) (print "should print"))       # 打印

;; for-each : 对列表中的每个元素应用函数，返回 nil
(define (for-each f lst)
  (if (null? lst)
      nil
      (begin
        (f (car lst))
        (for-each f (cdr lst)))))

## 支持 rest 参数（点列表）
#(define-macro with-output (stream . body)
#  (list 'let (list (list '*standard-output* stream)) (cons 'begin body)))

# 现有 sleep-ms 和 select 中的 after 已满足基本定时需求。如需更灵活的定时器（如周期性定时器），可封装一个 set-interval 宏，利用协程循环实现：
#|
(define-macro set-interval (ms . body)
  `(go (while true
         (sleep-ms ,ms)
         ,@body)))
|#
