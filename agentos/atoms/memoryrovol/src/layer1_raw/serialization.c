/**
 * @file serialization.c
 * @brief L1 记忆记录序列化与反序列化（JSON/二进制）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "layer1_raw.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../../../agentos/commons/utils/memory/include/memory_compat.h"
#include "../../../agentos/commons/utils/string/include/string_compat.h"
#include <string.h>
#include <stdio.h>

/* JSON解析库 - 条件编译 */
#ifdef AGENTOS_HAS_CJSON
#include <cjson/cJSON.h>
#else
typedef struct cJSON { int type; char* valuestring; double valuedouble; struct cJSON* child; struct cJSON* next; } cJSON;
#define cJSON_NULL 0 cJSON_False 1 cJSON_True 2 cJSON_Number 3 cJSON_String 4 cJSON_Array 5 cJSON_Object 6
static inline cJSON* cJSON_CreateObject(void) { return NULL; }
static inline cJSON* cJSON_CreateArray(void) { return NULL; }
static inline void cJSON_Delete(cJSON* item) { (void)item; }
static inline cJSON* cJSON_Parse(const char* s) { (void)s; return NULL; }
static inline cJSON* cJSON_GetObjectItem(const cJSON* o, const char* k) { (void)o;(void)k; return NULL; }
static inline void cJSON_AddItemToObject(cJSON* o, const char* k, cJSON* i) { (void)o;(void)k;(void)i; }
static inline void cJSON_AddStringToObject(cJSON* o, const char* k, const char* v) { (void)o;(void)k;(void)v; }
static inline void cJSON_AddNumberToObject(cJSON* o, const char* k, double v) { (void)o;(void)k;(void)v; }
static inline char* cJSON_PrintUnformatted(const cJSON* i) { (void)i; return NULL; }
#endif /* AGENTOS_HAS_CJSON */

/**
 * @brief 将元数据对象序列化为 JSON 字符�?
 * @param meta 元数据指�?
 * @return JSON 字符串（需调用�?free�?
 */
char* agentos_raw_metadata_to_json(const agentos_raw_metadata_t* meta) {
    if (!meta) return NULL;

    cJSON* root = cJSON_CreateObject();
    if (!root) return NULL;

    cJSON_AddStringToObject(root, "record_id", meta->record_id ? meta->record_id : "");
    cJSON_AddNumberToObject(root, "timestamp", (double)meta->timestamp);
    cJSON_AddNumberToObject(root, "data_len", meta->data_len);
    cJSON_AddNumberToObject(root, "access_count", meta->access_count);
    cJSON_AddNumberToObject(root, "last_access", (double)meta->last_access);
    if (meta->source) cJSON_AddStringToObject(root, "source", meta->source);
    if (meta->trace_id) cJSON_AddStringToObject(root, "trace_id", meta->trace_id);
    if (meta->tags_json) {
        // 尝试解析 tags_json 为对象，否则存为字符�?
        cJSON* tags = cJSON_Parse(meta->tags_json);
        if (tags) {
            cJSON_AddItemToObject(root, "tags", tags);
        } else {
            cJSON_AddStringToObject(root, "tags", meta->tags_json);
        }
    }

    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}

/**
 * @brief �?JSON 字符串解析元数据对象
 * @param json JSON 字符�?
 * @return 元数据对象（需调用 agentos_layer1_raw_metadata_free 释放�?
 */
agentos_raw_metadata_t* agentos_raw_metadata_from_json(const char* json) {
    if (!json) return NULL;

    cJSON* root = cJSON_Parse(json);
    if (!root) return NULL;

    agentos_raw_metadata_t* meta = (agentos_raw_metadata_t*)AGENTOS_MALLOC(sizeof(agentos_raw_metadata_t));
    if (!meta) {
        cJSON_Delete(root);
        return NULL;
    }
    memset(meta, 0, sizeof(agentos_raw_metadata_t));

    cJSON* item;
    item = cJSON_GetObjectItem(root, "record_id");
    if (item && cJSON_IsString(item)) meta->record_id = AGENTOS_STRDUP(item->valuestring);

    item = cJSON_GetObjectItem(root, "timestamp");
    if (item && cJSON_IsNumber(item)) meta->timestamp = (uint64_t)item->valuedouble;

    item = cJSON_GetObjectItem(root, "data_len");
    if (item && cJSON_IsNumber(item)) meta->data_len = (uint32_t)item->valueint;

    item = cJSON_GetObjectItem(root, "access_count");
    if (item && cJSON_IsNumber(item)) meta->access_count = (uint32_t)item->valueint;

    item = cJSON_GetObjectItem(root, "last_access");
    if (item && cJSON_IsNumber(item)) meta->last_access = (uint64_t)item->valuedouble;

    item = cJSON_GetObjectItem(root, "source");
    if (item && cJSON_IsString(item)) meta->source = AGENTOS_STRDUP(item->valuestring);

    item = cJSON_GetObjectItem(root, "trace_id");
    if (item && cJSON_IsString(item)) meta->trace_id = AGENTOS_STRDUP(item->valuestring);

    item = cJSON_GetObjectItem(root, "tags");
    if (item) {
        char* tags_str = cJSON_PrintUnformatted(item);
        if (tags_str) {
            meta->tags_json = tags_str;
        }
    }

    cJSON_Delete(root);
    return meta;
}

