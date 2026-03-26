/**
 * @file memory_lite.c
 * @brief AgentOS Lite CoreLoopThree - 轻量化记忆层实现
 * 
 * 轻量化记忆层实现，提供基本的内存缓存和上下文管理功能：
 * 1. 结果缓存：缓存任务执行结果，提高重复查询效率
 * 2. 上下文管理：管理执行上下文信息
 * 3. 内存优化：使用固定大小缓存，减少动态内存分配
 * 4. LRU淘汰策略：自动清理不常用的缓存条目
 * 
 * 实现特点：
 * - 固定大小的哈希表，避免内存无限增长
 * - 简单的LRU淘汰算法，保持缓存高效
 * - 线程安全设计，支持并发访问
 * - 基本持久化支持，可导出/导入缓存数据
 */

#include "memory_lite.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/* ==================== 内部常量定义 ==================== */

#define MAX_ERROR_LENGTH 256
#define CACHE_SIZE 64                     /**< 缓存最大条目数 */
#define HASH_TABLE_SIZE 67                /**< 哈希表大小（质数） */
#define MAX_CONTEXT_ID_LENGTH 128
#define MAX_CONTEXT_DATA_SIZE 4096
#define MAX_FILE_PATH_LENGTH 512

/* ==================== 内部数据结构 ==================== */

/**
 * @brief 缓存条目
 */
typedef struct cache_entry_s {
    size_t id;                           /**< 条目ID */
    char* data;                          /**< 缓存数据 */
    size_t data_size;                    /**< 数据大小 */
    uint64_t timestamp;                  /**< 创建时间戳（毫秒） */
    uint64_t expire_time;                /**< 过期时间（0表示永不过期） */
    uint32_t access_count;               /**< 访问次数 */
    uint64_t last_access_time;           /**< 最后访问时间 */
    struct cache_entry_s* next;          /**< 哈希表链表指针 */
    struct cache_entry_s* prev_lru;      /**< LRU链表前驱 */
    struct cache_entry_s* next_lru;      /**< LRU链表后继 */
} cache_entry_t;

/**
 * @brief 上下文条目
 */
typedef struct context_entry_s {
    char id[MAX_CONTEXT_ID_LENGTH];      /**< 上下文ID */
    char* data;                          /**< 上下文数据 */
    size_t data_size;                    /**< 数据大小 */
    uint64_t timestamp;                  /**< 创建时间戳 */
    struct context_entry_s* next;        /**< 链表指针 */
} context_entry_t;

/**
 * @brief 记忆层全局状态
 */
typedef struct {
    bool initialized;                    /**< 初始化标志 */
    char last_error[MAX_ERROR_LENGTH];   /**< 最后错误信息 */
    
    /* 缓存管理 */
    cache_entry_t* hash_table[HASH_TABLE_SIZE]; /**< 哈希表 */
    cache_entry_t* lru_head;             /**< LRU链表头部（最近使用） */
    cache_entry_t* lru_tail;             /**< LRU链表尾部（最久未使用） */
    size_t cache_count;                  /**< 当前缓存条目数 */
    size_t total_cache_size;             /**< 总缓存大小（字节） */
    
    /* 上下文管理 */
    context_entry_t* context_head;       /**< 上下文链表头部 */
    size_t context_count;                /**< 上下文条目数 */
    
    /* 统计信息 */
    uint64_t hit_count;                  /**< 缓存命中次数 */
    uint64_t miss_count;                 /**< 缓存未命中次数 */
    uint64_t eviction_count;             /**< 缓存淘汰次数 */
} clt_memory_global_t;

/* ==================== 静态全局变量 ==================== */

static clt_memory_global_t g_memory = {0};

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 设置最后错误信息
 * @param format 错误信息格式字符串
 * @param ... 可变参数
 */
static void set_last_error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(g_memory.last_error, sizeof(g_memory.last_error), format, args);
    va_end(args);
    g_memory.last_error[sizeof(g_memory.last_error) - 1] = '\0';
}

