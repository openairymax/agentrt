/**
 * @file alloc.c
 * @brief 物理内存分配器（带追踪的 malloc/free 封装）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "mem.h"
#include "task.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdatomic.h>

static size_t total_allocated = 0;
static size_t used_allocated = 0;
static size_t peak_allocated = 0;
static agentos_mem_alloc_info_t* alloc_list = NULL;
static agentos_mutex_t* mem_stats_mutex = NULL;

static void update_peak(void) {
    if (used_allocated > peak_allocated) {
        peak_allocated = used_allocated;
    }
}

agentos_error_t agentos_mem_init(size_t heap_size) {
    (void)heap_size;
    if (!mem_stats_mutex) {
        mem_stats_mutex = agentos_mutex_create();
        if (!mem_stats_mutex) return AGENTOS_ENOMEM;
    }
    return AGENTOS_SUCCESS;
}

void agentos_mem_cleanup(void) {
    agentos_mem_check_leaks();

    agentos_mutex_lock(mem_stats_mutex);
    while (alloc_list) {
        agentos_mem_alloc_info_t* info = alloc_list;
        alloc_list = info->next;
        free(info);
    }
    agentos_mutex_unlock(mem_stats_mutex);

    if (mem_stats_mutex) {
        agentos_mutex_destroy(mem_stats_mutex);
        mem_stats_mutex = NULL;
    }
}

static void add_alloc_info(void* ptr, size_t size, const char* file, int line) {
    agentos_mem_alloc_info_t* info = (agentos_mem_alloc_info_t*)malloc(sizeof(agentos_mem_alloc_info_t));
    if (info) {
        info->ptr = ptr;
        info->size = size;
        info->file = file;
        info->line = line;
        info->next = alloc_list;
        alloc_list = info;
    }
}

static void remove_alloc_info_unlocked(void* ptr) {
    if (!ptr) return;
    agentos_mem_alloc_info_t* prev = NULL;
    agentos_mem_alloc_info_t* curr = alloc_list;
    while (curr) {
        if (curr->ptr == ptr) {
            if (prev) {
                prev->next = curr->next;
            } else {
                alloc_list = curr->next;
            }
            free(curr);
            break;
        }
        prev = curr;
        curr = curr->next;
    }
}

void* agentos_mem_alloc_ex(size_t size, const char* file, int line) {
    void* ptr = malloc(size);
    if (ptr && mem_stats_mutex) {
        agentos_mutex_lock(mem_stats_mutex);
        total_allocated += size;
        used_allocated += size;
        update_peak();
        add_alloc_info(ptr, size, file, line);
        agentos_mutex_unlock(mem_stats_mutex);
    }
    return ptr;
}

void* agentos_mem_alloc(size_t size) {
    return agentos_mem_alloc_ex(size, __FILE__, __LINE__);
}

void* agentos_mem_aligned_alloc_ex(size_t size, size_t alignment, const char* file, int line) {
    void* ptr = NULL;
#ifdef _WIN32
    ptr = _aligned_malloc(size, alignment);
#else
    if (posix_memalign(&ptr, alignment, size) != 0) ptr = NULL;
#endif
    if (ptr && mem_stats_mutex) {
        agentos_mutex_lock(mem_stats_mutex);
        total_allocated += size;
        used_allocated += size;
        update_peak();
        add_alloc_info(ptr, size, file, line);
        agentos_mutex_unlock(mem_stats_mutex);
    }
    return ptr;
}

void* agentos_mem_aligned_alloc(size_t size, size_t alignment) {
    return agentos_mem_aligned_alloc_ex(size, alignment, __FILE__, __LINE__);
}

void agentos_mem_free(void* ptr) {
    if (!ptr) return;

    if (mem_stats_mutex) {
        agentos_mutex_lock(mem_stats_mutex);
        agentos_mem_alloc_info_t* info = alloc_list;
        while (info) {
            if (info->ptr == ptr) {
                used_allocated -= info->size;
                remove_alloc_info_unlocked(ptr);
                break;
            }
            info = info->next;
        }
        agentos_mutex_unlock(mem_stats_mutex);
    }
    free(ptr);
}

void agentos_mem_aligned_free(void* ptr) {
    if (!ptr) return;

    if (mem_stats_mutex) {
        agentos_mutex_lock(mem_stats_mutex);
        agentos_mem_alloc_info_t* info = alloc_list;
        while (info) {
            if (info->ptr == ptr) {
                used_allocated -= info->size;
                remove_alloc_info_unlocked(ptr);
                break;
            }
            info = info->next;
        }
        agentos_mutex_unlock(mem_stats_mutex);
    }
#ifdef _WIN32
    _aligned_free(ptr);
#else
    free(ptr);
#endif
}

void* agentos_mem_realloc_ex(void* ptr, size_t new_size, const char* file, int line) {
    if (!ptr) return agentos_mem_alloc_ex(new_size, file, line);
    if (new_size == 0) {
        agentos_mem_free(ptr);
        return NULL;
    }

    size_t old_size = 0;
    if (mem_stats_mutex) {
        agentos_mutex_lock(mem_stats_mutex);
        agentos_mem_alloc_info_t* info = alloc_list;
        while (info) {
            if (info->ptr == ptr) {
                old_size = info->size;
                remove_alloc_info_unlocked(ptr);
                break;
            }
            info = info->next;
        }
        agentos_mutex_unlock(mem_stats_mutex);
    }

    void* new_ptr = realloc(ptr, new_size);
    if (new_ptr && mem_stats_mutex) {
        agentos_mutex_lock(mem_stats_mutex);
        used_allocated -= old_size;
        used_allocated += new_size;
        total_allocated += new_size;
        update_peak();
        add_alloc_info(new_ptr, new_size, file, line);
        agentos_mutex_unlock(mem_stats_mutex);
    }
    return new_ptr;
}

void* agentos_mem_realloc(void* ptr, size_t new_size) {
    return agentos_mem_realloc_ex(ptr, new_size, __FILE__, __LINE__);
}

void agentos_mem_stats(size_t* out_total, size_t* out_used, size_t* out_peak) {
    if (mem_stats_mutex) agentos_mutex_lock(mem_stats_mutex);
    if (out_total) *out_total = total_allocated;
    if (out_used) *out_used = used_allocated;
    if (out_peak) *out_peak = peak_allocated;
    if (mem_stats_mutex) agentos_mutex_unlock(mem_stats_mutex);
}

size_t agentos_mem_check_leaks(void) {
    size_t leak_count = 0;
    size_t leak_size = 0;

    if (mem_stats_mutex) agentos_mutex_lock(mem_stats_mutex);

    agentos_mem_alloc_info_t* info = alloc_list;
    while (info) {
        leak_count++;
        leak_size += info->size;
        printf("Memory leak: %p, size: %zu, file: %s, line: %d\n",
               info->ptr, info->size, info->file, info->line);
        info = info->next;
    }

    if (leak_count > 0) {
        printf("Total memory leaks: %zu allocations, %zu bytes\n", leak_count, leak_size);
    } else {
        printf("No memory leaks detected\n");
    }

    if (mem_stats_mutex) agentos_mutex_unlock(mem_stats_mutex);

    return leak_count;
}
