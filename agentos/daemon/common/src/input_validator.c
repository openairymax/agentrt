/**
 * @file input_validator.c
 * @brief 输入验证框架实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * @version 1.0.0
 * @date 2026-04-04
 */

#include "input_validator.h"
#include "svc_logger.h"
#include <stdlib.h>
#include <string.h>

/* ==================== 内部实现 ==================== */

#define MAX_RULES 64

struct validation_result {
    int valid;
    char* error_message;
    char* error_field;
    int error_code;
    validation_rule_t rules[MAX_RULES];
    size_t rule_count;
};

validation_result_t* validator_create(void) {
    validation_result_t* v = (validation_result_t*)calloc(1, sizeof(validation_result_t));
    if (!v) {
        SVC_LOG_ERROR("Failed to allocate validator");
        return NULL;
    }
    
    v->valid = 1; // 默认有效
    v->rule_count = 0;
    v->error_message = NULL;
    v->error_field = NULL;
    v->error_code = 0;
    
    SVC_LOG_DEBUG("Validator created");
    return v;
}

void validator_destroy(validation_result_t* validator) {
    if (!validator) return;
    
    free(validator->error_message);
    free(validator->error_field);
    free(validator);
}

int validator_add_rule(validation_result_t* validator, 
                       const validation_rule_t* rule) {
    if (!validator || !rule) {
        return -1;
    }
    
    if (validator->rule_count >= MAX_RULES) {
        SVC_LOG_ERROR("Too many validation rules");
        return -1;
    }
    
    memcpy(&validator->rules[validator->rule_count], rule, sizeof(validation_rule_t));
    validator->rule_count++;
    
    SVC_LOG_DEBUG("Validation rule added: field=%s", 
                  rule->field_name ? rule->field_name : "(none)");
    return 0;
}

static void set_error(validation_result_t* result, 
                     const char* field,
                     const char* message,
                     int code) {
    result->valid = 0;
    result->error_code = code;
    
    free(result->error_field);
    result->error_field = strdup(field ? field : "(unknown)");
    
    free(result->error_message);
    result->error_message = strdup(message ? message : "Validation failed");
    
    SVC_LOG_DEBUG("Validation error: field=%s, msg=%s", field, message);
}

static int validate_single_rule(const cJSON* value, 
                               const validation_rule_t* rule,
                               validation_result_t* result) {
    switch (rule->type) {
        case VALIDATE_REQUIRED:
            if (!value) {
                set_error(result, rule->field_name, 
                         "Field is required", -1);
                return -1;
            }
            break;

        case VALIDATE_STRING:
            if (value && !cJSON_IsString(value)) {
                set_error(result, rule->field_name,
                         "Field must be a string", -2);
                return -1;
            }
            break;

        case VALIDATE_NUMBER:
            if (value && !cJSON_IsNumber(value)) {
                set_error(result, rule->field_name,
                         "Field must be a number", -3);
                return -1;
            }
            break;

        case VALIDATE_BOOL:
            if (value && !cJSON_IsBool(value)) {
                set_error(result, rule->field_name,
                         "Field must be a boolean", -4);
                return -1;
            }
            break;

        case VALIDATE_ARRAY:
            if (value && !cJSON_IsArray(value)) {
                set_error(result, rule->field_name,
                         "Field must be an array", -5);
                return -1;
            }
            break;

        case VALIDATE_OBJECT:
            if (value && !cJSON_IsObject(value)) {
                set_error(result, rule->field_name,
                         "Field must be an object", -6);
                return -1;
            }
            break;

        case VALIDATE_MIN_LENGTH:
            if (value && cJSON_IsString(value)) {
                size_t len = strlen(value->valuestring);
                if (len < rule->length_value) {
                    char msg[128];
                    snprintf(msg, sizeof(msg), 
                            "Field too short (min=%zu)", rule->length_value);
                    set_error(result, rule->field_name, msg, -7);
                    return -1;
                }
            }
            break;

        case VALIDATE_MAX_LENGTH:
            if (value && cJSON_IsString(value)) {
                size_t len = strlen(value->valuestring);
                if (len > rule->length_value) {
                    char msg[128];
                    snprintf(msg, sizeof(msg),
                            "Field too long (max=%zu)", rule->length_value);
                    set_error(result, rule->field_name, msg, -8);
                    return -1;
                }
            }
            break;

        default:
            SVC_LOG_WARN("Unknown validation rule type: %d", rule->type);
            break;
    }

    return 0;
}

