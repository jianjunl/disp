;; 基础类型
(equal 1 1)           ; => t
(equal 1 2)           ; => nil
(equal "abc" "abc")   ; => t
(equal "abc" "ABC")   ; => nil

;; 符号
(equal 'a 'a)         ; => t
(equal 'a 'b)         ; => nil

;; cons / 列表
(equal '(1 2 3) '(1 2 3))     ; => t
(equal '(1 2 3) '(1 2 4))     ; => nil

;; eq? 的比较（身份，不等于 equal）
(setq x '(a b))
(setq y '(a b))
(eq? x y)    ; => nil（两个不同 cons）
(equal x y)  ; => t（内容相同）
(eq? x x)    ; => t（同一对象）
