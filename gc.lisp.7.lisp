
;; 全局 I/O 互斥锁，保护 stderr 写入
(define *io-mutex* (make-mutex))
;; 使用全局 I/O 互斥锁
(define *io-mutex* (make-mutex))

;; safe-fprintf0 可变参数包装（利用 apply 和点参数）
(define (safe-fprintf0 file fmt . args)
  (lock *io-mutex*)
  (let ((result (apply fprintf (cons file (cons fmt args)))))
    (unlock *io-mutex*)
    result))
(define (safe-fprintln f s)
  (lock *io-mutex*)
  (fprintf f "%s\n" s)
  (unlock *io-mutex*))

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
        (safe-fprintln stderr ";; Waiting for GC thread to finish...")
        (thread-join *gc-thread*)
        (set! *gc-thread* nil)
        (safe-fprintln stderr ";; GC thread stopped.")))
  #t)

(define (start-gc-thread interval)
  (if (not (number? interval))
      (begin (safe-fprintln stderr "start-gc-thread: interval must be a number") nil)
      (begin
        (if (not (null? *gc-thread*)) (stop-gc-thread))
        (set-car! *gc-state* interval)
        (lock *gc-mutex*)
        (set-cdr! *gc-state* #t)
        (unlock *gc-mutex*)
        (set! *gc-thread*
              (make-thread
               (lambda ()
                 (safe-fprintf0 stderr "GC thread started, interval = %d %s\n" (car *gc-state*) "seconds")
                 (let loop ()
                   (thread-sleep (car *gc-state*))
                   (lock *gc-mutex*)
                   (let ((running (cdr *gc-state*)))
                     (unlock *gc-mutex*)
                     (if running
                         (begin
                           (gc)
                           (safe-fprintln stderr ";; [GC] performed.")
                           (loop))
                         (safe-fprintln stderr ";; GC thread exiting...")))))))
        *gc-thread*)))

(if (not *gc-auto-started*)
    (begin
      (set! *gc-auto-started* #t)
      (start-gc-thread 1.0)
      (safe-fprintln stderr ";; Auto GC thread started (every 1 sec). Use (stop-gc-thread) to stop.")))