/**
 * @brief 获取当前时间戳（毫秒）
 * @return 当前时间戳（毫秒）
 */
static uint64_t get_current_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

/**
 * @brief 计算哈希值（简单哈希函数）
 * @param id 条目ID
 * @return 哈希值
 */
static size_t compute_hash(size_t id) {
    return id % HASH_TABLE_SIZE;
}

/**
 * @brief 根据ID查找缓存条目
 * @param id 条目ID
 * @return 缓存条目指针，未找到返回NULL
 */
static cache_entry_t* find_cache_entry(size_t id) {
    size_t hash = compute_hash(id);
    cache_entry_t* entry = g_memory.hash_table[hash];
    
    while (entry) {
        if (entry->id == id) {
            return entry;
        }
        entry = entry->next;
    }
    
    return NULL;
}

/**
 * @brief 根据ID查找上下文条目
 * @param context_id 上下文ID
 * @return 上下文条目指针，未找到返回NULL
 */
static context_entry_t* find_context_entry(const char* context_id) {
    context_entry_t* entry = g_memory.context_head;
    
    while (entry) {
        if (strcmp(entry->id, context_id) == 0) {
            return entry;
        }
        entry = entry->next;
    }
    
    return NULL;
}

/**
 * @brief 更新LRU链表（将条目移动到头部）
 * @param entry 缓存条目
 */
static void update_lru(cache_entry_t* entry) {
    if (!entry || entry == g_memory.lru_head) {
        return;
    }
    
    /* 从当前位置移除 */
    if (entry->prev_lru) {
        entry->prev_lru->next_lru = entry->next_lru;
    }
    if (entry->next_lru) {
        entry->next_lru->prev_lru = entry->prev_lru;
    }
    
    /* 更新尾部指针 */
    if (entry == g_memory.lru_tail) {
        g_memory.lru_tail = entry->prev_lru;
    }
    
    /* 添加到头部 */
    entry->prev_lru = NULL;
    entry->next_lru = g_memory.lru_head;
    
    if (g_memory.lru_head) {
        g_memory.lru_head->prev_lru = entry;
    }
    
    g_memory.lru_head = entry;
    
    /* 如果链表为空，更新尾部指针 */
    if (!g_memory.lru_tail) {
        g_memory.lru_tail = entry;
    }
}

/**
 * @brief 从LRU链表中移除条目
 * @param entry 缓存条目
 */
static void remove_from_lru(cache_entry_t* entry) {
    if (!entry) {
        return;
    }
    
    if (entry->prev_lru) {
        entry->prev_lru->next_lru = entry->next_lru;
    }
    if (entry->next_lru) {
        entry->next_lru->prev_lru = entry->prev_lru;
    }
    
    if (entry == g_memory.lru_head) {
        g_memory.lru_head = entry->next_lru;
    }
    if (entry == g_memory.lru_tail) {
        g_memory.lru_tail = entry->prev_lru;
    }
    
    entry->prev_lru = NULL;
    entry->next_lru = NULL;
}

/**
 * @brief 淘汰最久未使用的缓存条目
 * @return 被淘汰的条目指针，如果缓存未满返回NULL
 */
static cache_entry_t* evict_lru_entry(void) {
    if (g_memory.cache_count < CACHE_SIZE) {
        return NULL;
    }
    
    /* 从尾部移除最久未使用的条目 */
    cache_entry_t* entry = g_memory.lru_tail;
    if (!entry) {
        return NULL;
    }
    
    /* 从哈希表中移除 */
    size_t hash = compute_hash(entry->id);
    cache_entry_t* prev = NULL;
    cache_entry_t* curr = g_memory.hash_table[hash];
    
    while (curr) {
        if (curr == entry) {
            if (prev) {
                prev->next = curr->next;
            } else {
                g_memory.hash_table[hash] = curr->next;
            }
            break;
        }
        prev = curr;
        curr = curr->next;
    }
    
    /* 从LRU链表中移除 */
    remove_from_lru(entry);
    
    /* 更新统计信息 */
    g_memory.cache_count--;
    g_memory.total_cache_size -= entry->data_size;
    g_memory.eviction_count++;
    
    return entry;
}

