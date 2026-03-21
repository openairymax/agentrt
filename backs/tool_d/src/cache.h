/**
 * @file cache.h
 * @brief 工具结果缓存接口（复用 llm_d 的 cache 实现）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef TOOL_CACHE_H
#define TOOL_CACHE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tool_cache tool_cache_t;

tool_cache_t* tool_cache_create(size_t capacity, int ttl_sec);
void tool_cache_destroy(tool_cache_t* cache);
int tool_cache_get(tool_cache_t* cache, const char* key, char** out_value);
void tool_cache_put(tool_cache_t* cache, const char* key, const char* value);
char* tool_cache_key(const char* tool_id, const char* params_json);
tool_result_t* tool_result_from_json(const char* json);
char* tool_result_to_json(const tool_result_t* res);

// From data intelligence emerges. by spharx
#ifdef __cplusplus
}
#endif

#endif /* TOOL_CACHE_H */