
;; 状态存储：car=间隔(秒)，cdr=运行标志
(define *gc-state* (cons 1.0 #t))
(define *gc-thread* nil)
(define *gc-mutex* (make-mutex))
(define *gc-auto-started* nil)

(define (stop-gc-thread)
  (lock *gc-mutex*)
  (set-cdr! *gc-state* nil)
  (unlock *gc-mutex*)
  (if (not (null? *gc-thread*))
      (begin
        (safe-fprintf stderr ";; Waiting for GC thread to finish...\n")
        (thread-join *gc-thread*)
        (set! *gc-thread* nil)
        (safe-fprintf stderr ";; GC thread stopped.\n")))
  #t)

(define (start-gc-thread interval)
  (if (not (number? interval))
      (begin (safe-fprintf stderr "start-gc-thread: interval must be a number\n") nil)
      (begin
        (if (not (null? *gc-thread*)) (stop-gc-thread))
        (set-car! *gc-state* interval)
        (lock *gc-mutex*)
        (set-cdr! *gc-state* #t)
        (unlock *gc-mutex*)
        (set! *gc-thread*
              (make-thread
               (lambda ()
                 (safe-fprintf stderr "GC thread started, interval = %d %s\n" (car *gc-state*) "seconds")
                 (let loop ()
                   (thread-sleep (car *gc-state*))
                   (lock *gc-mutex*)
                   (let ((running (cdr *gc-state*)))
                     (unlock *gc-mutex*)
                     (if running
                         (begin
                           (gc)
                           (safe-fprintf stderr ";; [GC] performed.\n")
                           (loop))
                         (safe-fprintf stderr ";; GC thread exiting...\n")))))))
        *gc-thread*)))

(if (not *gc-auto-started*)
    (begin
      (set! *gc-auto-started* #t)
      (start-gc-thread 1.0)
      (safe-fprintf stderr ";; Auto GC thread started (every 1 sec). Use (stop-gc-thread) to stop.\n")))
