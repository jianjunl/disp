
(define qq-expand (x level)
  "递归展开 quasiquote 形式。Level 0 代表最外层，每进入一层 quasiquote 加 1。"
  (cond
    ;; 1. 非列表：原子（符号、数字等）直接返回被引用的形式
    ((atom x)
     `(quote ,x))

    ;; 2. 遇到 quasiquote：进入下一层嵌套
    ((eq? (car x) 'quasiquote)
     ;; 必须展开内层（level+1），并将结果包装在外层的构造中
     (qq-expand (list 'quote (qq-expand (cadr x) (+ level 1))) level))

    ;; 3. 处理 unquote：只有在 level 0 时才是真正的"开锁"
    ((eq? (car x) 'unquote)
     (if (= level 0)
         ;; 最外层：直接返回求值结果
         (cadr x)
         ;; 内层：消去一层引用，降低一级嵌套等级继续展开
         (qq-expand (list 'unquote (cadr x)) (- level 1))))

    ;; 4. 处理 unquote-splicing (类似 unquote 但用于拼接)
    ((eq? (car x) 'unquote-splicing)
     (if (= level 0)
         (error "unquote-splicing 出现在非法上下文 (level 0)"))
         (qq-expand (list 'unquote-splicing (cadr x)) (- level 1))))

    ;; 5. 处理普通列表或 cons：核心构造逻辑
    (t
     (qq-expand-list x level))))

(define qq-expand-list (x level)
  "处理列表结构，判断是否需要拼接。"
  (cond
    ;; 5.1 空列表
    ((null? x)
     ''nil)

    ;; 5.2 处理 (unquote-splicing ...) 在列表头部或中间的情况
    ((and (cons? (car x)) (eq? (caar x) 'unquote-splicing))
     (if (= level 0)
         ;; 最外层：拼接该列表与剩余部分
         `(append ,(cadar x) ,(qq-expand (cdr x) level))
         ;; 内层：降低嵌套等级
         (qq-expand `((unquote-splicing ,(cadar x)) . ,(cdr x)) (- level 1))))

    ;; 5.3 普通列表元素：递归展开头部和尾部
    (true
     (let ((a (qq-expand (car x) level))
           (b (qq-expand (cdr x) level)))
       ;; 优化：若尾部是 quote 形式，直接构造 cons，否则使用 list* 或 list
       (cond
         ((and (cons? b) (eq? (car b) 'quote))
          `(cons ,a ,b))
         (true
          `(list* ,a ,b)))))))
