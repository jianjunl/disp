
#ifndef DISP_H_DISP_ALLINONE_UNION_H
#error "must be part of disp.h"
#else

#ifndef DISP_ALLINONE_UNION_H
#define DISP_ALLINONE_UNION_H

/* 统一的对象数据联合 */
union disp_data {
    /* 基本数值类型 */
    char byte_val;
    short short_val;
    int int_val;
    long long_val;
    float float_val;
    double double_val;

    /* 字符串 */
    struct {
        char *str;
        size_t len;
    } string_val;

    /* cons 单元 */
    struct {
        disp_val *car;
        disp_val *cdr;
    } cons;

    /* 符号 */
    struct {
        char *name;
        disp_val *value;
    } symbol;

    /* 内置函数 / 系统调用 */
    struct {
        disp_builtin_t func;
        char *desc;
    } builtin; // for special_form/syntax
    struct {
        disp_syscall_t func;
        char *desc;
    } syscall; // for function/primitive

    /* 闭包 / 宏 */
    struct {
        disp_val *params;
        disp_val *body;
    } closure;

    /* 类型 */
    struct {
        char *name;
        disp_val *decl;
    } type_val;

    /* 文件 */
    struct {
        FILE *file;
        char *mode;
    } file_val;

    /* 协程 */
    disp_coro_t *coro;

    /* 通道 */
    disp_channel_t *chan;

    /* 套接字 */
    struct {
        int fd;
    } socket_val;
    /* 线程 */
    struct disp_thread_t *thread;
    /* 互斥锁 */
    gc_mutex_t *mutex;
    /* 条件变量 */
    gc_cond_t *cond;
};

#endif  /* DISP_ALLINONE_UNION_H */
#endif
