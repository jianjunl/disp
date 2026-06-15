# btmk - 构建 btree 库和测试程序
# 用法: make -f bt.mk [all|clean|test|lib|shared]

CC = gcc
CFLAGS = -Wall -Wextra -O2 -g -fPIC
AR = ar
ARFLAGS = rcs

# 源文件列表
BTREE_SRCS = btree.c split.c insert.c delete.c merge.c
BTREE_OBJS = $(BTREE_SRCS:.c=.o)
BTREE_TEST_SRCS = btt.c
BTREE_TEST_OBJS = $(BTREE_TEST_SRCS:.c=.o)

# 目标名称
LIB_STATIC = libbtree.a
LIB_SHARED = libbtree.so
TEST_BIN = btt

# 默认目标
all: $(LIB_STATIC) $(LIB_SHARED) $(TEST_BIN)

# 静态库
$(LIB_STATIC): $(BTREE_OBJS)
	$(AR) $(ARFLAGS) $@ $^

# 动态库
$(LIB_SHARED): $(BTREE_OBJS)
	$(CC) -shared -o $@ $^

# 测试可执行文件（链接静态库）
$(TEST_BIN): $(BTREE_TEST_OBJS) $(LIB_STATIC)
	$(CC) -o $@ $(BTREE_TEST_OBJS) -L. -lbtree
#	$(CC) -static -o $@ $(BTREE_TEST_OBJS) -L. -lbtree

# 或链接动态库（需要设置 LD_LIBRARY_PATH）
# $(TEST_BIN): $(BTREE_TEST_OBJS) $(LIB_SHARED)
# 	$(CC) -o $@ $(BTREE_TEST_OBJS) -L. -lbtree

# 编译规则
%.o: %.c btree.h bt_private.h
	$(CC) $(CFLAGS) -c $< -o $@

# 清理
clean:
	rm -f $(BTREE_OBJS) $(BTREE_TEST_OBJS) $(LIB_STATIC) $(LIB_SHARED) $(TEST_BIN)

# 仅运行测试
test: $(TEST_BIN)
	LD_LIBRARY_PATH=. ./$(TEST_BIN)

.PHONY: all clean test
