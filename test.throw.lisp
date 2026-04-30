;; test.throw.lisp - 测试非局部控制转移
(define (test name result expected)
  (if (equal result expected)
      (safe-fprintf stdout "%s %s\n" name ": OK")
      (safe-fprintf stdout "%s %s %s %s %s\n" name ": FAILED - got " result " expected " expected)))


;; -------------------------------
;; 1. catch / throw
;; -------------------------------
(test "catch/throw simple"
      (catch 'foo
        (throw 'foo 42)
        (println "never reached"))
      42)

(test "catch without throw"
      (catch 'bar
        (println "no throw")
        99)
      99)

;; -------------------------------
;; 2. block / return-from
;; -------------------------------
(test "block/return-from"
      (block myblock
        (return-from myblock 10)
        20)
      10)

(test "block without return-from"
      (block myblock2
        30)
      30)

;; -------------------------------
;; 3. return (隐式 nil block)
;; -------------------------------
(test "return from nil block"
      (block nil
        (return 7)
        8)
      7)

(test "nested nil block - return from inner"
      (block nil
        (println "outer")
        (block nil
          (return 55))     ; 离开内层 nil 块
        66)                ; 这行不会执行
      55)                  ; 外层继续执行返回 55? 注意: 内层 return 离开内层块后，外层继续执行其尾表达式
;; 解释: (block nil (println "outer") (block nil (return 55)) 66) => 66? 
;; 实际上内层 return 退出了内层 block，然后外层的 body 继续执行 66，最终返回 66。
(test "return from inner nil block"
      (block nil
        (println "outer")
        (block nil
          (return 55))     ; 退出内层 block
        66)                ; 外层继续，返回 66
      66)

;; -------------------------------
;; 4. error 和 raise
;; -------------------------------
(test "error caught by catch 'error"
      (catch 'error
        (error "something bad")
        999)               ; 不应到达
      "something bad")     ; 值应为传入的字符串

(test "raise caught by catch 'error"
      (catch 'error
        (raise "something else"))
      "something else")

;; -------------------------------
;; 5. unwind-protect
;; -------------------------------
#|
(let ((flag nil))
  (unwind-protect
      (progn
        (println "protected form")
        10)
    (setq flag true)
    (println "cleanup"))
  (test "unwind-protect normal cleanup" flag true))

;; 在 throw 时执行清理

(let ((flag nil))
  (catch 'exit
    (unwind-protect
        (progn
          (println "throwing...")
          (throw 'exit 77)
        )
      (setq flag true)
      (println "cleanup on throw")))
  (test "unwind-protect cleanup on throw" flag true))

;; 嵌套 unwind-protect + throw
(let ((flag1 nil)
      (flag2 nil))
  (catch 'outer
    (unwind-protect
        (progn
          (unwind-protect
              (throw 'outer 33)
            (setq flag1 true))    ; 内层清理
          (println "never"))
      (setq flag2 true)))         ; 外层清理（throw 也会触发）
  (test "nested unwind-protect inner cleanup" flag1 true)
  (test "nested unwind-protect outer cleanup" flag2 true))
|#

(println "All tests done.")
