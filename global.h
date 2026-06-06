
#ifndef _GLOBAL_H
#define _GLOBAL_H

/*
extern disp_val disp_builtin_roots[];

#define NUM_BUILTIN_ROOTS 16

enum disp_builtin_root_idx {
    IDX_NIL = 0, IDX_TRUE, IDX_QUIT, IDX_ELSE,
    IDX_BYTE, IDX_SHORT, IDX_INT, IDX_LONG, IDX_FLOAT, IDX_DOUBLE, IDX_PNTR,
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

extern disp_sid LAMBDA;
extern disp_sid MACRO;
extern disp_sid LET;
extern disp_sid LETA;
extern disp_sid LETREC;
extern disp_sid LETRECA;
extern disp_sid CONS;
extern disp_sid LIST;
extern disp_sid QUOTE;
extern disp_sid QUASIQUOTE;
extern disp_sid UNQUOTE;
extern disp_sid UNQUOTE_SPLICING;
extern disp_sid APPEND;
extern disp_sid IF;
extern disp_sid BEGIN;
extern disp_sid PROGN;
extern disp_sid COND;
extern disp_sid AND;
extern disp_sid OR;
extern disp_sid SET;
extern disp_sid SETQ;
extern disp_sid DEFINE;

extern disp_sid DEFAULT;
extern disp_sid RECV;
extern disp_sid SEND;
extern disp_sid AFTER;
extern disp_sid DO;
extern disp_sid DOTIMES;
extern disp_sid DOLIST;

#endif /* _GLOBAL_H */
