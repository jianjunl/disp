
;; 处理单个客户端（递归循环替代 while）
(define (handle-client client-sock)
  (let ((client (car client-sock))
        (addr (cdr client-sock)))
    (println "Accepted from " addr)
    (let loop ()
      (let ((data (recv-socket client 1024)))
        (if (eq? data nil)
            (begin
              (println "Client " addr " disconnected")
              (close-socket client))
            (begin
              (println "Received: " data)
              (if (not? data) (return))
              (send-socket client data)
              (loop)))))))

;; 服务器接受循环（递归）
(define (accept-loop listen-sock)
  (let ((client (accept-socket listen-sock)))
    (go-func (lambda () (handle-client client)))   ; 直接用 go-func + lambda
    (accept-loop listen-sock)))

;; 启动服务器
(define (server port)
  (let ((listen-sock (make-socket)))
    (bind-socket listen-sock port)
    (listen-socket listen-sock 10)
    (println "Echo server listening on port " port)
    (accept-loop listen-sock)))

;; 在主协程中启动服务器（使用 go-func 显式创建协程）
(go-func (lambda () (server 8999)))

;; 进入事件循环
;(event-loop)
;; 启动事件循环
(event-loop)

;; 防止 REPL 尝试打印返回值（event-loop 通常不返回）
(while true (sleep-ms 1000))
