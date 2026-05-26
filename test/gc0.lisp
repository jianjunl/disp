
(define (assert-equal result expected msg)
  (if (equal result expected)
    (safe-fprintf stderr "OK [%s%s%s%s%s\n" msg "]: got " result " expected " expected)
    (safe-fprintf stderr "FAILED [%s%s%s%s%s\n" msg "]: got " result " expected " expected)))

(define deep-value 'safe)

;; 创建大量垃圾，试图引发 GC
(let ((deep-value 'temp)
      (junk nil))
  (dotimes (i 1000) (define junk (cons i junk)))
  (gc)
  (assert-equal deep-value 'temp "nested let: local value during GC"))
(assert-equal deep-value 'safe "nested let: value restored after GC")
(gc)
(assert-equal deep-value 'safe "nested let: value restored after GC")
