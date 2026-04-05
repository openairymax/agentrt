/**
 * @file input_validator.h
 * @brief 输入验证框架
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * @version 1.0.0
 * @date 2026-04-04
 *
 * 设计说明：
 * - 提供统一的输入验证接口
 * - 防止注入攻击和无效输入
 * - 支持自定义验证规则
 */

#ifndef AGENTOS_INPUT_VALIDATOR_H
#define AGENTOS_INPUT_VALIDATOR_H

#include <cjson/cJSON.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 验证结果 ==================== */

typedef struct {
    int valid;
    char* error_message;
    char* error_field;
    int error_code;
} validation_result_t;

/* ==================== 验证规则类型 ==================== */

typedef enum {
    VALIDATE_REQUIRED,      // 必填字段
    VALIDATE_STRING,        // 字符串类型
    VALIDATE_NUMBER,        // 数字类型
    VALIDATE_BOOL,          // 布尔类型
    VALIDATE_ARRAY,         // 数组类型
    VALIDATE_OBJECT,        // 对象类型
    VALIDATE_MIN_LENGTH,    // 最小长度
    VALIDATE_MAX_LENGTH,    // 最大长度
    VALIDATE_MIN_VALUE,     // 最小值
    VALIDATE_MAX_VALUE,     // 最大值
    VALIDATE_PATTERN,       // 正则表达式模式（简化版）
    VALIDATE_ENUM,          // 枚举值
    VALIDATE_CUSTOM         // 自定义验证函数
} validation_rule_type_t;

/* ==================== 验证规则定义 ==================== */

typedef struct validation_rule {
    validation_rule_type_t type;
    const char* field_name;
    
    union {
        size_t length_value;      // 用于MIN_LENGTH/MAX_LENGTH
        double number_value;      // 用于MIN_VALUE/MAX_VALUE
        const char** enum_values; // 用于ENUM
        bool (*custom_fn)(const cJSON* value); // 自定义验证
    };
    
    const char* pattern;          // 用于PATTERN
    const char* message;          // 自定义错误消息
    
} validation_rule_t;

/* ==================== 验证器接口 ==================== */

/**
 * @brief 创建验证器实例
 */
validation_result_t* validator_create(void);

/**
 * @brief 销毁验证器实例
 */
void validator_destroy(validation_result_t* validator);

/**
 * @brief 添加验证规则
 * @param validator 验证器实例
 * @param rule 规则
 * @return 0成功，非0失败
 */
int validator_add_rule(validation_result_t* validator, 
                       const validation_rule_t* rule);

/**
 * @brief 验证JSON对象
 * @param validator 验证器实例
 * @param data 待验证的JSON对象
 * @return 验证结果（调用者负责validator_destroy）
 */
validation_result_t* validator_validate(validation_result_t* validator,
                                       const cJSON* data);

/* ==================== 便捷验证函数 ==================== */

/**
 * @brief 验证字符串字段
 */
int validate_string_field(const cJSON* obj, const char* field,
                         size_t min_len, size_t max_len,
                         char** out_error);

/**
 * @brief 验证数字字段
 */
int validate_number_field(const cJSON* obj, const char* field,
                         double min_val, double max_val,
                         char** out_error);

/**
 * @brief 验证必填字段是否存在
 */
int validate_required_field(const cJSON* obj, const char* field,
                            char** out_error);

/**
 * @brief 清理验证结果
 */
void validation_result_free(validation_result_t* result);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_INPUT_VALIDATOR_H */
