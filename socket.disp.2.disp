;; 定义 go 宏
(define-macro (go expr)
  `(go-func (lambda () ,expr)))

;; 定义 while 宏（支持 break）
(define-macro (while test . body)
  `(block nil
     (let loop ()
       (if ,test
           (begin ,@body (loop))
           (return)))))

(define break (return))

;; 处理客户端连接
(define (handle-client client-sock)
  (let ((client (car client-sock))
        (addr (cdr client-sock)))
    (println "Accepted from " addr)
    (while true
      (let ((data (recv-socket client 1024)))
        (if (eq? data nil)
            (begin
              (println "Client " addr " disconnected")
              (close-socket client)
              (break))
            (begin
              (println "Received: " data)
              (send-socket client data)))))))

;; 服务器主循环
(define (server port)
  (let ((listen-sock (make-socket)))
    (bind-socket listen-sock port)
    (listen-socket listen-sock 10)
    (println "Echo server listening on port " port)
    (while true
      (let ((client (accept-socket listen-sock)))
        (go (handle-client client))))))

;; 启动服务器（现在会正确创建协程）
(go (server 8999))

;; 启动事件循环
(event-loop)
