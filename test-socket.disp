
;; 定义处理单个客户端连接的协程
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
              (send-socket client data)))))))  ; echo back

;; 主服务器协程
(define (server port)
  (let ((listen-sock (make-socket)))
    (bind-socket listen-sock port)
    (listen-socket listen-sock 10)
    (println "Echo server listening on port " port)
    (while true
      (let ((client (accept-socket listen-sock)))
        (go (handle-client client))))))

;; 启动服务器
(go (server 8999))

;; 启动事件循环
(event-loop)
