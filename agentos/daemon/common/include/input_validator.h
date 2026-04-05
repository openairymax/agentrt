/**
 * @file input_validator.h
 * @brief 输入验证框架
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_INPUT_VALIDATOR_H
#define AGENTOS_INPUT_VALIDATOR_H

#include <cjson/cJSON.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct validation_result validation_result_t;

typedef enum {
    VALIDATE_REQUIRED,
    VALIDATE_STRING,
    VALIDATE_NUMBER,
    VALIDATE_MIN_LENGTH,
    VALIDATE_MAX_LENGTH,
    VALIDATE_MIN_VALUE,
    VALIDATE_MAX_VALUE
} validation_rule_type_t;

typedef struct {
    validation_rule_type_t type;
    const char* field_name;
    union {
        size_t length_value;
        double number_value;
    };
} validation_rule_t;

validation_result_t* validator_create(void);
void validator_destroy(validation_result_t* validator);
int validator_add_rule(validation_result_t* validator, const validation_rule_t* rule);
validation_result_t* validator_validate(validation_result_t* validator, const cJSON* data);
int validate_required_field(const cJSON* obj, const char* field, char** out_error);
int validate_string_field(const cJSON* obj, const char* field, size_t min_len, size_t max_len, char** out_error);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_INPUT_VALIDATOR_H */
