;; gc.lisp - 后台线程定期触发 GC，用于调试
;;
;; 用法: (load "gc.lisp")
;;       (start-gc-thread 1.0)   ; 每 1 秒 GC 一次
;;       (stop-gc-thread)        ; 停止后台 GC 线程

(define null? (lambda (x) (eq? x nil)))
(define fprintln (lambda (f s) (fprintf f "%s\n" s)))

;; 将控制状态存储在一个可变 cons 单元中，避免动态作用域问题
(define *gc-state* (cons 1.0 #t))   ; car = 间隔(秒), cdr = 运行标志(#t/nil)
(define *gc-thread* nil)
(define *gc-mutex* (make-mutex))
(define *gc-auto-started* nil)

;; 停止后台 GC 线程
(define (stop-gc-thread)
  (lock *gc-mutex*)
  (set-cdr! *gc-state* nil)          ; 设置运行标志为 nil
  (unlock *gc-mutex*)
  (if (not (null? *gc-thread*))
      (begin
        (fprintln stderr ";; Waiting for GC thread to finish...")
        (thread-join *gc-thread*)
        (set! *gc-thread* nil)
        (fprintln stderr ";; GC thread stopped.")))
  #t)

;; 启动后台 GC 线程，interval 单位为秒
(define (start-gc-thread interval)
  (if (not (number? interval))
      (begin (fprintln stderr "start-gc-thread: interval must be a number") nil)
      (begin
        (if (not (null? *gc-thread*)) (stop-gc-thread))
        (set-car! *gc-state* interval)   ; 保存间隔
        (lock *gc-mutex*)
        (set-cdr! *gc-state* #t)         ; 设置运行标志为真
        (unlock *gc-mutex*)
        (set! *gc-thread*
              (make-thread
               (lambda ()
                 (fprintln stderr (string ";; GC thread started, interval = "
                                          (number->string (car *gc-state*))
                                          " seconds"))
                 (let loop ()
                   (thread-sleep (car *gc-state*))   ; 直接从 cons 中取间隔
                   (lock *gc-mutex*)
                   (let ((running (cdr *gc-state*)))
                     (unlock *gc-mutex*)
                     (if running
                         (begin
                           (gc)
                           (fprintln stderr ";; [GC] performed.")
                           (loop))
                         (fprintln stderr ";; GC thread exiting...")))))))
        *gc-thread*)))

;; 自动启动（仅一次）
(if (not *gc-auto-started*)
    (begin
      (set! *gc-auto-started* #t)
      (start-gc-thread 1.0)
      (fprintln stderr ";; Auto GC thread started (every 1 sec). Use (stop-gc-thread) to stop.")))
