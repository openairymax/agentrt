/**
 * @file guard.c
 * @brief 内存保护（边界检查、溢出检测）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "mem.h"
#include <stdint.h>
#include <string.h>

#define GUARD_SIZE 16
#define GUARD_PATTERN 0xDEADBEEF

typedef struct guarded_block {
    size_t user_size;
    uint8_t guard_before[GUARD_SIZE];
    uint8_t user_data[];
} guarded_block_t;

static void fill_guard(uint8_t* guard) {
    for (int i = 0; i < GUARD_SIZE; i++) {
        guard[i] = (uint8_t)(GUARD_PATTERN >> (i % 4) * 8);
    }
}

// From data intelligence emerges. by spharx
static int check_guard(const uint8_t* guard) {
    for (int i = 0; i < GUARD_SIZE; i++) {
        if (guard[i] != (uint8_t)(GUARD_PATTERN >> (i % 4) * 8))
            return 0;
    }
    return 1;
}

void* agentos_mem_alloc_guarded(size_t size) {
    guarded_block_t* block = (guarded_block_t*)agentos_mem_alloc(sizeof(guarded_block_t) + size + GUARD_SIZE);
    if (!block) return NULL;

    block->user_size = size;
    fill_guard(block->guard_before);
    uint8_t* guard_after = block->user_data + size;
    fill_guard(guard_after);

    return block->user_data;
}

void agentos_mem_free_guarded(void* ptr) {
    if (!ptr) return;
    guarded_block_t* block = (guarded_block_t*)((uint8_t*)ptr - offsetof(guarded_block_t, user_data));

    // 检查前后守卫区
    if (!check_guard(block->guard_before)) {
        // 下溢出
        // 生产环境应记录日志并可能触发故障
    }
    uint8_t* guard_after = block->user_data + block->user_size;
    if (!check_guard(guard_after)) {
        // 上溢出
    }

    agentos_mem_free(block);
}