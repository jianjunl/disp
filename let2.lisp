
(define *gc-running* #t)
(define *gc-mutex* (make-mutex))
(define *gc-interval* 1.0)

(let loop2 ()
  (thread-sleep *gc-interval*)
  (lock *gc-mutex*)
    (unlock *gc-mutex*)
    (if #t
      (begin
        (gc)
        (fprintf stderr ";; [GC] performed.\n")
        (loop2)
      )
      (fprintf stderr ";; GC thread exiting...\n")
    )
)
