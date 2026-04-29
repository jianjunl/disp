
#define _POSIX_C_SOURCE 200809L
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#ifndef DEBUG
//#define DEBUG
#endif
#include "../disp.h"

/* 设置非阻塞模式 */
static int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

/* 创建 TCP socket */
static disp_val* make_socket_syscall(disp_val **args, int count) {
    (void)args; (void)count;
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        perror("socket");
        return NIL;
    }
    set_nonblocking(fd);
    //return disp_make_socket(fd);
    disp_val *sock = disp_make_socket(fd);
    DBG("Created socket object %p, fd=%d", (void*)sock, fd);
    return sock;
}

/* 绑定端口 */
static disp_val* bind_socket_syscall(disp_val **args, int count) {
    if (count != 2 || T(args[0]) != DISP_SOCKET)
        ERET(NIL, "bind-socket: expects (socket port)");
    int fd = disp_get_socket_fd(args[0]);
    if (fd == -1) return NIL;

    long port = 0;
    if (T(args[1]) == DISP_INT) port = disp_get_int(args[1]);
    else if (T(args[1]) == DISP_LONG) port = disp_get_long(args[1]);
    else ERET(NIL, "bind-socket: port must be integer");

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((uint16_t)port);

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("bind");
        return NIL;
    }
    return TRUE;
}

/* 监听 */
static disp_val* listen_socket_syscall(disp_val **args, int count) {
    if (count != 2 || T(args[0]) != DISP_SOCKET)
        ERET(NIL, "listen-socket: expects (socket backlog)");
    int fd = disp_get_socket_fd(args[0]);
    if (fd == -1) return NIL;

    int backlog = 5;
    if (T(args[1]) == DISP_INT) backlog = disp_get_int(args[1]);
    else if (T(args[1]) == DISP_LONG) backlog = (int)disp_get_long(args[1]);

    if (listen(fd, backlog) == -1) {
        perror("listen");
        return NIL;
    }
    return TRUE;
}

/* 非阻塞 accept */
static disp_val* accept_socket_syscall(disp_val **args, int count) {
    if (count != 1 || T(args[0]) != DISP_SOCKET)
        ERET(NIL, "accept-socket: expects a socket");
    int listen_fd = disp_get_socket_fd(args[0]);
    if (listen_fd == -1) return NIL;

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int client_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &addr_len);
        if (client_fd != -1) {
            set_nonblocking(client_fd);
            // 构造返回点对： (client-socket . "ip:port")
            char addr_str[INET_ADDRSTRLEN + 10];
            snprintf(addr_str, sizeof(addr_str), "%s:%d",
                     inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            disp_val *sock_obj = disp_make_socket(client_fd);
            disp_val *addr_obj = disp_make_string(addr_str);
            return disp_make_cons(sock_obj, addr_obj);
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            disp_val *current = disp_get_current_coro();
            event_loop_add_fd(listen_fd, current, EPOLLIN);
            scheduler_suspend();
            // 唤醒后重试
            continue;
        }
        perror("accept");
        return NIL;
    }
}

/* 非阻塞 connect */
static disp_val* connect_socket_syscall(disp_val **args, int count) {
    if (count != 3 || T(args[0]) != DISP_SOCKET || T(args[1]) != DISP_STRING)
        ERET(NIL, "connect-socket: expects (socket host port)");
    int fd = disp_get_socket_fd(args[0]);
    if (fd == -1) return NIL;

    const char *host = disp_get_str(args[1]);
    long port = 0;
    if (T(args[2]) == DISP_INT) port = disp_get_int(args[2]);
    else if (T(args[2]) == DISP_LONG) port = disp_get_long(args[2]);
    else ERET(NIL, "connect-socket: port must be integer");

    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%ld", port);

    struct addrinfo hints;
    struct addrinfo *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;       // IPv4
    hints.ai_socktype = SOCK_STREAM; // TCP

    int gai_ret = getaddrinfo(host, port_str, &hints, &res);
    if (gai_ret != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(gai_ret));
        return NIL;
    }

    int ret = connect(fd, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);

    if (ret == -1 && errno != EINPROGRESS) {
        perror("connect");
        return NIL;
    }
    if (ret == 0) return TRUE;  // 立即连接成功（例如本地连接）

    // 等待可写事件（表示连接完成或出错）
    disp_val *current = disp_get_current_coro();
    event_loop_add_fd(fd, current, EPOLLOUT);
    scheduler_suspend();

    // 检查连接是否成功
    int error = 0;
    socklen_t len = sizeof(error);
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) == -1 || error != 0) {
        errno = error;
        perror("connect");
        return NIL;
    }
    return TRUE;
}

