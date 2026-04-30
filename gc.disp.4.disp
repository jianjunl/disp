;; gc.lisp - 后台线程定期触发 GC，用于调试
;;
;; 用法: (load "gc.lisp")
;;       (start-gc-thread 1.0)   ; 每 1 秒 GC 一次
;;       (stop-gc-thread)        ; 停止后台 GC 线程
;;
;; 加载脚本后会自动以默认间隔 (1 秒) 启动 GC 线程。

;; 辅助函数：向 stderr 打印并换行
(define fprintln (lambda (f s) (safe-fprintf f "%s\n" s)))

;; 全局变量，用于控制后台线程
(define *gc-thread* nil)          ; 当前运行的 GC 线程对象
(define *gc-running* #t)          ; 是否继续运行
(define *gc-mutex* (make-mutex))  ; 保护 *gc-running* 的互斥锁
(define *gc-auto-started* nil)    ; 是否已自动启动
(define *gc-interval* 1.0)        ; 当前 GC 间隔（秒）

;; 停止后台 GC 线程
(define (stop-gc-thread)
  (lock *gc-mutex*)
  (set! *gc-running* nil)
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
  ;; 确保 interval 是数值（整数或浮点数）
  (if (not (or (integer? interval) (decimal? interval)))
      (begin
        (fprintln stderr "start-gc-thread: interval must be a number")
        nil)
      (begin
        ;; 如果已有线程在运行，先停止它
        (if (not (null? *gc-thread*))
            (stop-gc-thread))
        
        ;; 保存间隔到全局变量（避免闭包捕获局部变量的问题）
        (set! *gc-interval* interval)
        
        ;; 重置运行标志
        (lock *gc-mutex*)
        (set! *gc-running* #t)
        (unlock *gc-mutex*)
        
        ;; 创建线程
        (set! *gc-thread*
              (make-thread
               (lambda ()
                 (safe-fprintf stderr ";; GC thread started, interval = %d %s\n" *gc-interval* "seconds")
                 (while *gc-running*
                   (thread-sleep *gc-interval*)
                   (lock *gc-mutex*)
                   (let ((running *gc-running*))
                     (unlock *gc-mutex*)
                     (if running
                         (begin
                           (gc)
                           (fprintln stderr ";; [GC] performed."))
                         #f)))
                 (fprintln stderr ";; GC thread exiting..."))))
        *gc-thread*)))

;; 自动启动（仅一次）
(if (not *gc-auto-started*)
    (begin
      (set! *gc-auto-started* #t)
      (start-gc-thread 1.0)
      (fprintln stderr ";; Auto GC thread started (every 1 sec). Use (stop-gc-thread) to stop.")))
