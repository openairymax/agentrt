/**
 * @file sanitizer_cache.h
 * @brief 净化结果缓存内部接口
 */
#ifndef DOMAIN_SANITIZER_CACHE_H
#define DOMAIN_SANITIZER_CACHE_H

#include <stddef.h>
#include <stdint.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 缓存条目 */
typedef struct cache_entry {
    char*   key;                // 原始输入
    char*   cleaned;            // 净化后字符串
    int     risk;
    uint64_t timestamp_ms;      // 存入时间
    struct cache_entry* prev;
    struct cache_entry* next;
} cache_entry_t;

/* 缓存管理器 */
typedef struct sanitizer_cache {
    cache_entry_t*  head;
    cache_entry_t*  tail;
    size_t          capacity;
    size_t          size;
    uint32_t        ttl_ms;
    pthread_mutex_t lock;
} sanitizer_cache_t;

/**
 * @brief 创建缓存
 * @param capacity 最大条目数
 * @param ttl_ms TTL（毫秒），0表示永久
 * @return 缓存句柄，失败返回 NULL
 */
sanitizer_cache_t* sanitizer_cache_create(size_t capacity, uint32_t ttl_ms);

/**
 * @brief 销毁缓存
 */
void sanitizer_cache_destroy(sanitizer_cache_t* cache);

/**
 * @brief 获取缓存结果
 * @param cache 缓存
 * @param input 原始输入
 * @param out_cleaned 输出净化后字符串（命中时需调用者 free）
 * @param out_risk 输出风险等级
 * @return 1 命中，0 未命中，-1 错误
 */
int sanitizer_cache_get(sanitizer_cache_t* cache, const char* input,
                        char** out_cleaned, int* out_risk);

/**
 * @brief 存入缓存
 * @param cache 缓存
 * @param input 原始输入
 * @param cleaned 净化后字符串（内部会复制）
 * @param risk 风险等级
 */
void sanitizer_cache_put(sanitizer_cache_t* cache, const char* input,
                         const char* cleaned, int risk);

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_SANITIZER_CACHE_H */