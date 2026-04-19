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
    validation_rule_t rules[MAX_RULES];
    int rule_count;
};

validation_result_t* validator_create(void) {
    validation_result_t* v = (validation_result_t*)calloc(1, sizeof(validation_result_t));
    if (!v) return NULL;
    v->valid = 1;
    v->rule_count = 0;
    return v;
}

void validator_destroy(validation_result_t* v) {
    if (!v) return;
    for (int i = 0; i < v->rule_count; i++) {
        free(v->rules[i].field_name);
    }
    free(v->error_message);
    free(v->error_field);
    free(v);
}

int validator_add_rule(validation_result_t* validator, const validation_rule_t* rule) {
    if (!validator || !rule) return -1;
    if (validator->rule_count >= MAX_RULES) return -1;

    validation_rule_t* r = &validator->rules[validator->rule_count];
    memset(r, 0, sizeof(*r));
    r->type = rule->type;
    r->required = rule->required;
    r->min_len = rule->min_len;
    r->max_len = rule->max_len;
    if (rule->field_name) {
        r->field_name = strdup(rule->field_name);
        if (!r->field_name) return -1;
    }
    r->pattern = rule->pattern;
    r->custom_validator = rule->custom_validator;
    validator->rule_count++;
    return 0;
}

static int apply_single_rule(const validation_rule_t* rule, const cJSON* data,
                              char** out_error) {
    if (!rule || !data) return 0;

    cJSON* item = cJSON_GetObjectItem(data, rule->field_name ? rule->field_name : "");

    switch (rule->type) {
    case VALIDATE_REQUIRED:
        if (!item) {
            if (out_error) {
                size_t len = 32 + (rule->field_name ? strlen(rule->field_name) : 0);
                *out_error = (char*)malloc(len);
                if (*out_error)
                    snprintf(*out_error, len, "Missing required field: %s",
                             rule->field_name ? rule->field_name : "?");
            }
            return -1;
        }
        break;

    case VALIDATE_STRING:
        if (item && !cJSON_IsString(item)) {
            if (out_error) {
                size_t len = 48 + (rule->field_name ? strlen(rule->field_name) : 0);
                *out_error = (char*)malloc(len);
                if (*out_error)
                    snprintf(*out_error, len, "Field '%s' must be a string",
                             rule->field_name ? rule->field_name : "?");
            }
            return -1;
        }
        if (item && cJSON_IsString(item)) {
            size_t slen = strlen(item->valuestring);
            if (rule->min_len > 0 && slen < rule->min_len) {
                if (out_error) {
                    *out_error = (char*)malloc(128);
                    if (*out_error)
                        snprintf(*out_error, 128,
                                 "Field '%s' too short (min %zu, got %zu)",
                                 rule->field_name ? rule->field_name : "?",
                                 rule->min_len, slen);
                }
                return -1;
            }
            if (rule->max_len > 0 && slen > rule->max_len) {
                if (out_error) {
                    *out_error = (char*)malloc(128);
                    if (*out_error)
                        snprintf(*out_error, 128,
                                 "Field '%s' too long (max %zu, got %zu)",
                                 rule->field_name ? rule->field_name : "?",
                                 rule->max_len, slen);
                }
                return -1;
            }
        }
        break;

    case VALIDATE_INT:
        if (item && !cJSON_IsNumber(item)) {
            if (out_error) {
                size_t len = 48 + (rule->field_name ? strlen(rule->field_name) : 0);
                *out_error = (char*)malloc(len);
                if (*out_error)
                    snprintf(*out_error, len, "Field '%s' must be a number",
                             rule->field_name ? rule->field_name : "?");
            }
            return -1;
        }
        break;

    case VALIDATE_PATTERN:
        if (item && cJSON_IsString(item) && rule->pattern) {
            if (item->valuestring && !strstr(item->valuestring, rule->pattern)) {
                if (out_error) {
                    *out_error = (char*)malloc(128);
                    if (*out_error)
                        snprintf(*out_error, 128,
                                 "Field '%s' pattern mismatch",
                                 rule->field_name ? rule->field_name : "?");
                }
                return -1;
            }
        }
        break;

    case VALIDATE_CUSTOM:
        if (rule->custom_validator && item) {
            int ret = rule->custom_validator(item, out_error);
            if (ret != 0) return ret;
        }
        break;

    default:
        break;
    }

    return 0;
}

validation_result_t* validator_validate(validation_result_t* v, const cJSON* data) {
    if (!v) return NULL;
    if (!data) {
        v->valid = 0;
        v->error_message = strdup("No data provided for validation");
        return v;
    }

    v->valid = 1;
    free(v->error_message);
    v->error_message = NULL;
    free(v->error_field);
    v->error_field = NULL;

    for (int i = 0; i < v->rule_count; i++) {
        char* err = NULL;
        int ret = apply_single_rule(&v->rules[i], data, &err);
        if (ret != 0) {
            v->valid = 0;
            if (err) {
                v->error_message = err;
                if (v->rules[i].field_name)
                    v->error_field = strdup(v->rules[i].field_name);
            }
            break;
        }
    }

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
