/**
 * @file svc_cache.h
 * @brief 通用 LRU 缓存接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef SVC_CACHE_H
#define SVC_CACHE_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 通用缓存类型
 */
typedef struct svc_cache svc_cache_t;

/**
 * @brief 缓存配置
 */
typedef struct {
    size_t capacity;
    int ttl_sec;
    void (*value_free_fn)(void*);
} svc_cache_config_t;

/**
 * @brief 创建通用缓存
 * @param manager 缓存配置
 * @return 缓存指针，失败返回 NULL
 */
svc_cache_t* svc_cache_create(const svc_cache_config_t* manager);

/**
 * @brief 销毁缓存
 * @param cache 缓存指针
 */
void svc_cache_destroy(svc_cache_t* cache);

/**
 * @brief 获取缓存项
 * @param cache 缓存指针
 * @param key 缓存键
 * @param out_value 输出值（需调用者释放，如果设置了 value_free_fn 则内部释放）
 * @return 1 表示命中，0 表示未命中，负数表示错误
 */
int svc_cache_get(svc_cache_t* cache, const char* key, void** out_value);

/**
 * @brief 放入缓存项
 * @param cache 缓存指针
 * @param key 缓存键
 * @param value 缓存值（会被内部复制或转移，取决于配置）
 * @param value_size 值大小（如果是动态分配的值需要提供）
 * @return 0 表示成功，非 0 表示错误
 */
int svc_cache_put(svc_cache_t* cache, const char* key, const void* value, size_t value_size);

/**
 * @brief 清除所有缓存项
 * @param cache 缓存指针
 */
void svc_cache_clear(svc_cache_t* cache);

/**
 * @brief 获取缓存项数量
 * @param cache 缓存指针
 * @return 缓存项数量
 */
size_t svc_cache_size(svc_cache_t* cache);

/**
 * @brief 检查缓存是否为空
 * @param cache 缓存指针
 * @return true 表示为空
 */
bool svc_cache_is_empty(svc_cache_t* cache);

#ifdef __cplusplus
}
#endif

#endif /* SVC_CACHE_H */
