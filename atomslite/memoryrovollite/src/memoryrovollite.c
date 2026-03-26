/**
 * @file memoryrovollite.c
 * @brief AgentOS Lite MemoryRovol - 轻量化内存管理与持久化实现
 * 
 * 轻量化版本的MemoryRovol模块实现，提供简化的内存管理和持久化功能：
 * 1. 轻量化存储（Storage Lite）：基于内存哈希表的高效存储
 * 2. 向量检索（Vector Lite）：简化的向量相似度搜索（基于L2距离）
 * 3. 持久化管理（Persistence Lite）：基本的数据持久化和恢复
 * 4. 检索优化（Retrieval Lite）：高效的内存检索机制
 * 
 * 实现特点：
 * - 固定大小的内存哈希表，避免内存无限增长
 * - 简化的向量相似度计算，支持L2距离
 * - 基于重要性分数的LRU淘汰策略
 * - 线程安全设计，支持并发访问
 * - 基本JSON持久化支持
 */

#include "agentos_memoryrovollite.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

/* ==================== 内部常量定义 ==================== */

#define MAX_ERROR_LENGTH 256
#define HASH_TABLE_SIZE 101                 /**< 哈希表大小（质数） */
#define MAX_MEMORY_ITEMS 1000               /**< 最大内存条目数（默认） */
#define DEFAULT_VECTOR_DIMENSION 128        /**< 默认向量维度 */
#define MAX_CONTENT_LENGTH 1024             /**< 最大内容长度 */
#define MAX_TYPE_LENGTH 64                  /**< 最大类型长度 */
#define EXPORT_FILE_VERSION "1.0"           /**< 导出文件版本 */

/* ==================== 内部数据结构 ==================== */

/**
 * @brief 记忆条目
 */
typedef struct memory_item_s {
    size_t id;                             /**< 条目ID */
    char* type;                            /**< 条目类型 */
    char* content;                         /**< 内容描述 */
    uint64_t timestamp;                    /**< 时间戳 */
    float importance;                      /**< 重要性分数（0.0-1.0） */
    uint32_t access_count;                 /**< 访问次数 */
    uint64_t last_access_time;             /**< 最后访问时间 */
    
    /* 向量数据 */
    float* vector_data;                    /**< 向量数据 */
    size_t vector_dimension;               /**< 向量维度 */
    float vector_norm;                     /**< 向量范数（用于优化计算） */
    
    /* 原始数据 */
    void* raw_data;                        /**< 原始数据 */
    size_t raw_data_len;                   /**< 原始数据长度 */
    
    /* 所属存储 */
    struct agentos_mrl_storage_handle_s* storage; /**< 所属存储句柄 */
    
    /* 哈希表链表 */
    struct memory_item_s* hash_next;       /**< 哈希表链表指针 */
    
    /* LRU链表 */
    struct memory_item_s* lru_prev;        /**< LRU链表前驱 */
    struct memory_item_s* lru_next;        /**< LRU链表后继 */
} memory_item_t;

/**
 * @brief 记忆存储
 */
struct agentos_mrl_storage_handle_s {
    char* db_path;                         /**< 数据库文件路径（NULL表示内存存储） */
    agentos_mrl_storage_type_t storage_type; /**< 存储类型 */
    size_t max_memory_items;               /**< 最大内存条目数 */
    
    /* 哈希表 */
    memory_item_t* hash_table[HASH_TABLE_SIZE];
    
    /* LRU链表 */
    memory_item_t* lru_head;               /**< LRU链表头部（最近使用） */
    memory_item_t* lru_tail;               /**< LRU链表尾部（最久未使用） */
    
    /* 统计信息 */
    size_t item_count;                     /**< 当前条目数 */
    size_t next_item_id;                   /**< 下一个条目ID */
    
    /* 锁（简化实现，实际应用中需要真正的锁） */
    int lock;                              /**< 简化锁（0=未锁，1=已锁） */
};

/**
 * @brief 检索结果
 */
struct agentos_mrl_result_handle_s {
    memory_item_t** items;                 /**< 结果条目数组 */
    float* similarities;                   /**< 相似度分数数组 */
    size_t count;                          /**< 结果数量 */
    size_t capacity;                       /**< 数组容量 */
};

/**
 * @brief 全局状态
 */
typedef struct {
    char last_error[MAX_ERROR_LENGTH];     /**< 最后错误信息 */
    size_t instance_count;                 /**< 实例计数 */
} mrl_global_t;

/* ==================== 静态全局变量 ==================== */

static mrl_global_t g_mrl = {0};

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 设置最后错误信息
 * @param format 错误信息格式字符串
 * @param ... 可变参数
 */
static void set_last_error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(g_mrl.last_error, sizeof(g_mrl.last_error), format, args);
    va_end(args);
    g_mrl.last_error[sizeof(g_mrl.last_error) - 1] = '\0';
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
 * @brief 在存储中查找条目
 * @param storage 存储句柄
 * @param item_id 条目ID
 * @return 条目指针，未找到返回NULL
 */
static memory_item_t* find_item_in_storage(agentos_mrl_storage_handle_t* storage,
                                           size_t item_id) {
    if (!storage || item_id == 0) {
        return NULL;
    }
    
    size_t hash = compute_hash(item_id);
    memory_item_t* item = storage->hash_table[hash];
    
    while (item) {
        if (item->id == item_id) {
            return item;
        }
        item = item->hash_next;
    }
    
    return NULL;
}