/* 非阻塞 recv */
static disp_val* recv_socket_syscall(disp_val **args, int count) {
    if (count != 2 || T(args[0]) != DISP_SOCKET)
        ERET(NIL, "recv-socket: expects (socket size)");
    int fd = disp_get_socket_fd(args[0]);
    if (fd == -1) return NIL;

    int max_size = 4096;
    if (T(args[1]) == DISP_INT) max_size = disp_get_int(args[1]);
    else if (T(args[1]) == DISP_LONG) max_size = (int)disp_get_long(args[1]);

    char *buf = gc_malloc(max_size + 1);
    if (!buf) return NIL;

    while (1) {
        ssize_t n = recv(fd, buf, max_size, 0);
        if (n > 0) {
            buf[n] = '\0';
            disp_val *res = disp_make_string(buf);
            gc_free(buf);
            return res;
        }
        if (n == 0) {
            gc_free(buf);
            return NIL;  // 连接关闭
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            disp_val *current = disp_get_current_coro();
            event_loop_add_fd(fd, current, EPOLLIN);
            scheduler_suspend();
            continue;
        }
        perror("recv");
        gc_free(buf);
        return NIL;
    }
}

/* 非阻塞 send（完整发送） */
static disp_val* send_socket_syscall(disp_val **args, int count) {
    if (count != 2 || T(args[0]) != DISP_SOCKET || T(args[1]) != DISP_STRING)
        ERET(NIL, "send-socket: expects (socket data)");
    int fd = disp_get_socket_fd(args[0]);
    if (fd == -1) return NIL;

    const char *data = disp_get_str(args[1]);
    size_t len = strlen(data);
    size_t sent = 0;

    while (sent < len) {
        ssize_t n = send(fd, data + sent, len - sent, 0);
        if (n == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                disp_val *current = disp_get_current_coro();
                event_loop_add_fd(fd, current, EPOLLOUT);
                scheduler_suspend();
                continue;
            }
            perror("send");
            return NIL;
        }
        sent += n;
    }
    return disp_make_long((long)sent);
}

/* 关闭 socket */
static disp_val* close_socket_syscall(disp_val **args, int count) {
    if (count != 1 || T(args[0]) != DISP_SOCKET)
        ERET(NIL, "close-socket: expects a socket");
    int fd = disp_get_socket_fd(args[0]);
    if (fd != -1) {
        close(fd);
        // 将对象中的 fd 标记为 -1（通过重新分配 data ？简单起见可忽略）
    }
    return TRUE;
}

/* 模块初始化 */
void disp_init_module(void) {
    DEF("make-socket",   MKF(make_socket_syscall,   "<make-socket>"),   1);
    DEF("bind-socket",   MKF(bind_socket_syscall,   "<bind-socket>"),   1);
    DEF("listen-socket", MKF(listen_socket_syscall, "<listen-socket>"), 1);
    DEF("accept-socket", MKF(accept_socket_syscall, "<accept-socket>"), 1);
    DEF("connect-socket",MKF(connect_socket_syscall,"<connect-socket>"),1);
    DEF("recv-socket",   MKF(recv_socket_syscall,   "<recv-socket>"),   1);
    DEF("send-socket",   MKF(send_socket_syscall,   "<send-socket>"),   1);
    DEF("close-socket",  MKF(close_socket_syscall,  "<close-socket>"),  1);
}
