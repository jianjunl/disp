;; 测试基于原型的对象系统

;; 1. 定义根类型 Point，包含 x 和 y 字段
(define Point (new proto Point
                  (define x 0)
                  (define y 0)))

;; 2. 验证字段访问
(display "Point.x = ") (display (type Point x)) (newline)
(display "Point.y = ") (display (type Point y)) (newline)

;; 3. 创建子类型 p，覆盖 x 和 y
(define p (new Point p
               (set! x 10)
               (set! y 20)))

(display "p.x = ") (display (type p x)) (newline)
(display "p.y = ") (display (type p y)) (newline)

;; 4. 嵌套类型访问（通过点语法）
;; 定义类型 A，包含 B，B 包含 C
(define A (new proto A
               (define B (new proto B
                             (define C (new proto C
                                           (define value 42)))))))
;; 访问 C.value
(display "A.B.C.value = ") (display (type A B C value)) (newline)

;; 5. 测试点符号转换
;; 使用点号表达式：A.B.C.value 应被解析为 (type A B C value)
(display "A.B.C.value via dot: ") (display A.B.C.value) (newline)

;; 以点开头的表达式：.THIS 应转换为 (type THIS)
(define THIS (type THIS))  ; 但 THIS 是关键字，所以此处仅演示
(display ".THIS = ") (display .THIS) (newline)

;; 测试连续点和末尾点
;; a..b..c. 应转换为 (type a b c)
(define a..b..c.  (type a b c))   ; 这里 just for syntax test
(display "a..b..c. = ") (display a..b..c.) (newline)

;; 6. 测试 new 的 body 多表达式
(define Multi (new proto Multi
                 (define foo 1)
                 (define bar (+ foo 2))
                 (display "Multi created\n")
                 bar))
(display "Multi.bar = ") (display (type Multi bar)) (newline)
