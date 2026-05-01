
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
GC_DIR      = gc
MOD_DIR     = modules

CFLAGS += -g -std=c11 -pthread -fPIC -Wall -Wextra -I$(GC_DIR) -I$(MOD_DIR)
LDFLAGS += -pthread -ldl -rdynamic
LDLIBS   = -lm -ldl -lpthread -L$(GC_DIR) -lgc

DESTDIR ?=
PREFIX ?= /usr/local
BINDIR := $(PREFIX)/bin
LIBDIR := $(PREFIX)/lib
INCLUDEDIR := $(PREFIX)/include
MODDIR := $(PREFIX)/share/disp/modules

LIB_SRCS := disp.c number.c print.c parse.c load.c eval.c macro.c symbol.c prim.c func.c file.c info.c
LIB_OBJS := $(LIB_SRCS:.c=.o)
MAIN_SRC := main.c
MAIN_OBJ := main.o

MODULES := disp.data.so disp.quote.so \
           disp.lamb.so disp.leta.so \
           disp.flow.so disp.loop.so disp.throw.so \
           disp.math.so disp.string.so \
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

disp.static: $(MAIN_OBJ) libdisp.a | $(GC_LIB_STATIC)
	$(CC) $(LDFLAGS) $^ -lpthread -lm -ldl -L. -ldisp -L$(GC_DIR) -lgc -o $@

disp: $(MAIN_OBJ) libdisp.so | $(GC_LIB_SHARED)
	$(CC) $(LDFLAGS) $< -L. -ldisp $(LDLIBS) -o $@

%.so: $(MOD_DIR)/%.c disp.h $(GC_HEADER) | $(GC_LIB_STATIC)
	$(CC) $(CFLAGS) -shared $< $(LDLIBS) -o $@

install: all
	$(MAKE) -C $(GC_DIR) -f gc.mk install DESTDIR=$(DESTDIR) PREFIX=$(PREFIX)
	$(INSTALL) -d $(DESTDIR)$(BINDIR) $(DESTDIR)$(LIBDIR) $(DESTDIR)$(INCLUDEDIR) $(DESTDIR)$(MODDIR)
	$(INSTALL) -m755 disp $(DESTDIR)$(BINDIR)/disp
	$(INSTALL) -m755 disp.static $(DESTDIR)$(BINDIR)/disp.static
	$(INSTALL) -m644 libdisp.a $(DESTDIR)$(LIBDIR)/libdisp.a
	$(INSTALL) -m755 libdisp.so $(DESTDIR)$(LIBDIR)/libdisp.so
	$(INSTALL) -m644 disp.h $(DESTDIR)$(INCLUDEDIR)/disp.h
	for mod in $(MODULES); do \
	    $(INSTALL) -m755 $$mod $(DESTDIR)$(MODDIR)/$$mod; \
	done
	$(INSTALL) -m644 init.disp $(DESTDIR)$(MODDIR)/init.disp

uninstall:
	$(MAKE) -C $(GC_DIR) -f gc.mk uninstall DESTDIR=$(DESTDIR) PREFIX=$(PREFIX)
	$(RM) $(DESTDIR)$(BINDIR)/disp
	$(RM) $(DESTDIR)$(BINDIR)/disp.static
	$(RM) $(DESTDIR)$(LIBDIR)/libdisp.a
	$(RM) $(DESTDIR)$(LIBDIR)/libdisp.so
	$(RM) $(DESTDIR)$(INCLUDEDIR)/disp.h
	for mod in $(MODULES); do \
	    $(RM) $(DESTDIR)$(MODDIR)/$$mod; \
	done
	$(RM) $(DESTDIR)$(MODDIR)/init.disp

clean:
	$(MAKE) -C $(GC_DIR) -f gc.mk clean
	$(RM) $(LIB_OBJS) $(MAIN_OBJ) $(LIB_OBJS:.o=.d) $(MAIN_OBJ:.o=.d)
	$(RM) $(TARGETS) $(GC_STAMP)

.PHONY: all install uninstall clean
