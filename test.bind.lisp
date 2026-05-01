
(define define-macro (macro (name args . body)
  (list 'define name (list 'macro args (cons 'begin body)))))

;; test.bind.lisp - 测试局部绑定及 GC 下的旧值保护
;; 辅助函数
(define (assert-equal result expected msg)
  (if (not (equal result expected))
    (safe-fprintf stderr "FAILED [%s%s%s%s%s\n" msg "]: got " result " expected " expected)
    (safe-fprintf stderr "OK [%s%s%s%s%s\n" msg "]: got " result " expected " expected)))

;; 如果没有 gc 函数，定义一个空操作
;(unless (fboundp 'gc) (define gc () nil))

;; ------------------------------------------------------------------
;; 1. let 基本遮蔽与恢复
;; ------------------------------------------------------------------
(setq a 10)
(let ((a 20))
  (assert-equal a 20 "let: local binding"))
(assert-equal a 10 "let: restoration after let")

;; ------------------------------------------------------------------
;; 2. let* 顺序绑定
;; ------------------------------------------------------------------
(let* ((x 5)
       (y (+ x 1)))
  (assert-equal y 6 "let*: sequential binding"))

;; ------------------------------------------------------------------
;; 3. letrec 递归绑定
;; ------------------------------------------------------------------
(letrec ((even?
          (lambda (n)
            (if (= n 0) t (odd? (- n 1)))))
         (odd?
          (lambda (n)
            (if (= n 0) nil (even? (- n 1))))))
  (assert-equal (even? 4) t   "letrec: even? 4")
  (assert-equal (even? 3) nil "letrec: even? 3")
  (assert-equal (odd? 3) t    "letrec: odd? 3"))

;; ------------------------------------------------------------------
;; 4. letrec* 与 letrec 的区别
;; ------------------------------------------------------------------
(letrec* ((a 1)
          (b (+ a 1)))
  (assert-equal b 2 "letrec*: sequential initation"))

;; ------------------------------------------------------------------
;; 5. 命名 let (named let)
;; ------------------------------------------------------------------
(let loop ((i 3) (acc 0))
  (if (= i 0) acc
      (loop (- i 1) (+ acc i))))
(assert-equal (let loop ((i 3) (acc 0))
                (if (= i 0) acc
                    (loop (- i 1) (+ acc i))))
              6 "named let: sum 1..3")

;; ------------------------------------------------------------------
;; 6. 闭包与旧值恢复（动态作用域模拟）
;; ------------------------------------------------------------------
(setq cnt 100)
;; 在 let 内部定义函数，退出后检查符号恢复
(let ((cnt 999))
  (define get-cnt () cnt)
  (assert-equal (get-cnt) 999 "closure inside let captures local"))
;; 恢复后函数仍引用符号 cnt，现已被恢复为 100
(assert-equal cnt 100 "let: cnt restored")
(assert-equal (get-cnt) 100 "closure sees restored value (dynamic binding)")

;; ------------------------------------------------------------------
;; 7. 宏展开期间的绑定保护
;; ------------------------------------------------------------------
(setq *tracker* nil)

(define-macro test-macro (name)
  ;; 在展开期间修改全局变量，并分配一些对象
  (setq *tracker* (cons name *tracker*))
  ;; 产生一些垃圾
  (progn (gc) nil)   ; 如果 gc 可用则触发
  `',name)

(setq x 'original)
;; 宏展开会临时绑定 name 为 x（符号），展开后应该恢复 *tracker* 有 push 的效果
(test-macro x)
(assert-equal *tracker* '(x) "macro: side-effect preserved")
;; 展开时 name 绑定为符号 x，不会影响外部的 x 绑定
(assert-equal x 'original "macro: symbol binding restored")

;; ------------------------------------------------------------------
;; 8. 嵌套 let 与强制 GC
;; ------------------------------------------------------------------
(setq deep-value 'safe)


;; 创建大量垃圾，试图引发 GC
(let ((deep-value 'temp)
      (junk nil))
  (dotimes (i 1000)
    (setq junk (cons i junk)))
  (gc)  ;; 显式触发 GC（如果可用）
  (assert-equal deep-value 'temp "nested let: local value during GC"))
(assert-equal deep-value 'safe "nested let: value restored after GC")
(gc)
(assert-equal deep-value 'safe "nested let: value restored after GC")

;; ------------------------------------------------------------------
;; 9. let 内部错误（throw）时的恢复
;; ------------------------------------------------------------------
;#|
(setq flag 'before)
(catch 'escape
  (let ((flag 'inside))
    (throw 'escape nil)))
(assert-equal flag 'before "let: restoration after non-local exit")
;|#

;; ------------------------------------------------------------------
;; 10. 多个 let 的旧值保留（类似闭包可被捕获）
;; ------------------------------------------------------------------
(setq magic 0)
(let ((magic 1))
  (define inc-magic () (setq magic (+ magic 1))))
(inc-magic)
(assert-equal magic 1 "let: restore + dynamic inc")

(println "All binding tests completed.")
