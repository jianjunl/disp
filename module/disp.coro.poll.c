
#ifndef DISP_CORO_POLL_C
#define DISP_CORO_POLL_C

#define _POSIX_C_SOURCE 200809L
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ucontext.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#ifndef DEBUG
//#define DEBUG
#endif
#include "../disp.h"
#include "disp.coro.h"

/* =============================== select 语句 =============================== */

/* 简化的通道访问宏 */
#define CHAN(v)             v->data->chan
#define CAP(c)              c->cap
#define SIZE(c)             c->size
#define SIZE_INC(c)         c->size++
#define SIZE_DEC(c)         c->size--
#define HEAD(c)             c->head
#define TAIL(c)             c->tail
#define BUF(c)              c->buffer
#define WAIT_SEND(c)        c->wait_send
#define WAIT_RECV(c)        c->wait_recv
#define LOCK(c)             c->lock
#define CLOSED(c)           c->closed
#define DIRECT(c)           c->direct_value
#define SET_DIRECT(c, v)    c->direct_value = v
#define SET_HEAD(c, h)      c->head = h
#define SET_TAIL(c, t)      c->tail = t
#define SET_WAIT_SEND(c, q) c->wait_send = q
#define SET_WAIT_RECV(c, q) c->wait_recv = q

typedef enum {
    CASE_RECV,
    CASE_SEND,
    CASE_AFTER,
    CASE_DEFAULT
} case_type_t;

typedef struct {
    case_type_t type;
    disp_val *channel;      // evaluated channel object
    disp_val *value;        // evaluated value (for send)
    long timeout_ms;        // for after
    int timer_fd;           // for after
    disp_val *body;         // list of expressions to evaluate
} case_info_t;

/* 从等待队列中移除指定协程 */
static void remove_from_queue(disp_val **head, disp_val *coro) {
    disp_val *prev = NULL, *cur = *head;
    while (cur) {
        if (cur == coro) {
            if (prev)
                prev->data->coro->next = cur->data->coro->next;
            else
                *head = cur->data->coro->next;
            break;
        }
        prev = cur;
        cur = cur->data->coro->next;
    }
}

/* ======================== 立即尝试操作（在锁内调用） ======================== */

/* 尝试 recv：返回 1 表示成功，*value 被赋值；0 表示不可立即执行 */
static int try_recv_locked(disp_channel_t *c, disp_val **value) {
    if (CAP(c) == 0) {
        // 无缓冲通道：优先检查是否有等待的发送者，其次检查 direct_value（由已唤醒的发送者留下）
        if (WAIT_SEND(c)) {
            disp_val *waiter = WAIT_SEND(c);
            SET_WAIT_SEND(c, waiter->data->coro->next);
            *value = DIRECT(c);
            SET_DIRECT(c, NIL);
            scheduler_add(waiter);
            return 1;
        } else if (DIRECT(c) != NIL) {
            // 发送者已经将值放入 direct_value（例如在 select 唤醒后）
            *value = DIRECT(c);
            SET_DIRECT(c, NIL);
            return 1;
        } else if (CLOSED(c)) {
            *value = NIL;
            return 1;
        }
        return 0;
    } else {
        // 有缓冲通道
        if (SIZE(c) > 0) {
            *value = BUF(c)[HEAD(c)];
            SET_HEAD(c, (HEAD(c) + 1) % CAP(c));
            SIZE_DEC(c);
            if (WAIT_SEND(c)) {
                disp_val *waiter = WAIT_SEND(c);
                SET_WAIT_SEND(c, waiter->data->coro->next);
                scheduler_add(waiter);
            }
            return 1;
        } else if (CLOSED(c)) {
            *value = NIL;
            return 1;
        }
        return 0;
    }
}

/* 尝试 send：返回 1 表示成功；0 表示不可立即执行 */
static int try_send_locked(disp_channel_t *c, disp_val *val) {
    if (CAP(c) == 0) {
        if (WAIT_RECV(c)) {
            disp_val *waiter = WAIT_RECV(c);
            SET_WAIT_RECV(c, waiter->data->coro->next);
            SET_DIRECT(c, val);
            scheduler_add(waiter);
            return 1;
        }
        // 无缓冲且没有等待的接收者，不能发送（会挂起）
        return 0;
    } else {
        if (!CLOSED(c) && SIZE(c) < CAP(c)) {
            BUF(c)[TAIL(c)] = val;
            SET_TAIL(c, (TAIL(c) + 1) % CAP(c));
            SIZE_INC(c);
            if (WAIT_RECV(c)) {
                disp_val *waiter = WAIT_RECV(c);
                SET_WAIT_RECV(c, waiter->data->coro->next);
                scheduler_add(waiter);
            }
            return 1;
        }
        return 0;
    }
}