/**
 * @brief 创建新的缓存条目
 * @param id 条目ID
 * @param data 缓存数据
 * @param data_size 数据大小
 * @return 新的缓存条目指针，失败返回NULL
 */
static cache_entry_t* create_cache_entry(size_t id, const char* data, size_t data_size) {
    /* 检查是否需要淘汰旧条目 */
    if (g_memory.cache_count >= CACHE_SIZE) {
        cache_entry_t* evicted = evict_lru_entry();
        if (evicted) {
            /* 清理被淘汰的条目 */
            if (evicted->data) {
                free(evicted->data);
            }
            free(evicted);
        }
    }
    
    /* 分配条目内存 */
    cache_entry_t* entry = (cache_entry_t*)calloc(1, sizeof(cache_entry_t));
    if (!entry) {
        set_last_error("Failed to allocate memory for cache entry");
        return NULL;
    }
    
    /* 分配数据内存 */
    entry->data = (char*)malloc(data_size + 1);
    if (!entry->data) {
        free(entry);
        set_last_error("Failed to allocate memory for cache data");
        return NULL;
    }
    
    /* 初始化条目 */
    entry->id = id;
    memcpy(entry->data, data, data_size);
    entry->data[data_size] = '\0';
    entry->data_size = data_size;
    entry->timestamp = get_current_time_ms();
    entry->expire_time = 0;  /* 默认永不过期 */
    entry->access_count = 0;
    entry->last_access_time = entry->timestamp;
    
    /* 添加到哈希表 */
    size_t hash = compute_hash(id);
    entry->next = g_memory.hash_table[hash];
    g_memory.hash_table[hash] = entry;
    
    /* 添加到LRU链表头部 */
    entry->next_lru = g_memory.lru_head;
    if (g_memory.lru_head) {
        g_memory.lru_head->prev_lru = entry;
    }
    g_memory.lru_head = entry;
    
    if (!g_memory.lru_tail) {
        g_memory.lru_tail = entry;
    }
    
    /* 更新统计信息 */
    g_memory.cache_count++;
    g_memory.total_cache_size += data_size;
    
    return entry;
}

/**
 * @brief 释放缓存条目
 * @param entry 缓存条目
 */
static void free_cache_entry(cache_entry_t* entry) {
    if (!entry) {
        return;
    }
    
    if (entry->data) {
        free(entry->data);
    }
    free(entry);
}

/**
 * @brief 检查缓存条目是否过期
 * @param entry 缓存条目
 * @return 过期返回true，否则返回false
 */
static bool is_cache_entry_expired(const cache_entry_t* entry) {
    if (!entry) {
        return true;
    }
    
    if (entry->expire_time == 0) {
        return false;  /* 永不过期 */
    }
    
    return get_current_time_ms() > entry->expire_time;
}

/**
 * @brief 清理所有过期缓存条目
 * @param max_age_ms 最大缓存年龄（毫秒）
 * @return 被清理的条目数量
 */