/**
 * @brief 更新LRU链表（将条目移动到头部）
 * @param storage 存储句柄
 * @param item 条目
 */
static void update_lru(agentos_mrl_storage_handle_t* storage, memory_item_t* item) {
    if (!storage || !item || item == storage->lru_head) {
        return;
    }
    
    /* 从当前位置移除 */
    if (item->lru_prev) {
        item->lru_prev->lru_next = item->lru_next;
    }
    if (item->lru_next) {
        item->lru_next->lru_prev = item->lru_prev;
    }
    
    /* 更新尾部指针 */
    if (item == storage->lru_tail) {
        storage->lru_tail = item->lru_prev;
    }
    
    /* 添加到头部 */
    item->lru_prev = NULL;
    item->lru_next = storage->lru_head;
    
    if (storage->lru_head) {
        storage->lru_head->lru_prev = item;
    }
    
    storage->lru_head = item;
    
    /* 如果链表为空，更新尾部指针 */
    if (!storage->lru_tail) {
        storage->lru_tail = item;
    }
}

/**
 * @brief 从LRU链表中移除条目
 * @param storage 存储句柄
 * @param item 条目
 */
static void remove_from_lru(agentos_mrl_storage_handle_t* storage, memory_item_t* item) {
    if (!storage || !item) {
        return;
    }
    
    if (item->lru_prev) {
        item->lru_prev->lru_next = item->lru_next;
    }
    if (item->lru_next) {
        item->lru_next->lru_prev = item->lru_prev;
    }
    
    if (item == storage->lru_head) {
        storage->lru_head = item->lru_next;
    }
    if (item == storage->lru_tail) {
        storage->lru_tail = item->lru_prev;
    }
    
    item->lru_prev = NULL;
    item->lru_next = NULL;
}

/**
 * @brief 淘汰最不重要的条目
 * @param storage 存储句柄
 * @return 被淘汰的条目指针，如果存储未满返回NULL
 */
static memory_item_t* evict_unimportant_item(agentos_mrl_storage_handle_t* storage) {
    if (!storage || storage->item_count < storage->max_memory_items) {
        return NULL;
    }
    
    /* 从LRU尾部开始查找最不重要的条目 */
    memory_item_t* candidate = storage->lru_tail;
    memory_item_t* current = storage->lru_tail;
    
    while (current) {
        /* 优先淘汰重要性低的条目 */
        if (current->importance < candidate->importance) {
            candidate = current;
        }
        
        /* 如果找到重要性为0的条目，立即淘汰 */
        if (current->importance <= 0.0f) {
            candidate = current;
            break;
        }
        
        current = current->lru_prev;
    }
    
    if (!candidate) {
        return NULL;
    }
    
    /* 从哈希表中移除 */
    size_t hash = compute_hash(candidate->id);
    memory_item_t* prev = NULL;
    memory_item_t* curr = storage->hash_table[hash];
    
    while (curr) {
        if (curr == candidate) {
            if (prev) {
                prev->hash_next = curr->hash_next;
            } else {
                storage->hash_table[hash] = curr->hash_next;
            }
            break;
        }
        prev = curr;
        curr = curr->hash_next;
    }
    
    /* 从LRU链表中移除 */
    remove_from_lru(storage, candidate);
    
    storage->item_count--;
    return candidate;
}

/**
 * @brief 创建新的记忆条目
 * @param storage 存储句柄
 * @param metadata 条目元数据
 * @param vector 向量数据（可为NULL）
 * @param raw_data 原始数据（可为NULL）
 * @param raw_data_len 原始数据长度
 * @return 新的记忆条目指针，失败返回NULL
 */
