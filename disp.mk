
CC ?= gcc
AR ?= ar
INSTALL ?= install
RM ?= rm -f

BUILD ?= release
ifeq ($(BUILD), release)
    CFLAGS ?= -O2 -DNDEBUG
else
    CFLAGS ?= -g -O0 -DDEBUG
endif

# 子目录
BESTLINE_DIR = bestline
include $(BESTLINE_DIR)/bestline.mk
GC_DIR      = gc
MOD_DIR     = modules

CFLAGS += -g -std=c11 -pthread -fPIC -Wall -Wextra -I$(GC_DIR) -I$(MOD_DIR)
LDFLAGS += -pthread -ldl -rdynamic
LDLIBS  += -lm -ldl -lpthread -L$(GC_DIR) -lgc

DESTDIR ?=
PREFIX ?= /usr/local
BINDIR := $(PREFIX)/bin
LIBDIR := $(PREFIX)/lib
INCLUDEDIR := $(PREFIX)/include/disp
MODDIR := $(PREFIX)/share/disp/modules

LIB_SRCS := disp.c literal.c io.c read.c parse.c eval.c load.c flat/flat.c name.c symbol.c env.c \
            number.c str.c cons.c func.c letf.c file.c info.c repl.c args.c \
            tail/tail.c tail/flow.c tail/let.c tail/leta.c tail/letrec.c tail/letreca.c \
            skip/skip.c btree/btree.c  btree/insert.c  btree/split.c btree/delete.c btree/merge.c \
            gc/robin/rapidhash.c gc/robin/robin_table.c gc/robin/siphash.c gc/robin/xxh64.c \
            closure.c apply.c type.c socket.c
LIB_OBJS := $(LIB_SRCS:.c=.o)
MAIN_SRC := main.c
MAIN_OBJ := main.o

MODULES0 := 
MODULES := disp.data.so disp.quote.so \
           disp.lambda.so disp.apply.so disp.throw.so \
           disp.flow.so disp.define.so disp.set.so \
           disp.do.so disp.dotimes.so disp.dolist.so \
           disp.let.so disp.letrec.so \
           disp.cast.so disp.math.so disp.string.so \
           disp.file.so disp.os.so \
           disp.coro.so disp.coro.chan.so disp.coro.poll.so \
           disp.coro.nio.so disp.coro.net.so disp.thread.so

GC_LIB_STATIC  = $(GC_DIR)/libgc.a
GC_LIB_SHARED  = $(GC_DIR)/libgc.so
GC_HEADER      = $(GC_DIR)/gc.h
GC_STAMP       = $(GC_DIR)/.gc-built

TARGETS := libdisp.a libdisp.so disp.static disp $(MODULES)

all: $(TARGETS)

# 生成 GC 库（只执行一次）
$(GC_STAMP):
	$(MAKE) -C $(GC_DIR) -f gc.mk all
	@touch $@

$(GC_LIB_STATIC) $(GC_LIB_SHARED): $(GC_STAMP) ;

# 自动依赖
DEPFLAGS = -MMD -MP
%.o: %.c
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

-include $(LIB_OBJS:.o=.d) $(MAIN_OBJ:.o=.d)

libdisp.a: $(LIB_OBJS)
	$(AR) crs $@ $^

# 动态库也依赖 GC 库（修正处）
libdisp.so: $(LIB_OBJS) | $(GC_LIB_STATIC)
	$(CC) -shared $(LDFLAGS) $^ $(LDLIBS) -o $@

disp.static: $(MAIN_OBJ) libdisp.a $(BESTLINE_LIB) | $(GC_LIB_STATIC)
	$(CC) $(LDFLAGS) $^ -lpthread -lm -ldl -L. -ldisp -L$(GC_DIR) -lgc -o $@

disp: $(MAIN_OBJ) libdisp.so | $(GC_LIB_SHARED) $(BESTLINE_SO)
	$(CC) $(LDFLAGS) $< -L. -ldisp $(LDLIBS) -L$(BESTLINE_DIR) -lbestline -o $@

%.so: $(MOD_DIR)/%.c disp.h $(GC_HEADER) | $(GC_LIB_STATIC)
	$(CC) $(CFLAGS) -shared $< $(LDLIBS) -o $@

install: all
	$(INSTALL) -d $(DESTDIR)$(BINDIR) $(DESTDIR)$(LIBDIR) $(DESTDIR)$(INCLUDEDIR) $(DESTDIR)$(MODDIR)
	$(MAKE) -C $(GC_DIR) -f gc.mk install
	$(MAKE) -C $(BESTLINE_DIR) -f bestline.mk inst
	$(INSTALL) -m755 disp $(DESTDIR)$(BINDIR)/disp
	$(INSTALL) -m755 disp.static $(DESTDIR)$(BINDIR)/disp.static
	$(INSTALL) -m644 libdisp.a $(DESTDIR)$(LIBDIR)/libdisp.a
	$(INSTALL) -m755 libdisp.so $(DESTDIR)$(LIBDIR)/libdisp.so
	$(INSTALL) -m644 *.h $(DESTDIR)$(INCLUDEDIR)
	$(INSTALL) -d $(DESTDIR)$(INCLUDEDIR)/modules
	$(INSTALL) -m644 modules/*.h $(DESTDIR)$(INCLUDEDIR)/modules
	$(INSTALL) -d $(DESTDIR)$(INCLUDEDIR)/gc
	$(INSTALL) -m644 gc/*.h $(DESTDIR)$(INCLUDEDIR)/gc
	for mod in $(MODULES); do \
	    $(INSTALL) -m755 $$mod $(DESTDIR)$(MODDIR)/$$mod; \
	done
	$(INSTALL) -m644 init.lisp $(DESTDIR)$(MODDIR)/init.lisp
	$(INSTALL) -m644 repl.lisp $(DESTDIR)$(MODDIR)/repl.lisp

uninstall:
	$(MAKE) -C $(GC_DIR) -f gc.mk uninstall
	$(MAKE) -C $(BESTLINE_DIR) -f bestline.mk uninst
	$(RM) $(DESTDIR)$(BINDIR)/disp
	$(RM) $(DESTDIR)$(BINDIR)/disp.static
	$(RM) $(DESTDIR)$(LIBDIR)/libdisp.a
	$(RM) $(DESTDIR)$(LIBDIR)/libdisp.so
	$(RM) -r $(DESTDIR)$(INCLUDEDIR)
	for mod in $(MODULES); do \
	    $(RM) $(DESTDIR)$(MODDIR)/$$mod; \
	done
	$(RM) $(DESTDIR)$(MODDIR)/init.lisp
	$(RM) $(DESTDIR)$(MODDIR)/repl.lisp

clean:
	$(MAKE) -C $(GC_DIR) -f gc.mk clean
	$(MAKE) -C $(BESTLINE_DIR) -f bestline.mk clear
	$(RM) $(LIB_OBJS) $(MAIN_OBJ) $(LIB_OBJS:.o=.d) $(MAIN_OBJ:.o=.d)
	$(RM) $(TARGETS) $(GC_STAMP)

.PHONY: all install uninstall clean