/**
 * @brief 将记忆记录序列化为字节流（可用于网络传输�?
 * @param record 记忆记录
 * @param out_bytes 输出字节流（需调用者释放）
 * @param out_len 输出长度
 * @return agentos_error_t
 */
agentos_error_t agentos_layer1_raw_serialize(
    const agentos_raw_metadata_t* meta,
    const void* data,
    size_t data_len,
    void** out_bytes,
    size_t* out_len) {

    if (!meta || !data || data_len == 0 || !out_bytes || !out_len) return AGENTOS_EINVAL;

    // 格式：|meta_json_len(4)|meta_json|data|
    char* meta_json = agentos_raw_metadata_to_json(meta);
    if (!meta_json) return AGENTOS_ENOMEM;

    size_t meta_len = strlen(meta_json);
    uint32_t meta_len_be = meta_len;  // 网络字节序？暂不处理

    size_t total_len = sizeof(uint32_t) + meta_len + data_len;
    void* buf = AGENTOS_MALLOC(total_len);
    if (!buf) {
        AGENTOS_FREE(meta_json);
        return AGENTOS_ENOMEM;
    }

    memcpy(buf, &meta_len_be, sizeof(uint32_t));
    memcpy((char*)buf + sizeof(uint32_t), meta_json, meta_len);
    memcpy((char*)buf + sizeof(uint32_t) + meta_len, data, data_len);

    AGENTOS_FREE(meta_json);
    *out_bytes = buf;
    *out_len = total_len;
    return AGENTOS_SUCCESS;
}

/**
 * @brief 反序列化字节流为记忆记录
 * @param bytes 字节�?
 * @param len 长度
 * @param out_meta 输出元数据（需调用者释放）
 * @param out_data 输出数据（需调用者释放）
 * @param out_data_len 输出数据长度
 * @return agentos_error_t
 */
agentos_error_t agentos_layer1_raw_deserialize(
    const void* bytes,
    size_t len,
    agentos_raw_metadata_t** out_meta,
    void** out_data,
    size_t* out_data_len) {

    if (!bytes || len < sizeof(uint32_t) || !out_meta || !out_data || !out_data_len) return AGENTOS_EINVAL;

    uint32_t meta_len;
    memcpy(&meta_len, bytes, sizeof(uint32_t));
    if (sizeof(uint32_t) + meta_len > len) return AGENTOS_EINVAL;

    const char* meta_json = (const char*)bytes + sizeof(uint32_t);
    char* meta_json_copy = (char*)AGENTOS_MALLOC(meta_len + 1);
    if (!meta_json_copy) return AGENTOS_ENOMEM;
    memcpy(meta_json_copy, meta_json, meta_len);
    meta_json_copy[meta_len] = '\0';

    agentos_raw_metadata_t* meta = agentos_raw_metadata_from_json(meta_json_copy);
    AGENTOS_FREE(meta_json_copy);
    if (!meta) return AGENTOS_EINVAL;

    size_t data_len = len - sizeof(uint32_t) - meta_len;
    void* data = AGENTOS_MALLOC(data_len);
    if (!data) {
        agentos_layer1_raw_metadata_free(meta);
        return AGENTOS_ENOMEM;
    }
    memcpy(data, (const char*)bytes + sizeof(uint32_t) + meta_len, data_len);

    *out_meta = meta;
    *out_data = data;
    *out_data_len = data_len;
    return AGENTOS_SUCCESS;
}
