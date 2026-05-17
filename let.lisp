
(define *gc-running* #t)
(define *gc-mutex* (make-mutex))
(define *gc-interval* 1.0)

(letrec loop2 ()
  (thread-sleep *gc-interval*)
  (lock *gc-mutex*)
  (let* ((running *gc-running*))
    (unlock *gc-mutex*)
    (if running
      (begin
        (gc)
        (fprintf stderr ";; [GC] performed.\n")
        (loop2)
      )
      (fprintf stderr ";; GC thread exiting...\n")
    )
  )
)