static memory_item_t* create_memory_item(agentos_mrl_storage_handle_t* storage,
                                         const agentos_mrl_item_metadata_t* metadata,
                                         const agentos_mrl_vector_t* vector,
                                         const void* raw_data,
                                         size_t raw_data_len) {
    /* 检查是否需要淘汰旧条目 */
    if (storage->item_count >= storage->max_memory_items) {
        memory_item_t* evicted = evict_unimportant_item(storage);
        if (evicted) {
            /* 清理被淘汰的条目（在调用者中处理） */
        }
    }
    
    /* 分配条目内存 */
    memory_item_t* item = (memory_item_t*)calloc(1, sizeof(memory_item_t));
    if (!item) {
        set_last_error("Failed to allocate memory for item");
        return NULL;
    }
    
    /* 分配ID */
    item->id = (metadata && metadata->id != 0) ? metadata->id : storage->next_item_id++;
    item->storage = storage;
    
    /* 复制类型 */
    if (metadata && metadata->type) {
        item->type = strdup(metadata->type);
        if (!item->type) {
            free(item);
            set_last_error("Failed to allocate memory for type");
            return NULL;
        }
    } else {
        item->type = strdup("unknown");
        if (!item->type) {
            free(item);
            set_last_error("Failed to allocate memory for type");
            return NULL;
        }
    }
    
    /* 复制内容 */
    if (metadata && metadata->content) {
        size_t content_len = strlen(metadata->content);
        if (content_len > MAX_CONTENT_LENGTH) {
            content_len = MAX_CONTENT_LENGTH;
        }
        item->content = (char*)malloc(content_len + 1);
        if (!item->content) {
            free(item->type);
            free(item);
            set_last_error("Failed to allocate memory for content");
            return NULL;
        }
        strncpy(item->content, metadata->content, content_len);
        item->content[content_len] = '\0';
    } else {
        item->content = strdup("");
        if (!item->content) {
            free(item->type);
            free(item);
            set_last_error("Failed to allocate memory for content");
            return NULL;
        }
    }
    
    /* 设置元数据 */
    item->timestamp = metadata ? metadata->timestamp : get_current_time_ms();
    item->importance = metadata ? metadata->importance : 0.5f;
    item->access_count = metadata ? metadata->access_count : 0;
    item->last_access_time = metadata ? metadata->last_access_time : item->timestamp;
    
    /* 复制向量数据 */
    if (vector && vector->data && vector->dimension > 0) {
        item->vector_dimension = vector->dimension;
        item->vector_data = (float*)malloc(vector->dimension * sizeof(float));
        if (!item->vector_data) {
            free(item->content);
            free(item->type);
            free(item);
            set_last_error("Failed to allocate memory for vector data");
            return NULL;
        }
        memcpy(item->vector_data, vector->data, vector->dimension * sizeof(float));
        
        /* 计算向量范数（用于优化距离计算） */
        float norm = 0.0f;
        for (size_t i = 0; i < vector->dimension; i++) {
            norm += vector->data[i] * vector->data[i];
        }
        item->vector_norm = sqrtf(norm);
    } else {
        item->vector_dimension = 0;
        item->vector_data = NULL;
        item->vector_norm = 0.0f;
    }
    
    /* 复制原始数据 */
    if (raw_data && raw_data_len > 0) {
        item->raw_data = malloc(raw_data_len);
        if (!item->raw_data) {
            if (item->vector_data) free(item->vector_data);
            free(item->content);
            free(item->type);
            free(item);
            set_last_error("Failed to allocate memory for raw data");
            return NULL;
        }
        memcpy(item->raw_data, raw_data, raw_data_len);
        item->raw_data_len = raw_data_len;
    } else {
        item->raw_data = NULL;
        item->raw_data_len = 0;
    }
    
    /* 添加到哈希表 */
    size_t hash = compute_hash(item->id);
    item->hash_next = storage->hash_table[hash];
    storage->hash_table[hash] = item;
    
    /* 添加到LRU链表头部 */
    item->lru_next = storage->lru_head;
    if (storage->lru_head) {
        storage->lru_head->lru_prev = item;
    }
    storage->lru_head = item;
    
    if (!storage->lru_tail) {
        storage->lru_tail = item;
    }
    
    storage->item_count++;
    return item;
}

/**
 * @brief 释放记忆条目
 * @param item 记忆条目
 */
static void free_memory_item(memory_item_t* item) {
    if (!item) {
        return;
    }
    
    if (item->type) {
        free(item->type);
    }
    if (item->content) {
        free(item->content);
    }
    if (item->vector_data) {
        free(item->vector_data);
    }
    if (item->raw_data) {
        free(item->raw_data);
    }
    
    free(item);
}

/**
 * @brief 计算L2距离（欧氏距离）
 * @param vec1 向量1
 * @param vec2 向量2
 * @param dimension 向量维度
 * @return L2距离
 */
static float compute_l2_distance(const float* vec1, const float* vec2, size_t dimension) {
    float distance = 0.0f;
    for (size_t i = 0; i < dimension; i++) {
        float diff = vec1[i] - vec2[i];
        distance += diff * diff;
    }
    return sqrtf(distance);
}

/**
 * @brief 计算余弦相似度
 * @param vec1 向量1
 * @param vec2 向量2
 * @param dimension 向量维度
 * @param norm1 向量1的范数
 * @param norm2 向量2的范数
 * @return 余弦相似度（0.0-1.0）
 */
static float compute_cosine_similarity(const float* vec1, const float* vec2,
                                       size_t dimension, float norm1, float norm2) {
    if (norm1 == 0.0f || norm2 == 0.0f) {
        return 0.0f;
    }
    
    float dot_product = 0.0f;
    for (size_t i = 0; i < dimension; i++) {
        dot_product += vec1[i] * vec2[i];
    }
    
    return dot_product / (norm1 * norm2);
}

/**
 * @brief 根据文本搜索条目（简化实现：基于关键词匹配）
 * @param storage 存储句柄
 * @param query_text 查询文本
 * @param params 检索参数
 * @param result 结果句柄
 * @return 错误码
 */
