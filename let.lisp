
(define *gc-running* #t)
(define *gc-mutex* (make-mutex))
(define *gc-interval* 1.0)

(let loop2 ()
  (thread-sleep *gc-interval*)
  (lock *gc-mutex*)
  (let ((running *gc-running*))
    (unlock *gc-mutex*)
    (if running
      (begin
        (gc)
        (writeln ";; [GC] performed.")
        (loop2)
      )
      (writeln ";; GC thread exiting...")
    )
  )
)
