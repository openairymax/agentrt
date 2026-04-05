/**
 * @file input_validator.c
 * @brief 输入验证框架实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "input_validator.h"
#include "svc_logger.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MAX_RULES 64

struct validation_result {
    int valid;
    char* error_message;
    char* error_field;
};

validation_result_t* validator_create(void) {
    return (validation_result_t*)calloc(1, sizeof(validation_result_t));
}

void validator_destroy(validation_result_t* v) {
    if (!v) return;
    free(v->error_message);
    free(v->error_field);
    free(v);
}

int validator_add_rule(validation_result_t* validator, const validation_rule_t* rule) {
    (void)validator;
    (void)rule;
    return 0;
}

validation_result_t* validator_validate(validation_result_t* v, const cJSON* data) {
    if (!v) return NULL;
    v->valid = 1;
    (void)data;
    return v;
}

int validate_required_field(const cJSON* obj, const char* field, char** out_error) {
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

int validate_string_field(const cJSON* obj, const char* field, size_t min_len, size_t max_len, char** out_error) {
    if (!obj || !field) {
        if (out_error) *out_error = strdup("Invalid parameters");
        return -1;
    }
    
    cJSON* item = cJSON_GetObjectItem(obj, field);
    if (!item) {
        if (out_error) *out_error = strdup("Field not found");
        return -1;
    }
    
    if (!cJSON_IsString(item)) {
        if (out_error) *out_error = strdup("Field is not a string");
        return -1;
    }
    
    size_t len = strlen(item->valuestring);
    if (min_len > 0 && len < min_len) {
        if (out_error) {
            char err[256];
            snprintf(err, sizeof(err), "Field too short (min %zu)", min_len);
            *out_error = strdup(err);
        }
        return -1;
    }
    
    if (max_len > 0 && len > max_len) {
        if (out_error) {
            char err[256];
            snprintf(err, sizeof(err), "Field too long (max %zu)", max_len);
            *out_error = strdup(err);
        }
        return -1;
    }
    
    return 0;
}