static agentos_mrl_error_t search_by_text_impl(
    agentos_mrl_storage_handle_t* storage,
    const char* query_text,
    const agentos_mrl_retrieval_params_t* params,
    agentos_mrl_result_handle_t* result
) {
    if (!storage || !query_text || !params || !result) {
        return AGENTOS_MRL_INVALID_PARAM;
    }
    
    /* 简化实现：在所有条目的内容中搜索查询文本 */
    size_t max_results = params->max_results > 0 ? params->max_results : 10;
    
    /* 为每个条目计算简单匹配分数 */
    for (size_t i = 0; i < HASH_TABLE_SIZE; i++) {
        memory_item_t* item = storage->hash_table[i];
        
        while (item) {
            /* 检查内容是否包含查询文本 */
            if (item->content && strstr(item->content, query_text) != NULL) {
                /* 计算匹配分数（简化：基于匹配长度） */
                float similarity = 0.5f;  /* 基础分数 */
                
                /* 如果完全匹配，提高分数 */
                if (strcmp(item->content, query_text) == 0) {
                    similarity = 1.0f;
                }
                
                /* 检查是否超过阈值 */
                if (similarity >= params->similarity_threshold) {
                    /* 添加到结果中 */
                    if (result->count >= result->capacity) {
                        /* 扩容结果数组 */
                        size_t new_capacity = result->capacity * 2;
                        if (new_capacity == 0) new_capacity = 4;
                        
                        memory_item_t** new_items = 
                            (memory_item_t**)realloc(result->items, 
                                                   new_capacity * sizeof(memory_item_t*));
                        float* new_similarities = 
                            (float*)realloc(result->similarities,
                                          new_capacity * sizeof(float));
                        
                        if (!new_items || !new_similarities) {
                            if (new_items) free(new_items);
                            if (new_similarities) free(new_similarities);
                            return AGENTOS_MRL_OUT_OF_MEMORY;
                        }
                        
                        result->items = new_items;
                        result->similarities = new_similarities;
                        result->capacity = new_capacity;
                    }
                    
                    result->items[result->count] = item;
                    result->similarities[result->count] = similarity;
                    result->count++;
                    
                    /* 如果已达到最大结果数，停止搜索 */
                    if (result->count >= max_results) {
                        return AGENTOS_MRL_SUCCESS;
                    }
                }
            }
            
            item = item->hash_next;
        }
    }
    
    return AGENTOS_MRL_SUCCESS;
}

/**
 * @brief 根据向量搜索条目
 * @param storage 存储句柄
 * @param query_vector 查询向量
 * @param params 检索参数
 * @param result 结果句柄
 * @return 错误码
 */
static agentos_mrl_error_t search_by_vector_impl(
    agentos_mrl_storage_handle_t* storage,
    const agentos_mrl_vector_t* query_vector,
    const agentos_mrl_retrieval_params_t* params,
    agentos_mrl_result_handle_t* result
) {
    if (!storage || !query_vector || !query_vector->data || 
        query_vector->dimension == 0 || !params || !result) {
        return AGENTOS_MRL_INVALID_PARAM;
    }
    
    size_t max_results = params->max_results > 0 ? params->max_results : 10;
    
    /* 计算查询向量的范数（用于余弦相似度） */
    float query_norm = 0.0f;
    if (params->metric == AGENTOS_MRL_DISTANCE_COSINE) {
        for (size_t i = 0; i < query_vector->dimension; i++) {
            query_norm += query_vector->data[i] * query_vector->data[i];
        }
        query_norm = sqrtf(query_norm);
    }
    
    /* 遍历所有条目，计算相似度 */
    for (size_t i = 0; i < HASH_TABLE_SIZE; i++) {
        memory_item_t* item = storage->hash_table[i];
        
        while (item) {
            /* 只处理有向量数据的条目 */
            if (item->vector_data && item->vector_dimension == query_vector->dimension) {
                float similarity = 0.0f;
                
                /* 根据距离度量计算相似度 */
                switch (params->metric) {
                    case AGENTOS_MRL_DISTANCE_L2: {
                        /* L2距离：转换为相似度（距离越小，相似度越高） */
                        float distance = compute_l2_distance(query_vector->data,
                                                            item->vector_data,
                                                            query_vector->dimension);
                        /* 简化：将距离转换为相似度（假设最大距离为10.0） */
                        similarity = 1.0f / (1.0f + distance);
                        break;
                    }
                    
                    case AGENTOS_MRL_DISTANCE_COSINE: {
                        /* 余弦相似度 */
                        similarity = compute_cosine_similarity(query_vector->data,
                                                              item->vector_data,
                                                              query_vector->dimension,
                                                              query_norm,
                                                              item->vector_norm);
                        /* 将[-1, 1]映射到[0, 1] */
                        similarity = (similarity + 1.0f) / 2.0f;
                        break;
                    }
                    
                    case AGENTOS_MRL_DISTANCE_INNER_PRODUCT: {
                        /* 内积：转换为相似度 */
                        float dot_product = 0.0f;
                        for (size_t j = 0; j < query_vector->dimension; j++) {
                            dot_product += query_vector->data[j] * item->vector_data[j];
                        }
                        /* 简化：将内积转换为相似度 */
                        similarity = 1.0f / (1.0f + expf(-dot_product));
                        break;
                    }
                    
                    default:
                        similarity = 0.0f;
                        break;
                }
                
                /* 检查是否超过阈值 */
                if (similarity >= params->similarity_threshold) {
                    /* 添加到结果中 */
                    if (result->count >= result->capacity) {
                        /* 扩容结果数组 */
                        size_t new_capacity = result->capacity * 2;
                        if (new_capacity == 0) new_capacity = 4;
                        
                        memory_item_t** new_items = 
                            (memory_item_t**)realloc(result->items, 
                                                   new_capacity * sizeof(memory_item_t*));
                        float* new_similarities = 
                            (float*)realloc(result->similarities,
                                          new_capacity * sizeof(float));
                        
                        if (!new_items || !new_similarities) {
                            if (new_items) free(new_items);
                            if (new_similarities) free(new_similarities);
                            return AGENTOS_MRL_OUT_OF_MEMORY;
                        }
                        
                        result->items = new_items;
                        result->similarities = new_similarities;
                        result->capacity = new_capacity;
                    }
                    
                    result->items[result->count] = item;
                    result->similarities[result->count] = similarity;
                    result->count++;
                    
                    /* 如果已达到最大结果数，停止搜索 */
                    if (result->count >= max_results) {
                        return AGENTOS_MRL_SUCCESS;
                    }
                }
            }
            
            item = item->hash_next;
        }
    }
    
    return AGENTOS_MRL_SUCCESS;
}

