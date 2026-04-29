
(load "disp.lisp")

;; 定义 go 宏
(define-macro go (expr)
  `(go-func (lambda () ,expr)))

#(define go (macro (expr) (list 'go-func (list 'lambda '() expr))))

;; 定义 break 和 while 宏（如果没有）
;; while 已有，但 break 可能需要实现为 catch/throw 或使用 return-from
;; 简单起见，用 return-from 实现 break：
(define-macro whil (test . body)
  `(block nil
     (let loop ()
       (if ,test
           (begin ,@body (loop))
           (return)))))

;; 处理客户端
(define (handle-client client-sock)
  (let ((client (car client-sock))
        (addr (cdr client-sock)))
    (println "Accepted from " addr)
    (whil true
      (let ((data (recv-socket client 1024)))
        (if (eq? data nil)
            (begin
              (println "Client " addr " disconnected")
              (close-socket client)
              (return))   ; 退出 while
            (begin
              (println "Received: " data)
              (send-socket client data)))))))

;; 服务器
(define (server port)
  (let ((listen-sock (make-socket)))
    (bind-socket listen-sock port)
    (listen-socket listen-sock 10)
    (println "Echo server listening on port " port)
    (whil true
      (let ((client (accept-socket listen-sock)))
        (go (handle-client client))))))

;; 启动
(go (server 8999))

;; 事件循环
(event-loop)