/* 外部接口：尝试 recv，若成功执行 body 并返回结果 */
static disp_val *try_recv(disp_val *ch, disp_val *body, int *executed) {
    disp_channel_t *c = CHAN(ch);
    gc_pthread_mutex_lock(LOCK(c));
    disp_val *value = NULL;
    int ok = try_recv_locked(c, &value);
    gc_pthread_mutex_unlock(LOCK(c));
    if (!ok) return NULL;

    // 绑定 'it' 并执行 body
    disp_val *it_sym = disp_find_symbol("it");
    disp_val *old_it = it_sym ? disp_get_symbol_value(it_sym) : NIL;
    disp_define_symbol("it", value, 0);

    disp_val *result = NIL;
    disp_val *body_it = body;
    while (body_it && T(body_it) == DISP_CONS) {
        result = disp_eval(disp_car(body_it));
        body_it = disp_cdr(body_it);
    }

    if (it_sym)
        disp_define_symbol("it", old_it, 0);
    else
        disp_define_symbol("it", NIL, 0);

    *executed = 1;
    return result;
}

/* 外部接口：尝试 send，若成功执行 body 并返回结果 */
static disp_val *try_send(disp_val *ch, disp_val *val, disp_val *body, int *executed) {
    disp_channel_t *c = CHAN(ch);
    gc_pthread_mutex_lock(LOCK(c));
    if (CLOSED(c)) {
        gc_pthread_mutex_unlock(LOCK(c));
        ERET(NIL, "send on closed channel");
    }
    int ok = try_send_locked(c, val);
    gc_pthread_mutex_unlock(LOCK(c));
    if (!ok) return NULL;

    disp_val *result = NIL;
    disp_val *body_it = body;
    while (body_it && T(body_it) == DISP_CONS) {
        result = disp_eval(disp_car(body_it));
        body_it = disp_cdr(body_it);
    }
    *executed = 1;
    return result;
}

/* 立即尝试 after (timeout_ms == 0) */
static disp_val *try_after(case_info_t *info, int *executed) {
    if (info->timeout_ms == 0) {
        disp_val *result = NIL;
        disp_val *body_it = info->body;
        while (body_it && T(body_it) == DISP_CONS) {
            result = disp_eval(disp_car(body_it));
            body_it = disp_cdr(body_it);
        }
        *executed = 1;
        return result;
    }
    return NULL;
}

/* ======================== 注册等待（挂起前） ======================== */

/* 检查某个 case 是否已经就绪（在注册前） */
static int is_case_ready(case_info_t *info) {
    disp_channel_t *c = CHAN(info->channel);
    gc_pthread_mutex_lock(LOCK(c));
    int ready = 0;
    if (info->type == CASE_RECV) {
        if (CAP(c) == 0)
            ready = (WAIT_SEND(c) != NULL) || (DIRECT(c) != NIL) || CLOSED(c);
        else
            ready = (SIZE(c) > 0) || CLOSED(c);
    } else if (info->type == CASE_SEND) {
        if (CAP(c) == 0)
            ready = (WAIT_RECV(c) != NULL);
        else
            ready = (!CLOSED(c) && SIZE(c) < CAP(c));
    }
    gc_pthread_mutex_unlock(LOCK(c));
    return ready;
}

/* 注册当前协程到等待队列（假设当前没有就绪） */
static void register_case(case_info_t *info, disp_val *current) {
    disp_channel_t *c = CHAN(info->channel);
    gc_pthread_mutex_lock(LOCK(c));
    if (info->type == CASE_RECV) {
        current->data->coro->next = WAIT_RECV(c);
        SET_WAIT_RECV(c, current);
    } else if (info->type == CASE_SEND) {
        current->data->coro->next = WAIT_SEND(c);
        SET_WAIT_SEND(c, current);
    }
    gc_pthread_mutex_unlock(LOCK(c));
}

/* 注册所有 case，如果发现有任何一个 case 在锁内变得就绪，则返回 -1 表示需要重试 */
static int register_all_cases(case_info_t *infos, int count, disp_val *current) {
    for (int i = 0; i < count; i++) {
        case_info_t *info = &infos[i];
        if (info->type == CASE_RECV || info->type == CASE_SEND) {
            // 快速检查（不加锁）避免不必要的锁竞争，但可能误判，所以注册前还要加锁再检查一次
            if (is_case_ready(info))
                return -1;
            register_case(info, current);
            // 注册后再次检查（因为注册过程中条件可能变化），如果就绪则返回 -1 要求重试
            if (is_case_ready(info)) {
                // 需要从等待队列中移除刚刚注册的自己（但为了简化，直接重试，下次会清理）
                return -1;
            }
        } else if (info->type == CASE_AFTER) {
            int tfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
            if (tfd == -1) return -2;
            struct itimerspec its;
            its.it_value.tv_sec = info->timeout_ms / 1000;
            its.it_value.tv_nsec = (info->timeout_ms % 1000) * 1000000;
            its.it_interval.tv_sec = 0;
            its.it_interval.tv_nsec = 0;
            if (timerfd_settime(tfd, 0, &its, NULL) == -1) {
                close(tfd);
                return -2;
            }
            info->timer_fd = tfd;
            event_loop_add_fd(tfd, current, EPOLLIN);
        }
    }
    return 1; // 成功注册
}

