/**
 * @file param_validator.h
 * @brief JSON-RPC 参数验证工具（统一验证逻辑，消除重复代码）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AGENTOS_PARAM_VALIDATOR_H
#define AGENTOS_PARAM_VALIDATOR_H

#include <cjson/cJSON.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 验证 JSON 对象是否存在且类型正确
 * @param obj JSON 对象
 * @param key 字段名
 * @param expected_type 期望类型
 * @return true 如果存在且类型正确
 */
static inline bool validate_json_field(cJSON* obj, const char* key, int expected_type) {
    cJSON* field = cJSON_GetObjectItem(obj, key);
    return (field != NULL && field->type == expected_type);
}

/**
 * @brief 获取字符串字段（带默认值）
 * @param obj JSON 对象
 * @param key 字段名
 * @param default_value 默认值
 * @return 字符串值或默认值
 */
static inline const char* get_string_field(cJSON* obj, const char* key, const char* default_value) {
    cJSON* field = cJSON_GetObjectItem(obj, key);
    return (cJSON_IsString(field) && field->valuestring) ? field->valuestring : default_value;
}

/**
 * @brief 获取整数字段（带默认值）
 * @param obj JSON 对象
 * @param key 字段名
 * @param default_value 默认值
 * @return 整数值或默认值
 */
static inline int get_int_field(cJSON* obj, const char* key, int default_value) {
    cJSON* field = cJSON_GetObjectItem(obj, key);
    return (cJSON_IsNumber(field)) ? field->valueint : default_value;
}

/**
 * @brief 获取双精度浮点字段（带默认值）
 * @param obj JSON 对象
 * @param key 字段名
 * @param default_value 默认值
 * @return 浮点值或默认值
 */
static inline double get_double_field(cJSON* obj, const char* key, double default_value) {
    cJSON* field = cJSON_GetObjectItem(obj, key);
    return (cJSON_IsNumber(field)) ? field->valuedouble : default_value;
}

/**
 * @brief 获取布尔字段（带默认值）
 * @param obj JSON 对象
 * @param key 字段名
 * @param default_value 默认值
 * @return 布尔值或默认值
 */
static inline bool get_bool_field(cJSON* obj, const char* key, bool default_value) {
    cJSON* field = cJSON_GetObjectItem(obj, key);
    if (field == NULL) return default_value;
    if (field->type == cJSON_True) return true;
    if (field->type == cJSON_False) return false;
    return default_value;
}

/**
 * @brief 验证必需字段是否存在
 * @param obj JSON 对象
 * @param ... 必需字段名列表（以 NULL 结尾）
 * @return 0 成功，-1 失败
 */
int validate_required_fields(cJSON* obj, ...);

/**
 * @brief 验证请求 ID 并返回整数值
 * @param id JSON 请求 ID 字段
 * @return 请求 ID 整数值
 */
static inline int get_request_id(cJSON* id) {
    return cJSON_IsNumber(id) ? id->valueint : 0;
}

/**
 * @brief 验证 JSON-RPC 请求的基本结构
 * @param req JSON 请求对象
 * @param jsonrpc jsonrpc 字段
 * @param method method 字段
 * @param params params 字段
 * @param id id 字段
 * @return 0 成功，-1 失败
 */
int validate_jsonrpc_request(cJSON* req, 
                             cJSON** jsonrpc, 
                             cJSON** method, 
                             cJSON** params, 
                             cJSON** id);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_PARAM_VALIDATOR_H */