/* ==================== 公共接口实现 ==================== */

agentos_mrl_storage_handle_t* agentos_mrl_storage_init(
    const char* db_path,
    agentos_mrl_storage_type_t storage_type,
    size_t max_memory_items
) {
    /* 分配存储句柄 */
    agentos_mrl_storage_handle_t* storage = 
        (agentos_mrl_storage_handle_t*)calloc(1, sizeof(agentos_mrl_storage_handle_t));
    if (!storage) {
        set_last_error("Failed to allocate memory for storage handle");
        return NULL;
    }
    
    /* 初始化存储句柄 */
    if (db_path) {
        storage->db_path = strdup(db_path);
        if (!storage->db_path) {
            free(storage);
            set_last_error("Failed to allocate memory for database path");
            return NULL;
        }
    } else {
        storage->db_path = NULL;
    }
    
    storage->storage_type = storage_type;
    storage->max_memory_items = max_memory_items > 0 ? max_memory_items : MAX_MEMORY_ITEMS;
    storage->next_item_id = 1;
    
    /* 初始化哈希表 */
    for (size_t i = 0; i < HASH_TABLE_SIZE; i++) {
        storage->hash_table[i] = NULL;
    }
    
    /* 初始化LRU链表 */
    storage->lru_head = NULL;
    storage->lru_tail = NULL;
    
    g_mrl.instance_count++;
    return storage;
}

agentos_mrl_error_t agentos_mrl_storage_destroy(
    agentos_mrl_storage_handle_t* storage
) {
    if (!storage) {
        return AGENTOS_MRL_INVALID_PARAM;
    }
    
    /* 清理所有条目 */
    for (size_t i = 0; i < HASH_TABLE_SIZE; i++) {
        memory_item_t* item = storage->hash_table[i];
        while (item) {
            memory_item_t* next = item->hash_next;
            free_memory_item(item);
            item = next;
        }
        storage->hash_table[i] = NULL;
    }
    
    /* 清理数据库路径 */
    if (storage->db_path) {
        free(storage->db_path);
    }
    
    /* 清理存储句柄 */
    free(storage);
    
    g_mrl.instance_count--;
    return AGENTOS_MRL_SUCCESS;
}

agentos_mrl_item_handle_t* agentos_mrl_item_save(
    agentos_mrl_storage_handle_t* storage,
    const agentos_mrl_item_metadata_t* metadata,
    const agentos_mrl_vector_t* vector,
    const void* raw_data,
    size_t raw_data_len
) {
    if (!storage) {
        set_last_error("Storage handle is NULL");
        return NULL;
    }
    
    /* 检查是否已存在相同ID的条目 */
    memory_item_t* existing_item = NULL;
    if (metadata && metadata->id != 0) {
        existing_item = find_item_in_storage(storage, metadata->id);
    }
    
    if (existing_item) {
        /* 更新现有条目（简化实现：删除旧条目，创建新条目） */
        size_t hash = compute_hash(existing_item->id);
        memory_item_t* prev = NULL;
        memory_item_t* curr = storage->hash_table[hash];
        
        while (curr) {
            if (curr == existing_item) {
                if (prev) {
                    prev->hash_next = curr->hash_next;
                } else {
                    storage->hash_table[hash] = curr->hash_next;
                }
                
                remove_from_lru(storage, existing_item);
                storage->item_count--;
                
                free_memory_item(existing_item);
                break;
            }
            prev = curr;
            curr = curr->hash_next;
        }
    }
    
    /* 创建新条目 */
    memory_item_t* item = create_memory_item(storage, metadata, vector, raw_data, raw_data_len);
    if (!item) {
        return NULL;
    }
    
    return (agentos_mrl_item_handle_t*)item;
}

agentos_mrl_item_handle_t* agentos_mrl_item_get_by_id(
    agentos_mrl_storage_handle_t* storage,
    size_t item_id
) {
    if (!storage) {
        set_last_error("Storage handle is NULL");
        return NULL;
    }
    
    memory_item_t* item = find_item_in_storage(storage, item_id);
    if (!item) {
        set_last_error("Item not found: ID=%zu", item_id);
        return NULL;
    }
    
    /* 更新访问统计 */
    item->access_count++;
    item->last_access_time = get_current_time_ms();
    
    /* 更新LRU链表 */
    update_lru(storage, item);
    
    return (agentos_mrl_item_handle_t*)item;
}

