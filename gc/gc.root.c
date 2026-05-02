/* ============================================================
 * gc.root.c - Precise root management (enhanced with tree support)
 *
 * Implements gc_add_root, gc_remove_root, gc_(un)root_cleanup, and
 * gc_root_malloc.  Precise roots protect specific pointer variables
 * from being missed by conservative stack scanning.
 *
 * Author: jianjunliu@126.com Generated with DeepSeek assistance
 * License: MIT
 * ============================================================ */

#define _GNU_SOURCE

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "gc.h"
//#define DEBUG // Uncomment to debug
#include "log.h"

// ---------- Precise roots (tree extension) ---------- //
typedef struct gc_root {
    void              **ptr_addr;
    struct gc_root     *next;   // 根链兄弟
    struct gc_root     *hold;   // 第一个子节点
    struct gc_root     *more;   // 子节点的兄弟
} gc_root_t;

gc_root_t *gc_roots = NULL;
pthread_mutex_t gc_roots_lock = PTHREAD_MUTEX_INITIALIZER;

/* 释放以 node->hold 为头的子树（不释放 node 本身） */
static void free_hold_subtree(gc_root_t *node) {
    gc_root_t *child = node->hold;
    while (child) {
        gc_root_t *next_child = child->more;
        free_hold_subtree(child);  // 递归释放更深层的子节点
        free(child);
        child = next_child;
    }
    node->hold = NULL;
}

/* 将 child 节点添加到 parent 节点的 hold 链末尾 */
static void append_hold_child(gc_root_t *parent, gc_root_t *child) {
    child->hold = NULL;
    child->more = NULL;
    child->next = NULL;
    if (!parent->hold) {
        parent->hold = child;
    } else {
        gc_root_t *sib = parent->hold;
        while (sib->more) sib = sib->more;
        sib->more = child;
    }
}

/* ---- 原有的基本函数 ---- */
void gc_add_root(void *ptr) {
    gc_root_t *root = calloc(1, sizeof(gc_root_t));
    if (!root) return;
    root->ptr_addr = (void**)ptr;
    root->next = NULL;
    root->hold = NULL;
    root->more = NULL;

    pthread_mutex_lock(&gc_roots_lock);
    root->next = gc_roots;
    gc_roots = root;
    pthread_mutex_unlock(&gc_roots_lock);
    LOG_DEBUG("added precise root %p (points to %p)", ptr, *(void**)ptr);
}

void gc_remove_root(void *ptr) {
    pthread_mutex_lock(&gc_roots_lock);
    gc_root_t *prev = NULL, *curr = gc_roots;
    while (curr) {
        if (curr->ptr_addr == (void**)ptr) {
            // 摘除前先释放其子树
            free_hold_subtree(curr);
            if (prev)
                prev->next = curr->next;
            else
                gc_roots = curr->next;
            free(curr);
            LOG_DEBUG("removed precise root %p", ptr);
            pthread_mutex_unlock(&gc_roots_lock);
            return;
        }
        prev = curr;
        curr = curr->next;
    }
    pthread_mutex_unlock(&gc_roots_lock);
}

void gc_root_cleanup(void **ptr_addr) {
    if (ptr_addr) gc_remove_root(ptr_addr);
}

/* ---- 新增的批量注册函数 ---- */
void gc_root_add(void *base, size_t nmemb, size_t size) {
    if (nmemb == 0) return;
    gc_root_t *root = calloc(1, sizeof(gc_root_t));
    if (!root) return;
    root->ptr_addr = (void**)base;
    root->next = NULL;
    root->hold = NULL;
    root->more = NULL;

    pthread_mutex_lock(&gc_roots_lock);
    root->next = gc_roots;
    gc_roots = root;
    pthread_mutex_unlock(&gc_roots_lock);

    for (size_t i = 0; i < nmemb; i++) {
        gc_root_t *child = calloc(1, sizeof(gc_root_t));
        if (!child) break;
        child->ptr_addr = (void**)((char*)base + i * size);
        child->next = NULL;
        child->hold = NULL;
        child->more = NULL;
        append_hold_child(root, child);
    }
    LOG_DEBUG("added root array base=%p, nmemb=%zu", base, nmemb);
}

/* ---- gc_hold_add ---- */
/* 在子树（root + 其子孙）中查找 *ptr_addr == parent 的节点 */
static bool search_and_hold(gc_root_t *node, void *parent, void *child_addr) {
    if (!node) return false;
    if (*(node->ptr_addr) == parent) {
        gc_root_t *new_child = calloc(1, sizeof(gc_root_t));
        if (new_child) {
            new_child->ptr_addr = (void**)child_addr;
            new_child->next = NULL;
            new_child->hold = NULL;
            new_child->more = NULL;
            append_hold_child(node, new_child);
        }
        return true;
    }
    // 继续在子树中搜索
    return search_and_hold(node->hold, parent, child_addr) ||
           search_and_hold(node->more, parent, child_addr);
}

void gc_hold_add(void *parent, void *child) {
    pthread_mutex_lock(&gc_roots_lock);
    for (gc_root_t *r = gc_roots; r; r = r->next) {
        search_and_hold(r, parent, child);
    }
    pthread_mutex_unlock(&gc_roots_lock);
    LOG_DEBUG("hold_add parent=%p child=%p", parent, child);
}

/* ---- gc_hold_swap ---- */
/* 将 parent 下第一个子节点的 ptr_addr 改为 child */
void gc_hold_swap(void *parent, void *child) {
    pthread_mutex_lock(&gc_roots_lock);
    for (gc_root_t *r = gc_roots; r; r = r->next) {
        if (*(r->ptr_addr) == parent && r->hold) {
            r->hold->ptr_addr = (void**)child;
            break;
        }
        // 也可以递归进子树，但通常 parent 直接对应顶层根节点，这里只检查根链
    }
    pthread_mutex_unlock(&gc_roots_lock);
    LOG_DEBUG("hold_swap parent=%p new_child=%p", parent, child);
}

/* ---- gc_root_swap ---- */
void gc_root_swap(void *old, void *new) {
    pthread_mutex_lock(&gc_roots_lock);
    for (gc_root_t *r = gc_roots; r; r = r->next) {
        if (r->ptr_addr == (void**)old) {
            r->ptr_addr = (void**)new;
            break;
        }
    }
    pthread_mutex_unlock(&gc_roots_lock);
    LOG_DEBUG("root_swap old=%p new=%p", old, new);
}