validation_result_t* validator_validate(validation_result_t* validator,
                                       const cJSON* data) {
    if (!validator || !data) {
        if (validator) {
            set_error(validator, "(root)", "Invalid parameters", -100);
        }
        return validator;
    }

    for (size_t i = 0; i < validator->rule_count; i++) {
        const validation_rule_t* rule = &validator->rules[i];
        
        if (!rule->field_name) continue;

        cJSON* value = cJSON_GetObjectItem(data, rule->field_name);

        if (validate_single_rule(value, rule, validator) != 0) {
            return validator; // 遇到第一个错误就返回
        }
    }

    SVC_LOG_DEBUG("Validation passed (%zu rules)", validator->rule_count);
    return validator;
}

/* ==================== 便捷验证函数 ==================== */

int validate_string_field(const cJSON* obj, const char* field,
                         size_t min_len, size_t max_len,
                         char** out_error) {
    if (!obj || !field) {
        if (out_error) *out_error = strdup("Invalid parameters");
        return -1;
    }

    cJSON* item = cJSON_GetObjectItem(obj, field);
    
    if (!item) {
        if (out_error) {
            char err[256];
            snprintf(err, sizeof(err), "Missing required field: %s", field);
            *out_error = strdup(err);
        }
        return -2;
    }

    if (!cJSON_IsString(item)) {
        if (out_error) {
            char err[256];
            snprintf(err, sizeof(err), "Field '%s' must be a string", field);
            *out_error = strdup(err);
        }
        return -3;
    }

    size_t len = strlen(item->valuestring);
    
    if (min_len > 0 && len < min_len) {
        if (out_error) {
            char err[256];
            snprintf(err, sizeof(err), 
                    "Field '%s' too short (got %zu, min %zu)", 
                    field, len, min_len);
            *out_error = strdup(err);
        }
        return -4;
    }

    if (max_len > 0 && len > max_len) {
        if (out_error) {
            char err[256];
            snprintf(err, sizeof(err), 
                    "Field '%s' too long (got %zu, max %zu)", 
                    field, len, max_len);
            *out_error = strdup(err);
        }
        return -5;
    }

    return 0;
}

int validate_number_field(const cJSON* obj, const char* field,
                         double min_val, double max_val,
                         char** out_error) {
    if (!obj || !field) {
        if (out_error) *out_error = strdup("Invalid parameters");
        return -1;
    }

    cJSON* item = cJSON_GetObjectItem(obj, field);
    
    if (!item) {
        if (out_error) {
            char err[256];
            snprintf(err, sizeof(err), "Missing required field: %s", field);
            *out_error = strdup(err);
        }
        return -2;
    }

    if (!cJSON_IsNumber(item)) {
        if (out_error) {
            char err[256];
            snprintf(err, sizeof(err), "Field '%s' must be a number", field);
            *out_error = strdup(err);
        }
        return -3;
    }

    double val = item->valuedouble;
    
    if (val < min_val) {
        if (out_error) {
            char err[256];
            snprintf(err, sizeof(err), 
                    "Field '%s' below minimum (%.2f < %.2f)", 
                    field, val, min_val);
            *out_error = strdup(err);
        }
        return -4;
    }

    if (val > max_val) {
        if (out_error) {
            char err[256];
            snprintf(err, sizeof(err), 
                    "Field '%s' above maximum (%.2f > %.2f)", 
                    field, val, max_val);
            *out_error = strdup(err);
        }
        return -5;
    }

    return 0;
}

int validate_required_field(const cJSON* obj, const char* field,
                            char** out_error) {
    if (!obj || !field) {
        if (out_error) *out_error = strdup("Invalid parameters");
        return -1;
    }

    cJSON* item = cJSON_GetObjectItem(obj, field);
    
    if (!item) {
        if (out_error) {
            char err[256];
            snprintf(err, sizeof(err), "Missing required field: %s", field);
            *out_error = strdup(err);
        }
        return -1;
    }

    return 0;
}

void validation_result_free(validation_result_t* result) {
    validator_destroy(result);
}
