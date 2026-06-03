
#ifndef _GLOBAL_H
#define _GLOBAL_H

extern disp_val disp_builtin_roots[];

#define NUM_BUILTIN_ROOTS 45

enum disp_builtin_root_idx {
    IDX_NIL = 0, IDX_TRUE, IDX_QUIT,
    IDX_BYTE, IDX_SHORT, IDX_INT, IDX_LONG, IDX_FLOAT, IDX_DOUBLE, IDX_PNTR,
    IDX_LAMBDA, IDX_MACRO, IDX_LET, IDX_LETA, IDX_LETREC, IDX_LETRECA,
    IDX_CONS, IDX_LIST, IDX_QUOTE, IDX_QUASIQUOTE, IDX_UNQUOTE, IDX_UNQUOTE_SPLICING, IDX_APPEND,
    IDX_ELSE, IDX_IF, IDX_BEGIN, IDX_PROGN, IDX_COND, IDX_AND, IDX_OR, IDX_SET, IDX_SETQ, IDX_DEFINE,
    IDX_MODPATH, IDX_ARGS, IDX_IT, IDX_DEFAULT, IDX_RECV, IDX_SEND, IDX_AFTER,
    IDX_DO, IDX_DOTIMES, IDX_DOLIST
};

#define NIL              disp_builtin_roots[IDX_NIL]
#define TRUE             disp_builtin_roots[IDX_TRUE]
#define QUIT             disp_builtin_roots[IDX_QUIT]
#define BYTE             disp_builtin_roots[IDX_BYTE]
#define SHORT            disp_builtin_roots[IDX_SHORT]
#define INT              disp_builtin_roots[IDX_INT]
#define LONG             disp_builtin_roots[IDX_LONG]
#define FLOAT            disp_builtin_roots[IDX_FLOAT]
#define LONG             disp_builtin_roots[IDX_LONG]
#define DOUBLE           disp_builtin_roots[IDX_DOUBLE]
#define PNTR             disp_builtin_roots[IDX_PNTR]
#define LAMBDA           disp_builtin_roots[IDX_LAMBDA]
#define MACRO            disp_builtin_roots[IDX_MACRO]
#define LET              disp_builtin_roots[IDX_LET]
#define LETA             disp_builtin_roots[IDX_LETA]
#define LETREC           disp_builtin_roots[IDX_LETREC]
#define LETRECA          disp_builtin_roots[IDX_LETRECA]
#define CONS             disp_builtin_roots[IDX_CONS]
#define LIST             disp_builtin_roots[IDX_LIST]
#define QUOTE            disp_builtin_roots[IDX_QUOTE]
#define QUASIQUOTE       disp_builtin_roots[IDX_QUASIQUOTE]
#define UNQUOTE          disp_builtin_roots[IDX_UNQUOTE]
#define UNQUOTE_SPLICING disp_builtin_roots[IDX_UNQUOTE_SPLICING]
#define APPEND           disp_builtin_roots[IDX_APPEND]
#define ELSE             disp_builtin_roots[IDX_ELSE]
#define IF               disp_builtin_roots[IDX_IF]
#define BEGIN            disp_builtin_roots[IDX_BEGIN]
#define PROGN            disp_builtin_roots[IDX_PROGN]
#define COND             disp_builtin_roots[IDX_COND]
#define AND              disp_builtin_roots[IDX_AND]
#define OR               disp_builtin_roots[IDX_OR]
#define SET              disp_builtin_roots[IDX_SET]
#define SETQ             disp_builtin_roots[IDX_SETQ]
#define DEFINE           disp_builtin_roots[IDX_DEFINE]
#define MODPATH          disp_builtin_roots[IDX_MODPATH]
#define ARGS             disp_builtin_roots[IDX_ARGS]
#define IT               disp_builtin_roots[IDX_IT]
#define DEFAULT          disp_builtin_roots[IDX_DEFAULT]
#define RECV             disp_builtin_roots[IDX_RECV]
#define SEND             disp_builtin_roots[IDX_SEND]
#define AFTER            disp_builtin_roots[IDX_AFTER]
#define DO               disp_builtin_roots[IDX_DO]
#define DOTIMES          disp_builtin_roots[IDX_DOTIMES]
#define DOLIST           disp_builtin_roots[IDX_DOLIST]

#endif /* _GLOBAL_H */
