
#ifndef _GLOBAL_H
#define _GLOBAL_H

/*
extern disp_val disp_builtin_roots[];

#define NUM_BUILTIN_ROOTS 16

enum disp_builtin_root_idx {
    IDX_NIL = 0, IDX_TRUE, IDX_QUIT, IDX_ELSE,
    IDX_BYTE, IDX_SHORT, IDX_INT, IDX_LONG, IDX_FLOAT, IDX_DOUBLE, IDX_PNTR
    IDX_MODPATH, IDX_ARGS, IDX_IT
};

#define NIL              disp_builtin_roots[IDX_NIL]
#define TRUE             disp_builtin_roots[IDX_TRUE]
#define QUIT             disp_builtin_roots[IDX_QUIT]
#define ELSE             disp_builtin_roots[IDX_ELSE]
#define BYTE             disp_builtin_roots[IDX_BYTE]
#define SHORT            disp_builtin_roots[IDX_SHORT]
#define INT              disp_builtin_roots[IDX_INT]
#define LONG             disp_builtin_roots[IDX_LONG]
#define FLOAT            disp_builtin_roots[IDX_FLOAT]
#define LONG             disp_builtin_roots[IDX_LONG]
#define DOUBLE           disp_builtin_roots[IDX_DOUBLE]
#define PNTR             disp_builtin_roots[IDX_PNTR]
#define MODPATH          disp_builtin_roots[IDX_MODPATH]
#define ARGS             disp_builtin_roots[IDX_ARGS]
#define IT               disp_builtin_roots[IDX_IT]
*/

extern disp_val NIL;
extern disp_val TRUE;
extern disp_val QUIT;
extern disp_val ELSE;
extern disp_val BYTE;
extern disp_val SHORT;
extern disp_val INT;
extern disp_val LONG;
extern disp_val FLOAT;
extern disp_val LONG;
extern disp_val DOUBLE;
extern disp_val PNTR;
extern disp_val MODPATH;
extern disp_val ARGS;
extern disp_val IT;

extern uint64_t LAMBDA;
extern uint64_t MACRO;
extern uint64_t LET;
extern uint64_t LETA;
extern uint64_t LETREC;
extern uint64_t LETRECA;
extern uint64_t CONS;
extern uint64_t LIST;
extern uint64_t QUOTE;
extern uint64_t QUASIQUOTE;
extern uint64_t UNQUOTE;
extern uint64_t UNQUOTE_SPLICING;
extern uint64_t APPEND;
extern uint64_t IF;
extern uint64_t BEGIN;
extern uint64_t PROGN;
extern uint64_t COND;
extern uint64_t AND;
extern uint64_t OR;
extern uint64_t SET;
extern uint64_t SETQ;
extern uint64_t DEFINE;

extern uint64_t DEFAULT;
extern uint64_t RECV;
extern uint64_t SEND;
extern uint64_t AFTER;
extern uint64_t DO;
extern uint64_t DOTIMES;
extern uint64_t DOLIST;

#endif /* _GLOBAL_H */