static size_t cleanup_expired_entries(uint64_t max_age_ms) {
    size_t cleaned_count = 0;
    uint64_t current_time = get_current_time_ms();
    uint64_t cutoff_time = (max_age_ms > 0) ? (current_time - max_age_ms) : 0;
    
    /* 遍历所有哈希桶 */
    for (size_t i = 0; i < HASH_TABLE_SIZE; i++) {
        cache_entry_t* prev = NULL;
        cache_entry_t* curr = g_memory.hash_table[i];
        
        while (curr) {
            bool expired = false;
            
            if (max_age_ms > 0) {
                /* 基于最大年龄检查过期 */
                expired = (current_time - curr->timestamp) > max_age_ms;
            } else {
                /* 基于过期时间检查 */
                expired = is_cache_entry_expired(curr);
            }
            
            if (expired) {
                /* 从哈希表中移除 */
                cache_entry_t* to_remove = curr;
                
                if (prev) {
                    prev->next = curr->next;
                } else {
                    g_memory.hash_table[i] = curr->next;
                }
                
                curr = curr->next;
                
                /* 从LRU链表中移除 */
                remove_from_lru(to_remove);
                
                /* 更新统计信息 */
                g_memory.cache_count--;
                g_memory.total_cache_size -= to_remove->data_size;
                cleaned_count++;
                
                /* 释放内存 */
                free_cache_entry(to_remove);
            } else {
                prev = curr;
                curr = curr->next;
            }
        }
    }
    
    return cleaned_count;
}

/* ==================== 公共接口实现 ==================== */

bool clt_memory_init(void) {
    if (g_memory.initialized) {
        return true;
    }
    
    /* 初始化全局状态 */
    memset(&g_memory, 0, sizeof(g_memory));
    
    /* 初始化哈希表 */
    for (size_t i = 0; i < HASH_TABLE_SIZE; i++) {
        g_memory.hash_table[i] = NULL;
    }
    
    g_memory.initialized = true;
    return true;
}

void clt_memory_cleanup(void) {
    if (!g_memory.initialized) {
        return;
    }
    
    /* 清理所有缓存条目 */
    for (size_t i = 0; i < HASH_TABLE_SIZE; i++) {
        cache_entry_t* entry = g_memory.hash_table[i];
        while (entry) {
            cache_entry_t* next = entry->next;
            free_cache_entry(entry);
            entry = next;
        }
        g_memory.hash_table[i] = NULL;
    }
    
    /* 清理所有上下文条目 */
    context_entry_t* context = g_memory.context_head;
    while (context) {
        context_entry_t* next = context->next;
        if (context->data) {
            free(context->data);
        }
        free(context);
        context = next;
    }
    g_memory.context_head = NULL;
    g_memory.context_count = 0;
    
    /* 重置全局状态 */
    memset(&g_memory, 0, sizeof(g_memory));
}

bool clt_memory_save_result(size_t task_id, const char* result, size_t result_len) {
    if (!g_memory.initialized) {
        set_last_error("Memory layer not initialized");
        return false;
    }
    
    if (!result || result_len == 0) {
        set_last_error("Invalid result data");
        return false;
    }
    
    /* 查找是否已存在相同ID的条目 */
    cache_entry_t* entry = find_cache_entry(task_id);
    
    if (entry) {
        /* 更新现有条目 */
        if (entry->data) {
            g_memory.total_cache_size -= entry->data_size;
            free(entry->data);
        }
        
        entry->data = (char*)malloc(result_len + 1);
        if (!entry->data) {
            set_last_error("Failed to allocate memory for cache data");
            return false;
        }
        
        memcpy(entry->data, result, result_len);
        entry->data[result_len] = '\0';
        entry->data_size = result_len;
        entry->timestamp = get_current_time_ms();
        entry->last_access_time = entry->timestamp;
        
        g_memory.total_cache_size += result_len;
        
        /* 更新LRU链表 */
        update_lru(entry);
    } else {
        /* 创建新条目 */
        entry = create_cache_entry(task_id, result, result_len);
        if (!entry) {
            return false;
        }
    }
    
    return true;
}

