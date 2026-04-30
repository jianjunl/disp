

(println "before let: (gc) begin")
(gc)
(println "before let: (gc) end")
(let ((value 'temp))
  (println "inside let: (gc) begin")
  (gc)
  (println "inside let: (gc) end"))
(println "after let: (gc) begin")
(gc)
(println "after let: (gc) end")