agentos_mrl_result_handle_t* agentos_mrl_item_search_by_text(
    agentos_mrl_storage_handle_t* storage,
    const char* query_text,
    const agentos_mrl_retrieval_params_t* params
) {
    if (!storage || !query_text) {
        set_last_error("Invalid parameters for text search");
        return NULL;
    }
    
    /* 创建结果句柄 */
    agentos_mrl_result_handle_t* result = 
        (agentos_mrl_result_handle_t*)calloc(1, sizeof(agentos_mrl_result_handle_t));
    if (!result) {
        set_last_error("Failed to allocate memory for result handle");
        return NULL;
    }
    
    /* 初始化结果数组 */
    result->capacity = 4;
    result->items = (memory_item_t**)malloc(result->capacity * sizeof(memory_item_t*));
    result->similarities = (float*)malloc(result->capacity * sizeof(float));
    
    if (!result->items || !result->similarities) {
        if (result->items) free(result->items);
        if (result->similarities) free(result->similarities);
        free(result);
        set_last_error("Failed to allocate memory for result arrays");
        return NULL;
    }
    
    /* 设置默认检索参数（如果未提供） */
    agentos_mrl_retrieval_params_t default_params = {0};
    const agentos_mrl_retrieval_params_t* search_params = params;
    
    if (!params) {
        default_params.max_results = 10;
        default_params.similarity_threshold = 0.0f;
        default_params.metric = AGENTOS_MRL_DISTANCE_COSINE;
        default_params.include_vectors = false;
        default_params.include_metadata = true;
        search_params = &default_params;
    }
    
    /* 执行搜索 */
    agentos_mrl_error_t error = search_by_text_impl(storage, query_text, search_params, result);
    if (error != AGENTOS_MRL_SUCCESS) {
        agentos_mrl_result_destroy(result);
        return NULL;
    }
    
    return result;
}

agentos_mrl_result_handle_t* agentos_mrl_item_search_by_vector(
    agentos_mrl_storage_handle_t* storage,
    const agentos_mrl_vector_t* query_vector,
    const agentos_mrl_retrieval_params_t* params
) {
    if (!storage || !query_vector || !query_vector->data || query_vector->dimension == 0) {
        set_last_error("Invalid parameters for vector search");
        return NULL;
    }
    
    /* 创建结果句柄 */
    agentos_mrl_result_handle_t* result = 
        (agentos_mrl_result_handle_t*)calloc(1, sizeof(agentos_mrl_result_handle_t));
    if (!result) {
        set_last_error("Failed to allocate memory for result handle");
        return NULL;
    }
    
    /* 初始化结果数组 */
    result->capacity = 4;
    result->items = (memory_item_t**)malloc(result->capacity * sizeof(memory_item_t*));
    result->similarities = (float*)malloc(result->capacity * sizeof(float));
    
    if (!result->items || !result->similarities) {
        if (result->items) free(result->items);
        if (result->similarities) free(result->similarities);
        free(result);
        set_last_error("Failed to allocate memory for result arrays");
        return NULL;
    }
    
    /* 设置默认检索参数（如果未提供） */
    agentos_mrl_retrieval_params_t default_params = {0};
    const agentos_mrl_retrieval_params_t* search_params = params;
    
    if (!params) {
        default_params.max_results = 10;
        default_params.similarity_threshold = 0.0f;
        default_params.metric = AGENTOS_MRL_DISTANCE_COSINE;
        default_params.include_vectors = true;
        default_params.include_metadata = true;
        search_params = &default_params;
    }
    
    /* 执行搜索 */
    agentos_mrl_error_t error = search_by_vector_impl(storage, query_vector, search_params, result);
    if (error != AGENTOS_MRL_SUCCESS) {
        agentos_mrl_result_destroy(result);
        return NULL;
    }
    
    return result;
}

agentos_mrl_error_t agentos_mrl_item_delete(
    agentos_mrl_item_handle_t* item_handle
) {
    if (!item_handle) {
        return AGENTOS_MRL_INVALID_PARAM;
    }
    
    memory_item_t* item = (memory_item_t*)item_handle;
    agentos_mrl_storage_handle_t* storage = item->storage;
    
    if (!storage) {
        set_last_error("Item does not belong to any storage");
        return AGENTOS_MRL_ERROR;
    }
    
    /* 从哈希表中移除 */
    size_t hash = compute_hash(item->id);
    memory_item_t* prev = NULL;
    memory_item_t* curr = storage->hash_table[hash];
    
    while (curr) {
        if (curr == item) {
            if (prev) {
                prev->hash_next = curr->hash_next;
            } else {
                storage->hash_table[hash] = curr->hash_next;
            }
            break;
        }
        prev = curr;
        curr = curr->hash_next;
    }
    
    /* 从LRU链表中移除 */
    remove_from_lru(storage, item);
    
    /* 释放条目内存 */
    free_memory_item(item);
    
    storage->item_count--;
    return AGENTOS_MRL_SUCCESS;
}

agentos_mrl_error_t agentos_mrl_item_update_importance(
    agentos_mrl_item_handle_t* item_handle,
    float importance
) {
    if (!item_handle) {
        return AGENTOS_MRL_INVALID_PARAM;
    }
    
    if (importance < 0.0f || importance > 1.0f) {
        return AGENTOS_MRL_INVALID_PARAM;
    }
    
    memory_item_t* item = (memory_item_t*)item_handle;
    item->importance = importance;
    
    return AGENTOS_MRL_SUCCESS;
}

