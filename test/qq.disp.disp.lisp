;; qq-expand : 准引号展开器
;; 参数 expr : 待展开的表达式
;;      level: 当前嵌套层级（初始为 1）
;; 返回值: 展开后的表达式（可直接求值生成最终数据）

(define (qq-expand expr level)
  (cond
   ;; 原子：符号、数字、字符串、nil 直接返回（被引用）
   ((or (symbol? expr) (number? expr) (string? expr) (null? expr))
    (list 'quote expr))

   ;; 列表处理
   ((cons? expr)
    (let ((head (car expr)))
      (cond
       ;; (quasiquote ...)  增加层级
       ((and (symbol? head) (eq? head 'quasiquote))
        (list 'cons
              ''quasiquote
              (qq-expand (cadr expr) (+ level 1))))

       ;; (unquote ...)  减少层级
       ((and (symbol? head) (eq? head 'unquote))
        (if (= level 1)
            (cadr expr)                          ; 层级 1：直接返回内部表达式（将被求值）
            (list 'cons
                  ''unquote
                  (qq-expand (cadr expr) (- level 1)))))

       ;; (unquote-splicing ...)  处理拼接
       ((and (symbol? head) (eq? head 'unquote-splicing))
        (if (= level 1)
            (list 'splice (cadr expr))           ; 标记为需要拼接
            (list 'cons
                  ''unquote-splicing
                  (qq-expand (cadr expr) (- level 1)))))

       ;; 普通列表：递归展开每个元素
       (else
        (expand-list expr level)))))

   ;; 其他类型（向量等暂不支持）
   (else (list 'quote expr))))

;; 辅助函数：展开列表的每个元素，并处理可能的 splice 标记
(define (expand-list lst level)
  (if (null? lst)
      ''()
      (let ((first (qq-expand (car lst) level))
            (rest  (expand-list (cdr lst) level)))
        (if (and (cons? first) (eq? (car first) 'splice))
            ;; 遇到 splice 标记：生成 (append <spliced-list> <rest>)
            (list 'append (cadr first) rest)
            ;; 普通元素：生成 (cons <expanded-first> <rest>)
            (list 'cons first rest)))))

;; 顶层 quasiquote 宏：对模板调用 qq-expand 并求值
(define-macro quasiquote
  (lambda (tmpl)
    (qq-expand tmpl 1)))
