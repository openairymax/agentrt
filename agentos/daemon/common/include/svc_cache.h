// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file svc_cache.h
 * @brief 缓存服务兼容层
 * 
 * 本文件是 agentos/commons/utils/cache 的兼容层，提供向后兼容的 API。
 * 新代码应直接使用 #include <cache_common.h>
 * 
 * @see agentos/commons/utils/cache/include/cache_common.h
 */

#ifndef SVC_CACHE_H
#define SVC_CACHE_H

/* 包含 commons 的统一缓存库 */
#include <cache_common.h>

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 类型兼容 ==================== */

/**
 * @brief 兼容旧缓存类型名
 * @deprecated 请使用 cache_t
 */
typedef cache_t svc_cache_t;

/**
 * @brief 兼容旧缓存配置结构
 * @deprecated 请使用 cache_config_t
 */
typedef struct {
    size_t capacity;
    int ttl_sec;
    cache_free_func_t value_free_fn;
} svc_cache_config_t;

/* ==================== 兼容性函数包装 ==================== */

/**
 * @brief 创建缓存（兼容层）
 * @param manager 缓存配置
 * @return 缓存指针，失败返回 NULL
 * @deprecated 请使用 cache_create()
 */
static inline svc_cache_t* svc_cache_create(const svc_cache_config_t* manager) {
    if (!manager) {
        return NULL;
    }
    
    cache_config_t config = cache_create_default_config();
    config.capacity = manager->capacity;
    config.ttl_sec = manager->ttl_sec;
    config.value_free_func = manager->value_free_fn;
    
    return cache_create(&config);
}

/**
 * @brief 销毁缓存（兼容层）
 * @param cache 缓存指针
 * @deprecated 请使用 cache_destroy()
 */
static inline void svc_cache_destroy(svc_cache_t* cache) {
    cache_destroy(cache);
}

/**
 * @brief 获取缓存项（兼容层）
 * @param cache 缓存指针
 * @param key 缓存键
 * @param out_value 输出值
 * @return 1 表示命中，0 表示未命中，负数表示错误
 * @deprecated 请使用 cache_get()
 */
static inline int svc_cache_get(svc_cache_t* cache, const char* key, void** out_value) {
    return cache_get(cache, key, out_value);
}

/**
 * @brief 放入缓存项（兼容层）
 * @param cache 缓存指针
 * @param key 缓存键
 * @param value 缓存值
 * @param value_size 值大小（兼容层忽略此参数，使用指针存储）
 * @return 0 表示成功，非 0 表示错误
 * @deprecated 请使用 cache_put()
 */
static inline int svc_cache_put(svc_cache_t* cache, const char* key, const void* value, size_t value_size) {
    (void)value_size;  /* commons 版本存储指针，不需要大小 */
    cache_put(cache, key, value);
    return 0;
}

/**
 * @brief 清除所有缓存项（兼容层）
 * @param cache 缓存指针
 * @deprecated 请使用 cache_clear()
 */
static inline void svc_cache_clear(svc_cache_t* cache) {
    cache_clear(cache);
}

/**
 * @brief 获取缓存项数量（兼容层）
 * @param cache 缓存指针
 * @return 缓存项数量
 * @deprecated 请使用 cache_get_size()
 */
static inline size_t svc_cache_size(svc_cache_t* cache) {
    return cache_get_size(cache);
}

/**
 * @brief 检查缓存是否为空（兼容层）
 * @param cache 缓存指针
 * @return true 表示为空
 * @deprecated 请使用 cache_get_size() == 0
 */
static inline bool svc_cache_is_empty(svc_cache_t* cache) {
    return cache_get_size(cache) == 0;
}

/* ==================== 额外的便捷函数 ==================== */

/**
 * @brief 检查缓存是否存在键
 * @param cache 缓存指针
 * @param key 缓存键
 * @return true 表示存在
 */
static inline bool svc_cache_contains(svc_cache_t* cache, const char* key) {
    void* value = NULL;
    int result = cache_get(cache, key, &value);
    if (result == 1 && value) {
        /* 注意：cache_get 会复制值，需要释放 */
        free(value);
        return true;
    }
    return false;
}

/**
 * @brief 获取缓存容量
 * @param cache 缓存指针
 * @return 缓存容量
 */
static inline size_t svc_cache_capacity(svc_cache_t* cache) {
    return cache_get_capacity(cache);
}

/**
 * @brief 设置缓存容量
 * @param cache 缓存指针
 * @param capacity 新容量
 */
static inline void svc_cache_set_capacity(svc_cache_t* cache, size_t capacity) {
    cache_set_capacity(cache, capacity);
}

/**
 * @brief 获取缓存 TTL
 * @param cache 缓存指针
 * @return TTL 秒数
 */
static inline int svc_cache_get_ttl(svc_cache_t* cache) {
    return cache_get_ttl(cache);
}

/**
 * @brief 设置缓存 TTL
 * @param cache 缓存指针
 * @param ttl_sec TTL 秒数
 */
static inline void svc_cache_set_ttl(svc_cache_t* cache, int ttl_sec) {
    cache_set_ttl(cache, ttl_sec);
}

#ifdef __cplusplus
}
#endif

#endif /* SVC_CACHE_H */