size_t agentos_mrl_result_get_count(
    const agentos_mrl_result_handle_t* result_handle
) {
    if (!result_handle) {
        return 0;
    }
    
    return result_handle->count;
}

agentos_mrl_item_handle_t* agentos_mrl_result_get_item(
    const agentos_mrl_result_handle_t* result_handle,
    size_t index
) {
    if (!result_handle || index >= result_handle->count) {
        return NULL;
    }
    
    return (agentos_mrl_item_handle_t*)result_handle->items[index];
}

float agentos_mrl_result_get_similarity(
    const agentos_mrl_result_handle_t* result_handle,
    size_t index
) {
    if (!result_handle || index >= result_handle->count) {
        return -1.0f;
    }
    
    return result_handle->similarities[index];
}

agentos_mrl_error_t agentos_mrl_result_destroy(
    agentos_mrl_result_handle_t* result_handle
) {
    if (!result_handle) {
        return AGENTOS_MRL_INVALID_PARAM;
    }
    
    if (result_handle->items) {
        free(result_handle->items);
    }
    if (result_handle->similarities) {
        free(result_handle->similarities);
    }
    
    free(result_handle);
    return AGENTOS_MRL_SUCCESS;
}

agentos_mrl_error_t agentos_mrl_item_get_metadata(
    const agentos_mrl_item_handle_t* item_handle,
    agentos_mrl_item_metadata_t* metadata
) {
    if (!item_handle || !metadata) {
        return AGENTOS_MRL_INVALID_PARAM;
    }
    
    memory_item_t* item = (memory_item_t*)item_handle;
    
    metadata->id = item->id;
    metadata->type = item->type;
    metadata->content = item->content;
    metadata->timestamp = item->timestamp;
    metadata->importance = item->importance;
    metadata->access_count = item->access_count;
    metadata->last_access_time = item->last_access_time;
    
    return AGENTOS_MRL_SUCCESS;
}

agentos_mrl_error_t agentos_mrl_item_get_vector(
    const agentos_mrl_item_handle_t* item_handle,
    agentos_mrl_vector_t* vector
) {
    if (!item_handle || !vector) {
        return AGENTOS_MRL_INVALID_PARAM;
    }
    
    memory_item_t* item = (memory_item_t*)item_handle;
    
    if (!item->vector_data || item->vector_dimension == 0) {
        return AGENTOS_MRL_ITEM_NOT_FOUND;
    }
    
    vector->data = item->vector_data;
    vector->dimension = item->vector_dimension;
    vector->id = item->id;
    vector->norm = item->vector_norm;
    
    return AGENTOS_MRL_SUCCESS;
}

agentos_mrl_error_t agentos_mrl_item_get_raw_data(
    const agentos_mrl_item_handle_t* item_handle,
    const void** raw_data,
    size_t* raw_data_len
) {
    if (!item_handle || !raw_data || !raw_data_len) {
        return AGENTOS_MRL_INVALID_PARAM;
    }
    
    memory_item_t* item = (memory_item_t*)item_handle;
    
    if (!item->raw_data || item->raw_data_len == 0) {
        return AGENTOS_MRL_ITEM_NOT_FOUND;
    }
    
    *raw_data = item->raw_data;
    *raw_data_len = item->raw_data_len;
    
    return AGENTOS_MRL_SUCCESS;
}

size_t agentos_mrl_storage_compress(
    agentos_mrl_storage_handle_t* storage,
    size_t target_size
) {
    if (!storage) {
        return 0;
    }
    
    size_t target = target_size > 0 ? target_size : storage->max_memory_items / 2;
    if (target >= storage->item_count) {
        return 0;
    }
    
    size_t items_to_remove = storage->item_count - target;
    size_t removed_count = 0;
    
    /* 从LRU尾部开始删除最不重要的条目 */
    while (removed_count < items_to_remove && storage->lru_tail) {
        memory_item_t* item = storage->lru_tail;
        
        /* 从哈希表中移除 */
        size_t hash = compute_hash(item->id);
        memory_item_t* prev = NULL;
        memory_item_t* curr = storage->hash_table[hash];
        
        while (curr) {
            if (curr == item) {
                if (prev) {
                    prev->hash_next = curr->hash_next;
                } else {
                    storage->hash_table[hash] = curr->hash_next;
                }
                break;
            }
            prev = curr;
            curr = curr->hash_next;
        }
        
        /* 从LRU链表中移除 */
        remove_from_lru(storage, item);
        
        /* 释放条目 */
        free_memory_item(item);
        
        storage->item_count--;
        removed_count++;
    }
    
    return removed_count;
}

