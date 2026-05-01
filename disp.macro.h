
#ifndef DISP_MACRO_H
#define DISP_MACRO_H

#define S disp_get_symbol_name
#define V disp_get_symbol_value
#define T(v) v->flag
#define N(v) v->next
#define MKB disp_make_builtin
#define MKF disp_make_syscall
#define MKS disp_make_string
#define DEF(n, v, i) disp_define_symbol(n, v, i)

#ifndef URET
#define URET(R) do { \
    disp_val *_r = R; \
    return _r; \
} while(0);
#endif

#ifndef DISP_INFO 
#define DISP_INFO(I, F, M, ...) do { \
    disp_info_t *_info = disp_get_current_info(); \
    if (_info && _info->filename) { \
        fprintf(F, #I " @ %s:%d: \"" M "\"\n", __FILE__, __LINE__, ##__VA_ARGS__); \
        fprintf(F, "   @ "); \
        int _depth = 0; \
        for (disp_info_t *_p = _info; _p; _p = _p->next) { \
            if (_depth == 0) \
                fprintf(F, "\"%s\":%d:%d: ", _p->filename, _p->line, _p->column); \
            else \
                fprintf(F, "\n      @ \"%s\":%d:%d", _p->filename, _p->line, _p->column); \
            _depth++; \
        } \
    } else { \
        fprintf(F, #I " @ %s:%d: \"" M "\"\n", __FILE__, __LINE__, ##__VA_ARGS__); \
    } \
} while(0)
#endif

#ifndef DISP_IRET
#define DISP_IRET(R, I, F, M, ...) do { \
    DISP_INFO(I, F, M, ##__VA_ARGS__); \
    return R; \
} while(0);
#endif

#ifdef NN 
# error "NN has been defined before " #__LINE__ "@" #__FILE__
#else 
#define NN(a) do { \
    if (!a || a == NIL) { \
        DISP_INFO(ASSERT, stderr, #a " must not be null or nil"); \
        exit(1); \
    } \
} while(0);
#endif

#ifdef A 
# error "A has been defined before " #__LINE__ "@" #__FILE__
#else 
#ifndef DEBUG
#define A(a) ((void)0)
#else
#define A(a) do { \
    if (!a) { \
        DISP_INFO(ASSERT, stderr, #a " must not be null"); \
        exit(1); \
    } \
} while(0);
#endif
#endif

#ifdef AA 
# error "AA has been defined before " #__LINE__ "@" #__FILE__
#else 
#ifndef DEBUG
#define AA(a, aa) ((void)0)
#else
#define AA(a, aa) do { \
    A(a); \
    A(a->aa); \
} while(0);
#endif
#endif

#ifdef AAA 
# error "AAA has been defined before " #__LINE__ "@" #__FILE__
#else
#ifndef DEBUG
#define AAA(a, aa, aaa) ((void)0)
#else
#define AAA(a, aa, aaa) do { \
    AA(a, aa); \
    A(a->aa->aaa); \
} while(0);
#endif
#endif

#ifdef AAAA 
# error "AAAA has been defined before " #__LINE__ "@" #__FILE__
#else 
#ifndef DEBUG
#define AAAA(a, aa, aaa, aaaa) ((void)0)
#else
#define AAAA(a, aa, aaa, aaaa) do { \
    AAA(a, aa, aaa); \
    A(a->aa->aaa->aaaa); \
} while(0);
#endif
#endif

#ifdef AAAAA 
# error "AAAAA has been defined before " #__LINE__ "@" #__FILE__
#else 
#ifndef DEBUG
#define AAAAA(a, aa, aaa, aaaa, aaaaa) ((void)0)
#else
#define AAAAA(a, aa, aaa, aaaa, aaaaa) do { \
    AAAA(a, aa, aaa, aaaa); \
    A(a->aa->aaa->aaaa->aaaaa); \
} while(0);
#endif
#endif

#ifdef INFO 
# error "INFO has been defined before " #__FILENAME__
#else 
#define INFO(M, ...) DISP_INFO(INFO, stdout, M, ##__VA_ARGS__)
#endif

#ifdef ERRO 
# error "ERRO has been defined before " #__FILENAME__
#else 
#define ERRO(M, ...) DISP_INFO(ERRO,stderr, M, ##__VA_ARGS__)
#endif

#ifdef ERET 
# error "ERET has been defined before " #__FILENAME__
#else 
#define ERET(R, M, ...) DISP_IRET(R, ERET, stderr, M, ##__VA_ARGS__)
#endif

#if !defined DEBUG
#define DBG(M, ...) ((void)0)
#else
#define DBG(M, ...) DISP_INFO(DBG, stderr, M, ##__VA_ARGS__)
#endif

#endif /* DISP_MACRO_H */