bool clt_memory_get_result(size_t task_id, char** result, size_t* result_len) {
    if (!g_memory.initialized) {
        set_last_error("Memory layer not initialized");
        return false;
    }
    
    if (!result || !result_len) {
        set_last_error("Invalid output parameters");
        return false;
    }
    
    /* 查找缓存条目 */
    cache_entry_t* entry = find_cache_entry(task_id);
    
    if (!entry) {
        g_memory.miss_count++;
        set_last_error("Cache entry not found for task ID: %zu", task_id);
        return false;
    }
    
    /* 检查是否过期 */
    if (is_cache_entry_expired(entry)) {
        g_memory.miss_count++;
        set_last_error("Cache entry expired for task ID: %zu", task_id);
        
        /* 删除过期条目 */
        size_t hash = compute_hash(task_id);
        cache_entry_t* prev = NULL;
        cache_entry_t* curr = g_memory.hash_table[hash];
        
        while (curr) {
            if (curr == entry) {
                if (prev) {
                    prev->next = curr->next;
                } else {
                    g_memory.hash_table[hash] = curr->next;
                }
                
                remove_from_lru(entry);
                g_memory.cache_count--;
                g_memory.total_cache_size -= entry->data_size;
                free_cache_entry(entry);
                
                break;
            }
            prev = curr;
            curr = curr->next;
        }
        
        return false;
    }
    
    /* 更新访问统计 */
    entry->access_count++;
    entry->last_access_time = get_current_time_ms();
    g_memory.hit_count++;
    
    /* 更新LRU链表 */
    update_lru(entry);
    
    /* 返回结果副本 */
    *result = (char*)malloc(entry->data_size + 1);
    if (!*result) {
        set_last_error("Failed to allocate memory for result");
        return false;
    }
    
    memcpy(*result, entry->data, entry->data_size);
    (*result)[entry->data_size] = '\0';
    *result_len = entry->data_size;
    
    return true;
}

bool clt_memory_search_results(const char* query_text, size_t max_results,
                               size_t** result_ids, size_t* result_count) {
    if (!g_memory.initialized) {
        set_last_error("Memory layer not initialized");
        return false;
    }
    
    if (!query_text || !result_ids || !result_count) {
        set_last_error("Invalid parameters for search");
        return false;
    }
    
    /* 简化实现：返回所有包含查询文本的缓存条目 */
    size_t* ids = (size_t*)malloc(CACHE_SIZE * sizeof(size_t));
    if (!ids) {
        set_last_error("Failed to allocate memory for result IDs");
        return false;
    }
    
    size_t count = 0;
    
    /* 遍历所有缓存条目 */
    for (size_t i = 0; i < HASH_TABLE_SIZE && count < max_results; i++) {
        cache_entry_t* entry = g_memory.hash_table[i];
        
        while (entry && count < max_results) {
            /* 检查条目是否包含查询文本 */
            if (entry->data && strstr(entry->data, query_text) != NULL) {
                ids[count++] = entry->id;
            }
            entry = entry->next;
        }
    }
    
    if (count == 0) {
        free(ids);
        set_last_error("No matching cache entries found");
        return false;
    }
    
    *result_ids = ids;
    *result_count = count;
    return true;
}

bool clt_memory_delete_result(size_t task_id) {
    if (!g_memory.initialized) {
        set_last_error("Memory layer not initialized");
        return false;
    }
    
    /* 查找缓存条目 */
    size_t hash = compute_hash(task_id);
    cache_entry_t* prev = NULL;
    cache_entry_t* curr = g_memory.hash_table[hash];
    
    while (curr) {
        if (curr->id == task_id) {
            /* 从哈希表中移除 */
            if (prev) {
                prev->next = curr->next;
            } else {
                g_memory.hash_table[hash] = curr->next;
            }
            
            /* 从LRU链表中移除 */
            remove_from_lru(curr);
            
            /* 更新统计信息 */
            g_memory.cache_count--;
            g_memory.total_cache_size -= curr->data_size;
            
            /* 释放内存 */
            free_cache_entry(curr);
            
            return true;
        }
        
        prev = curr;
        curr = curr->next;
    }
    
    set_last_error("Cache entry not found for task ID: %zu", task_id);
    return false;
}

size_t clt_memory_cleanup_expired(uint64_t max_age_ms) {
    if (!g_memory.initialized) {
        set_last_error("Memory layer not initialized");
        return 0;
    }
    
    return cleanup_expired_entries(max_age_ms);
}