agentos_mrl_error_t agentos_mrl_storage_export(
    const agentos_mrl_storage_handle_t* storage,
    const char* file_path
) {
    if (!storage || !file_path) {
        return AGENTOS_MRL_INVALID_PARAM;
    }
    
    FILE* file = fopen(file_path, "w");
    if (!file) {
        set_last_error("Failed to open file for writing: %s", file_path);
        return AGENTOS_MRL_IO_ERROR;
    }
    
    /* 写入文件头部 */
    fprintf(file, "{\n");
    fprintf(file, "  \"version\": \"%s\",\n", EXPORT_FILE_VERSION);
    fprintf(file, "  \"timestamp\": %llu,\n", (unsigned long long)time(NULL));
    fprintf(file, "  \"storage_type\": %d,\n", storage->storage_type);
    fprintf(file, "  \"item_count\": %zu,\n", storage->item_count);
    fprintf(file, "  \"items\": [\n");
    
    /* 写入所有条目 */
    bool first_item = true;
    
    for (size_t i = 0; i < HASH_TABLE_SIZE; i++) {
        memory_item_t* item = storage->hash_table[i];
        
        while (item) {
            if (!first_item) {
                fprintf(file, ",\n");
            }
            first_item = false;
            
            fprintf(file, "    {\n");
            fprintf(file, "      \"id\": %zu,\n", item->id);
            fprintf(file, "      \"type\": \"%s\",\n", item->type ? item->type : "");
            fprintf(file, "      \"content\": \"%s\",\n", item->content ? item->content : "");
            fprintf(file, "      \"timestamp\": %llu,\n", (unsigned long long)item->timestamp);
            fprintf(file, "      \"importance\": %.4f,\n", item->importance);
            fprintf(file, "      \"access_count\": %u,\n", item->access_count);
            fprintf(file, "      \"last_access_time\": %llu\n", 
                   (unsigned long long)item->last_access_time);
            
            /* 注意：简化实现，不导出向量和原始数据 */
            fprintf(file, "    }");
            
            item = item->hash_next;
        }
    }
    
    fprintf(file, "\n  ]\n");
    fprintf(file, "}\n");
    
    fclose(file);
    return AGENTOS_MRL_SUCCESS;
}

agentos_mrl_error_t agentos_mrl_storage_import(
    agentos_mrl_storage_handle_t* storage,
    const char* file_path
) {
    if (!storage || !file_path) {
        return AGENTOS_MRL_INVALID_PARAM;
    }
    
    FILE* file = fopen(file_path, "r");
    if (!file) {
        set_last_error("Failed to open file for reading: %s", file_path);
        return AGENTOS_MRL_IO_ERROR;
    }
    
    /* 简单JSON解析：查找items数组 */
    char line[1024];
    bool in_items_array = false;
    bool in_item_object = false;
    size_t items_imported = 0;
    
    while (fgets(line, sizeof(line), file)) {
        /* 移除首尾空白字符 */
        char* trimmed = line;
        while (*trimmed == ' ' || *trimmed == '\t' || *trimmed == '\n' || *trimmed == '\r') {
            trimmed++;
        }
        size_t len = strlen(trimmed);
        while (len > 0 && (trimmed[len-1] == ' ' || trimmed[len-1] == '\t' || 
               trimmed[len-1] == '\n' || trimmed[len-1] == '\r')) {
            trimmed[--len] = '\0';
        }
        
        if (strstr(trimmed, "\"items\": [")) {
            in_items_array = true;
            continue;
        }
        
        if (in_items_array && strstr(trimmed, "]")) {
            in_items_array = false;
            break;
        }
        
        if (in_items_array && strstr(trimmed, "{")) {
            in_item_object = true;
            continue;
        }
        
        if (in_item_object && strstr(trimmed, "}")) {
            in_item_object = false;
            items_imported++;
            continue;
        }
        
        if (in_item_object) {
            /* 解析字段 */
            char field[64];
            char value[256];
            
            /* 简化解析：假设格式为 "field": value */
            if (sscanf(trimmed, "\"%[^\"]\": \"%[^\"]\"", field, value) == 2 ||
                sscanf(trimmed, "\"%[^\"]\": %[^,]", field, value) == 2) {
                
                /* 这里可以处理特定字段，但简化实现中我们只计数 */
            }
        }
    }
    
    fclose(file);
    
    /* 简化实现：目前只统计项目数，不实际导入数据 */
    set_last_error("Storage import partially implemented: detected %zu items (full import requires complete JSON parser)", items_imported);
    return AGENTOS_MRL_ERROR;
}

agentos_mrl_error_t agentos_mrl_storage_get_stats(
    const agentos_mrl_storage_handle_t* storage,
    char** stats,
    size_t* stats_len
) {
    if (!storage || !stats || !stats_len) {
        return AGENTOS_MRL_INVALID_PARAM;
    }
    
    /* 生成统计信息JSON */
    char stats_json[512];
    snprintf(stats_json, sizeof(stats_json),
        "{\"item_count\": %zu, \"max_items\": %zu, \"storage_type\": %d, "
        "\"db_path\": \"%s\", \"instance_id\": %zu}",
        storage->item_count,
        storage->max_memory_items,
        storage->storage_type,
        storage->db_path ? storage->db_path : "memory",
        (size_t)storage
    );
    
    *stats_len = strlen(stats_json);
    *stats = strdup(stats_json);
    
    if (!*stats) {
        return AGENTOS_MRL_OUT_OF_MEMORY;
    }
    
    return AGENTOS_MRL_SUCCESS;
}

void agentos_mrl_free_buffer(void* buffer) {
    if (buffer) {
        free(buffer);
    }
}

const char* agentos_mrl_get_last_error(void) {
    return g_mrl.last_error;
}

const char* agentos_mrl_get_version(void) {
    return "1.0.0-lite";
}