/* ======================== 唤醒后处理 ======================== */

/* 唤醒后，找出就绪的 case 并执行其 body，同时清理其他 case 的等待注册 */
static disp_val *handle_ready_cases(case_info_t *infos, int count, disp_val *current) {
    int executed = 0;
    disp_val *result = NIL;

    for (int i = 0; i < count && !executed; i++) {
        case_info_t *info = &infos[i];
        if (info->type == CASE_RECV) {
            disp_channel_t *c = CHAN(info->channel);
            gc_pthread_mutex_lock(LOCK(c));
            disp_val *value = NULL;
            int ok = try_recv_locked(c, &value);
            if (ok) {
                gc_pthread_mutex_unlock(LOCK(c));
                // 绑定 'it' 并执行 body
                disp_val *it_sym = disp_find_symbol("it");
                disp_val *old_it = it_sym ? disp_get_symbol_value(it_sym) : NIL;
                disp_define_symbol("it", value, 0);

                disp_val *body_it = info->body;
                result = NIL;
                while (body_it && T(body_it) == DISP_CONS) {
                    result = disp_eval(disp_car(body_it));
                    body_it = disp_cdr(body_it);
                }

                if (it_sym)
                    disp_define_symbol("it", old_it, 0);
                else
                    disp_define_symbol("it", NIL, 0);
                executed = 1;
            } else {
                // 未就绪，从等待队列中移除自己（可能是被其他 case 唤醒）
                remove_from_queue(&c->wait_recv, current);
                gc_pthread_mutex_unlock(LOCK(c));
            }
        } else if (info->type == CASE_SEND) {
            disp_channel_t *c = CHAN(info->channel);
            gc_pthread_mutex_lock(LOCK(c));
            if (CLOSED(c)) {
                gc_pthread_mutex_unlock(LOCK(c));
                ERET(NIL, "send on closed channel");
            }
            int ok = try_send_locked(c, info->value);
            if (ok) {
                gc_pthread_mutex_unlock(LOCK(c));
                disp_val *body_it = info->body;
                result = NIL;
                while (body_it && T(body_it) == DISP_CONS) {
                    result = disp_eval(disp_car(body_it));
                    body_it = disp_cdr(body_it);
                }
                executed = 1;
            } else {
                remove_from_queue(&c->wait_send, current);
                gc_pthread_mutex_unlock(LOCK(c));
            }
        } else if (info->type == CASE_AFTER && info->timer_fd != -1) {
            // 计时器已触发
            uint64_t buf;
            read(info->timer_fd, &buf, sizeof(buf));
            close(info->timer_fd);
            info->timer_fd = -1;

            disp_val *body_it = info->body;
            result = NIL;
            while (body_it && T(body_it) == DISP_CONS) {
                result = disp_eval(disp_car(body_it));
                body_it = disp_cdr(body_it);
            }
            executed = 1;
        }
    }

    // 清理所有未触发的 timerfd
    for (int i = 0; i < count; i++) {
        if (infos[i].type == CASE_AFTER && infos[i].timer_fd != -1) {
            close(infos[i].timer_fd);
        }
    }

    return executed ? result : NIL;
}