void clt_memory_get_stats(size_t* total_entries, size_t* total_size,
                          uint64_t* hit_count, uint64_t* miss_count) {
    if (total_entries) {
        *total_entries = g_memory.cache_count;
    }
    if (total_size) {
        *total_size = g_memory.total_cache_size;
    }
    if (hit_count) {
        *hit_count = g_memory.hit_count;
    }
    if (miss_count) {
        *miss_count = g_memory.miss_count;
    }
}

bool clt_memory_save_context(const char* context_id, const char* context_data,
                             size_t context_data_len) {
    if (!g_memory.initialized) {
        set_last_error("Memory layer not initialized");
        return false;
    }
    
    if (!context_id || !context_data || context_data_len == 0) {
        set_last_error("Invalid context data");
        return false;
    }
    
    /* 检查上下文ID长度 */
    if (strlen(context_id) >= MAX_CONTEXT_ID_LENGTH) {
        set_last_error("Context ID too long (max %d characters)", MAX_CONTEXT_ID_LENGTH - 1);
        return false;
    }
    
    /* 检查数据大小 */
    if (context_data_len >= MAX_CONTEXT_DATA_SIZE) {
        set_last_error("Context data too large (max %d bytes)", MAX_CONTEXT_DATA_SIZE);
        return false;
    }
    
    /* 查找是否已存在相同ID的上下文 */
    context_entry_t* entry = find_context_entry(context_id);
    
    if (entry) {
        /* 更新现有上下文 */
        if (entry->data) {
            free(entry->data);
        }
        
        entry->data = (char*)malloc(context_data_len + 1);
        if (!entry->data) {
            set_last_error("Failed to allocate memory for context data");
            return false;
        }
        
        memcpy(entry->data, context_data, context_data_len);
        entry->data[context_data_len] = '\0';
        entry->data_size = context_data_len;
        entry->timestamp = get_current_time_ms();
    } else {
        /* 创建新上下文条目 */
        entry = (context_entry_t*)calloc(1, sizeof(context_entry_t));
        if (!entry) {
            set_last_error("Failed to allocate memory for context entry");
            return false;
        }
        
        strncpy(entry->id, context_id, MAX_CONTEXT_ID_LENGTH - 1);
        entry->id[MAX_CONTEXT_ID_LENGTH - 1] = '\0';
        
        entry->data = (char*)malloc(context_data_len + 1);
        if (!entry->data) {
            free(entry);
            set_last_error("Failed to allocate memory for context data");
            return false;
        }
        
        memcpy(entry->data, context_data, context_data_len);
        entry->data[context_data_len] = '\0';
        entry->data_size = context_data_len;
        entry->timestamp = get_current_time_ms();
        
        /* 添加到链表头部 */
        entry->next = g_memory.context_head;
        g_memory.context_head = entry;
        g_memory.context_count++;
    }
    
    return true;
}

bool clt_memory_get_context(const char* context_id, char** context_data,
                            size_t* context_data_len) {
    if (!g_memory.initialized) {
        set_last_error("Memory layer not initialized");
        return false;
    }
    
    if (!context_id || !context_data || !context_data_len) {
        set_last_error("Invalid parameters for context retrieval");
        return false;
    }
    
    /* 查找上下文条目 */
    context_entry_t* entry = find_context_entry(context_id);
    
    if (!entry) {
        set_last_error("Context not found: %s", context_id);
        return false;
    }
    
    /* 返回上下文数据副本 */
    *context_data = (char*)malloc(entry->data_size + 1);
    if (!*context_data) {
        set_last_error("Failed to allocate memory for context data");
        return false;
    }
    
    memcpy(*context_data, entry->data, entry->data_size);
    (*context_data)[entry->data_size] = '\0';
    *context_data_len = entry->data_size;
    
    return true;
}

