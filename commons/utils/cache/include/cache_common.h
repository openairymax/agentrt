/**
 * @file cache_common.h
 * @brief 通用缓存库
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 提供通用的缓存实现，包括：
 * - LRU 缓存
 * - 哈希表缓存
 * - 带过期时间的缓存
 */

#ifndef AGENTOS_CACHE_COMMON_H
#define AGENTOS_CACHE_COMMON_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 缓存键值对回调函数类型
 */
typedef void (*cache_free_func_t)(void* data);
typedef void* (*cache_copy_func_t)(const void* data);
typedef int (*cache_compare_func_t)(const void* a, const void* b);
typedef unsigned int (*cache_hash_func_t)(const void* key);

/**
 * @brief 缓存配置结构体
 */
typedef struct {
    size_t capacity;           // 缓存容量
    int ttl_sec;               // 过期时间（秒）
    cache_hash_func_t hash_func;   // 哈希函数
    cache_compare_func_t compare_func; // 比较函数
    cache_free_func_t key_free_func;   // 键释放函数
    cache_free_func_t value_free_func; // 值释放函数
    cache_copy_func_t key_copy_func;   // 键复制函数
    cache_copy_func_t value_copy_func; // 值复制函数
} cache_config_t;

/**
 * @brief 缓存句柄类型
 */
typedef struct cache_impl* cache_t;

/**
 * @brief 创建默认缓存配置
 * @return 默认缓存配置
 */
cache_config_t cache_create_default_config(void);

/**
 * @brief 创建缓存
 * @param manager 缓存配置
 * @return 缓存句柄
 */
cache_t cache_create(const cache_config_t* manager);

/**
 * @brief 销毁缓存
 * @param cache 缓存句柄
 */
void cache_destroy(cache_t cache);

/**
 * @brief 从缓存获取值
 * @param cache 缓存句柄
 * @param key 键
 * @param out_value 输出值
 * @return 1 表示找到值，0 表示未找到，-1 表示错误
 */
int cache_get(cache_t cache, const void* key, void** out_value);

/**
 * @brief 向缓存存入值
 * @param cache 缓存句柄
 * @param key 键
 * @param value 值
 */
void cache_put(cache_t cache, const void* key, const void* value);

/**
 * @brief 从缓存删除值
 * @param cache 缓存句柄
 * @param key 键
 */
void cache_delete(cache_t cache, const void* key);

/**
 * @brief 清空缓存
 * @param cache 缓存句柄
 */
void cache_clear(cache_t cache);

/**
 * @brief 获取缓存大小
 * @param cache 缓存句柄
 * @return 缓存大小
 */
size_t cache_get_size(cache_t cache);

/**
 * @brief 获取缓存容量
 * @param cache 缓存句柄
 * @return 缓存容量
 */
size_t cache_get_capacity(cache_t cache);

/**
 * @brief 设置缓存容量
 * @param cache 缓存句柄
 * @param capacity 新容量
 */
void cache_set_capacity(cache_t cache, size_t capacity);

/**
 * @brief 获取缓存过期时间
 * @param cache 缓存句柄
 * @return 过期时间（秒）
 */
int cache_get_ttl(cache_t cache);

/**
 * @brief 设置缓存过期时间
 * @param cache 缓存句柄
 * @param ttl_sec 过期时间（秒）
 */
void cache_set_ttl(cache_t cache, int ttl_sec);

/**
 * @brief 字符串键默认哈希函数
 * @param key 字符串键
 * @return 哈希值
 */
unsigned int cache_string_hash(const void* key);

/**
 * @brief 字符串键默认比较函数
 * @param a 字符串键1
 * @param b 字符串键2
 * @return 比较结果
 */
int cache_string_compare(const void* a, const void* b);

/**
 * @brief 字符串默认复制函数
 * @param data 字符串
 * @return 复制的字符串
 */
void* cache_string_copy(const void* data);

/**
 * @brief 字符串默认释放函数
 * @param data 字符串
 */
void cache_string_free(void* data);

/**
 * @brief 创建字符串缓存（使用默认字符串处理函数）
 * @param capacity 缓存容量
 * @param ttl_sec 过期时间（秒）
 * @return 缓存句柄
 */
cache_t cache_create_string_cache(size_t capacity, int ttl_sec);

/**
 * @brief 从字符串缓存获取值
 * @param cache 缓存句柄
 * @param key 字符串键
 * @param out_value 输出字符串值
 * @return 1 表示找到值，0 表示未找到，-1 表示错误
 */
int cache_get_string(cache_t cache, const char* key, char** out_value);

/**
 * @brief 向字符串缓存存入值
 * @param cache 缓存句柄
 * @param key 字符串键
 * @param value 字符串值
 */
void cache_put_string(cache_t cache, const char* key, const char* value);

#ifdef __cplusplus
}
#endif

#endif /* CACHE_COMMON_H */
