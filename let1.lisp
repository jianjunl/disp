
(define (assert-equal result expected msg)
  (if (equal result expected)
    (safe-fprintf stderr "OK [%s%s%s%s%s\n" msg "]: got " result " expected " expected)
    (safe-fprintf stderr "FAILED [%s%s%s%s%s\n" msg "]: got " result " expected " expected)))

(define a 10)
(let ((a 20))
  (assert-equal a 20 "let: local binding"))
(assert-equal a 10 "let: restoration after let")
(quit)
