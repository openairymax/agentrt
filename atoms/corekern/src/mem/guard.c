/**
 * @file guard.c
 * @brief 内存守卫（边界检测，Debug 构建使用�?
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "mem.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../../../commons/utils/memory/include/memory_compat.h"
#include "../../../commons/utils/string/include/string_compat.h"
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#define GUARD_SIZE     16
#define GUARD_PATTERN  0xDEADBEEFU

typedef struct {
    uint8_t front[GUARD_SIZE];
    uint8_t* user_ptr;
    size_t   user_size;
    uint8_t back[GUARD_SIZE];
} guard_block_t;

static void fill_guard(uint8_t guard[GUARD_SIZE]) {
    for (int i = 0; i < GUARD_SIZE; i++) {
        guard[i] = (uint8_t)(GUARD_PATTERN >> ((i % 4) * 8));
    }
}

static int check_guard(const uint8_t guard[GUARD_SIZE]) {
    for (int i = 0; i < GUARD_SIZE; i++) {
        uint8_t expected = (uint8_t)(GUARD_PATTERN >> ((i % 4) * 8));
        if (guard[i] != expected) return 0;
    }
    return 1;
}

static guard_block_t* create_guarded_block(size_t size) {
    size_t total = sizeof(guard_block_t) + size;
    guard_block_t* block = (guard_block_t*)AGENTOS_MALLOC(total);
    if (!block) return NULL;

    fill_guard(block->front);
    block->user_size = size;
    block->user_ptr = (uint8_t*)block + sizeof(guard_block_t);
    fill_guard(block->back);

    memset(block->user_ptr, 0xCD, size);
    return block;
}

static void validate_guard(guard_block_t* block, const char* operation) {
    if (!block) return;
#ifdef _WIN32
    (void)operation;  /* 避免未使用参数警告 */
#else
    UNREFERENCED_PARAMETER(operation);
#endif

    if (!check_guard(block->front)) {
#ifdef AGENTOS_ENABLE_MEMORY_DEBUG
        printf("GUARD VIOLATION [%s]: front guard corrupted (underflow) at %p\n",
               operation, (void*)block);
#endif
    }
    if (!check_guard(block->back)) {
#ifdef AGENTOS_ENABLE_MEMORY_DEBUG
        printf("GUARD VIOLATION [%s]: back guard corrupted (overflow) at %p, size=%zu\n",
               operation, (void*)block, block->user_size);
#endif
    }
}

void* agentos_mem_guard_alloc(size_t size) {
    guard_block_t* block = create_guarded_block(size);
    return block ? block->user_ptr : NULL;
}

void* agentos_mem_guard_alloc_check(size_t size, void** out_block) {
    guard_block_t* block = create_guarded_block(size);
    if (out_block) *out_block = block;
    return block ? block->user_ptr : NULL;
}

void agentos_mem_guard_free(void* ptr) {
    if (!ptr) return;
    guard_block_t* block = (guard_block_t*)((uint8_t*)ptr - sizeof(guard_block_t));
    validate_guard(block, "free");
    AGENTOS_FREE(block);
}

int agentos_mem_guard_check(void* ptr) {
    if (!ptr) return 0;
    guard_block_t* block = (guard_block_t*)((uint8_t*)ptr - sizeof(guard_block_t));
    return check_guard(block->front) && check_guard(block->back);
}

size_t agentos_mem_guard_usable_size(void* ptr) {
    if (!ptr) return 0;
    guard_block_t* block = (guard_block_t*)((uint8_t*)ptr - sizeof(guard_block_t));
    return block->user_size;
}
