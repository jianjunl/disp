;; 加载基础库
(load "disp.data.so")
(load "disp.flow.so")
(load "disp.lisp")   ;; 现在提供了 define-macro

;; 使用 define-macro 定义 go 宏
(define-macro (go expr)
  (list 'go-func (list 'lambda '() expr)))

;; 定义 while 宏（支持 break）
(define-macro (while test . body)
  (list 'block nil
        (list 'let 'loop '()
              (list 'if test
                    (cons 'begin (append body (list '(loop))))
                    '(return)))))

;; 定义 break 宏
(define-macro (break) '(return))

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

;; 启动服务器
(go (server 8999))

;; 启动事件循环
(event-loop)