/* ======================== select 主函数 ======================== */
static disp_val *select_builtin(disp_val *expr) {
    disp_val *clauses = disp_cdr(expr);
    if (!clauses || T(clauses) != DISP_CONS)
        ERET(NIL, "select: missing clauses");

    disp_val *current = disp_get_current_coro();
    if (current == NIL)
        ERET(NIL, "select must be called from a coroutine (use 'go')");

    // 统计 clause 个数
    int case_count = 0;
    for (disp_val *c = clauses; c && T(c) == DISP_CONS; c = disp_cdr(c))
        case_count++;
    if (case_count == 0)
        ERET(NIL, "select: no clauses");

    case_info_t *infos = gc_malloc(case_count * sizeof(case_info_t));
    if (!infos) return NIL;
    int default_idx = -1;

    int i = 0;
    for (disp_val *c = clauses; c && T(c) == DISP_CONS; c = disp_cdr(c), i++) {
        disp_val *clause = disp_car(c);
        if (T(clause) != DISP_CONS) {
            gc_free(infos);
            ERET(NIL, "select: malformed clause");
        }
        disp_val *op = disp_car(clause);
        disp_val *body = disp_cdr(clause);

        if (T(op) == DISP_SYMBOL && strcmp(disp_get_symbol_name(op), "default") == 0) {
            infos[i].type = CASE_DEFAULT;
            infos[i].body = body;
            default_idx = i;
            continue;
        }

        if (T(op) != DISP_CONS) {
            gc_free(infos);
            ERET(NIL, "select: operation must be (recv ...), (send ...) or (after ...)");
        }
        disp_val *op_name = disp_car(op);
        if (T(op_name) != DISP_SYMBOL) {
            gc_free(infos);
            ERET(NIL, "select: unknown operation");
        }
        const char *op_str = disp_get_symbol_name(op_name);

        if (strcmp(op_str, "recv") == 0) {
            disp_val *rest = disp_cdr(op);
            if (!rest || T(rest) != DISP_CONS) {
                gc_free(infos);
                ERET(NIL, "select: recv expects (recv ch)");
            }
            disp_val *ch_expr = disp_car(rest);
            disp_val *ch_arg = disp_eval(ch_expr);
            if (T(ch_arg) != DISP_CHAN) {
                gc_free(infos);
                ERET(NIL, "select: recv argument must be a channel");
            }
            infos[i].type = CASE_RECV;
            infos[i].channel = ch_arg;
            infos[i].body = body;
        } else if (strcmp(op_str, "send") == 0) {
            disp_val *rest = disp_cdr(op);
            if (!rest || T(rest) != DISP_CONS) {
                gc_free(infos);
                ERET(NIL, "select: send expects (send ch val)");
            }
            disp_val *ch_expr = disp_car(rest);
            disp_val *ch_arg = disp_eval(ch_expr);
            if (T(ch_arg) != DISP_CHAN) {
                gc_free(infos);
                ERET(NIL, "select: send channel must be a channel");
            }
            disp_val *val_expr = disp_car(disp_cdr(rest));
            disp_val *val_arg = disp_eval(val_expr);
            infos[i].type = CASE_SEND;
            infos[i].channel = ch_arg;
            infos[i].value = val_arg;
            infos[i].body = body;
        } else if (strcmp(op_str, "after") == 0) {
            disp_val *rest = disp_cdr(op);
            if (!rest || T(rest) != DISP_CONS) {
                gc_free(infos);
                ERET(NIL, "select: after expects (after ms)");
            }
            disp_val *ms_expr = disp_car(rest);
            disp_val *ms_val = disp_eval(ms_expr);
            long ms = 0;
            if (T(ms_val) == DISP_INT)
                ms = disp_get_int(ms_val);
            else if (T(ms_val) == DISP_LONG)
                ms = disp_get_long(ms_val);
            else {
                gc_free(infos);
                ERET(NIL, "select: after argument must evaluate to integer");
            }
            infos[i].type = CASE_AFTER;
            infos[i].timeout_ms = ms;
            infos[i].timer_fd = -1;
            infos[i].body = body;
        } else {
            gc_free(infos);
            ERET(NIL, "select: unknown operation %s", op_str);
        }
    }

    int executed = 0;
    disp_val *result = NIL;

    // 第一轮：尝试立即执行
    for (i = 0; i < case_count && !executed; i++) {
        case_info_t *info = &infos[i];
        if (info->type == CASE_RECV)
            result = try_recv(info->channel, info->body, &executed);
        else if (info->type == CASE_SEND)
            result = try_send(info->channel, info->value, info->body, &executed);
        else if (info->type == CASE_AFTER)
            result = try_after(info, &executed);
    }

    if (executed) {
        gc_free(infos);
        return result;
    }

    // 如果有 default 分支，直接执行
    if (default_idx != -1) {
        disp_val *body_it = infos[default_idx].body;
        result = NIL;
        while (body_it && T(body_it) == DISP_CONS) {
            result = disp_eval(disp_car(body_it));
            body_it = disp_cdr(body_it);
        }
        gc_free(infos);
        return result;
    }

    // 第二轮：注册等待（可能会因为条件在注册过程中变得满足而重试）
    while (1) {
        int reg = register_all_cases(infos, case_count, current);
        if (reg == -1) {
            // 有 case 在注册过程中变得就绪，释放 info 并重试整个 select
            gc_free(infos);
            return select_builtin(expr);
        }
        if (reg == -2) {
            gc_free(infos);
            ERET(NIL, "select: failed to create timer");
        }
        break;
    }

    // 挂起当前协程
    scheduler_suspend();

    // 唤醒后处理
    result = handle_ready_cases(infos, case_count, current);
    gc_free(infos);
    return result;
}

/* 模块初始化 */
void disp_init_module(void) {
    DEF("select", MKB(select_builtin, "<#select>"), 1);
}

#endif /* DISP_CORO_POLL_C */
