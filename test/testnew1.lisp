;; 测试基于原型的对象系统

;; 1. 定义根类型 Point，包含 x 和 y 字段
(new proto Point
     (define x 2)
     (define y 3))

;; 2. 验证字段访问
(display "Point.x = ") (display (type Point x)) (newline)
(display "Point.y = ") (display (type Point y)) (newline)
