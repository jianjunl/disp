
(define (assert-equal result expected msg)
  (if (not (equal result expected))
    (safe-fprintf stderr "FAILED [%s%s%s%s%s\n" msg "]: got " result " expected " expected)
    (safe-fprintf stderr "OK [%s%s%s%s%s\n" msg "]: got " result " expected " expected)))

;; ------------------------------------------------------------------
;; 9. let 内部错误（throw）时的恢复
;; ------------------------------------------------------------------
;#|
(setq flag 'before)
(catch 'escape
  (printf "catch 'escape flag=%s\n" flag)
  (let ((flag 'inside))
    (printf "let flag=%s\n" flag)
    (throw 'escape nil)
    (printf "thrown flag=%s\n" flag)
  )
)
(assert-equal flag 'before "let: restoration after non-local exit")
;|#
