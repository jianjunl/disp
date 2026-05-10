# bestline.mk ── 独立的 bestline 库构建（可嵌入 disp.mk）
BESTLINE_DIR ?= .

CC ?= gcc
AR ?= ar
CFLAGS ?= -g -O2 -Wall -Wextra -pthread -fPIC

BESTLINE_SRC  = $(BESTLINE_DIR)/bestline.c
BESTLINE_OBJ  = $(BESTLINE_DIR)/bestline.o
BESTLINE_LIB  = $(BESTLINE_DIR)/libbestline.a
BESTLINE_SO   = $(BESTLINE_DIR)/libbestline.so
BESTLINE_HEAD = $(BESTLINE_DIR)/bestline.h

# 若作为独立项目，可定义 TEST_SRC
TEST_SRC ?= $(BESTLINE_DIR)/test_bestline.c
TEST_BIN ?= $(BESTLINE_DIR)/test_bestline

.PHONY: all clean install

#all: $(BESTLINE_LIB) $(BESTLINE_SO) $(TEST_BIN)
all: $(BESTLINE_LIB) $(BESTLINE_SO)

$(BESTLINE_OBJ): $(BESTLINE_SRC) $(BESTLINE_HEAD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BESTLINE_LIB): $(BESTLINE_OBJ)
	$(AR) rcs $@ $^

$(BESTLINE_SO): $(BESTLINE_OBJ)
	$(CC) -shared -o $@ $^

$(TEST_BIN): $(TEST_SRC) $(BESTLINE_HEAD) $(BESTLINE_LIB)
	$(CC) $(CFLAGS) -o $@ $(TEST_SRC) -L$(BESTLINE_DIR) -lbestline

install: all
	install -d $(DESTDIR)$(PREFIX)/lib $(DESTDIR)$(PREFIX)/include
	install -m 644 $(BESTLINE_LIB) $(DESTDIR)$(PREFIX)/lib/
	install -m 755 $(BESTLINE_SO) $(DESTDIR)$(PREFIX)/lib/
	install -m 644 $(BESTLINE_HEAD) $(DESTDIR)$(PREFIX)/include/

uninstall: all
	rm -f $(BESTLINE_LIB) $(DESTDIR)$(PREFIX)/lib/$(BESTLINE_LIB)
	rm -f $(BESTLINE_LIB) $(DESTDIR)$(PREFIX)/lib/$(BESTLINE_SO)
	rm -f $(DESTDIR)$(PREFIX)/include/$(BESTLINE_HEAD)

clean:
	rm -f $(BESTLINE_OBJ) $(BESTLINE_LIB) $(BESTLINE_SO) $(TEST_BIN)