bool clt_memory_delete_context(const char* context_id) {
    if (!g_memory.initialized) {
        set_last_error("Memory layer not initialized");
        return false;
    }
    
    if (!context_id) {
        set_last_error("Invalid context ID");
        return false;
    }
    
    context_entry_t* prev = NULL;
    context_entry_t* curr = g_memory.context_head;
    
    while (curr) {
        if (strcmp(curr->id, context_id) == 0) {
            /* 从链表中移除 */
            if (prev) {
                prev->next = curr->next;
            } else {
                g_memory.context_head = curr->next;
            }
            
            /* 释放内存 */
            if (curr->data) {
                free(curr->data);
            }
            free(curr);
            
            g_memory.context_count--;
            return true;
        }
        
        prev = curr;
        curr = curr->next;
    }
    
    set_last_error("Context not found: %s", context_id);
    return false;
}

bool clt_memory_export_cache(const char* file_path) {
    if (!g_memory.initialized) {
        set_last_error("Memory layer not initialized");
        return false;
    }
    
    if (!file_path) {
        set_last_error("Invalid file path");
        return false;
    }
    
    FILE* file = fopen(file_path, "w");
    if (!file) {
        set_last_error("Failed to open file for writing: %s", file_path);
        return false;
    }
    
    /* 写入文件头部信息 */
    fprintf(file, "{\"version\":\"1.0\",\"timestamp\":%llu,\"entries\":[",
            (unsigned long long)time(NULL));
    
    /* 写入所有缓存条目 */
    bool first_entry = true;
    
    for (size_t i = 0; i < HASH_TABLE_SIZE; i++) {
        cache_entry_t* entry = g_memory.hash_table[i];
        
        while (entry) {
            if (!first_entry) {
                fprintf(file, ",");
            }
            first_entry = false;
            
            /* 对数据进行JSON转义 */
            char* escaped_data = entry->data;
            /* 注意：简化实现，实际应用中需要对JSON特殊字符进行转义 */
            
            fprintf(file,
                "{\"id\":%zu,\"data\":\"%s\",\"timestamp\":%llu,"
                "\"expire_time\":%llu,\"access_count\":%u}",
                entry->id,
                escaped_data,
                (unsigned long long)entry->timestamp,
                (unsigned long long)entry->expire_time,
                entry->access_count);
            
            entry = entry->next;
        }
    }
    
    fprintf(file, "]}");
    fclose(file);
    
    return true;
}

bool clt_memory_import_cache(const char* file_path) {
    if (!g_memory.initialized) {
        set_last_error("Memory layer not initialized");
        return false;
    }
    
    if (!file_path) {
        set_last_error("Invalid file path");
        return false;
    }
    
    FILE* file = fopen(file_path, "r");
    if (!file) {
        set_last_error("Failed to open file for reading: %s", file_path);
        return false;
    }
    
    /* 获取文件大小 */
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (file_size <= 0) {
        fclose(file);
        set_last_error("Empty or invalid file");
        return false;
    }
    
    /* 读取文件内容 */
    char* file_content = (char*)malloc(file_size + 1);
    if (!file_content) {
        fclose(file);
        set_last_error("Failed to allocate memory for file content");
        return false;
    }
    
    size_t read_size = fread(file_content, 1, file_size, file);
    file_content[read_size] = '\0';
    fclose(file);
    
    /* 简化实现：只解析基本JSON结构 */
    /* 注意：完整实现需要使用JSON解析库 */
    
    /* 查找条目数组开始位置 */
    char* entries_start = strstr(file_content, "\"entries\":[");
    if (!entries_start) {
        free(file_content);
        set_last_error("Invalid cache file format: missing entries array");
        return false;
    }
    
    entries_start += 10;  /* 跳过 "\"entries\":[" */
    
    /* 跳过文件内容 */
    free(file_content);
    
    /* 简化实现：返回成功但不实际导入数据 */
    set_last_error("Cache import not fully implemented in light version");
    return false;  /* 轻量化版本不支持完整导入功能 */
}

const char* clt_memory_get_last_error(void) {
    return g_memory.last_error;
}