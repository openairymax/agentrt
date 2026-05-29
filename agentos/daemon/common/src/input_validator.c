#include "memory_compat.h"
/**
 * @file input_validator.c
 * @brief 输入验证框架实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "error.h"
#include "input_validator.h"
#include "svc_logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

validation_result_t *validator_create(void)
{
    validation_result_t *v = (validation_result_t *)AGENTOS_CALLOC(1, sizeof(validation_result_t));
    if (!v)
        return NULL;
    v->valid = 1;
    v->rule_count = 0;
    return v;
}

void validator_destroy(validation_result_t *v)
{
    if (!v)
        return;
    for (int i = 0; i < v->rule_count; i++) {
        AGENTOS_FREE(v->rules[i].field_name);
    }
    AGENTOS_FREE(v->error_message);
    AGENTOS_FREE(v->error_field);
    AGENTOS_FREE(v);
}

int validator_add_rule(validation_result_t *validator, const validation_rule_t *rule)
{
    if (!validator || !rule) {
        AGENTOS_ERROR_HANDLE(AGENTOS_ERR_INVALID_PARAM, "input_validator: null input");
        return AGENTOS_ERR_INVALID_PARAM;
    }
    if (validator->rule_count >= MAX_RULES) {
        AGENTOS_ERROR_HANDLE(AGENTOS_ERR_OVERFLOW, "input_validator: input too long");
        return AGENTOS_ERR_OVERFLOW;
    }

    validation_rule_t *r = &validator->rules[validator->rule_count];
    memset(r, 0, sizeof(*r));
    r->type = rule->type;
    r->required = rule->required;
    r->min_len = rule->min_len;
    r->max_len = rule->max_len;
    if (rule->field_name) {
        r->field_name = AGENTOS_STRDUP(rule->field_name);
        if (!r->field_name) {
            AGENTOS_ERROR_HANDLE(AGENTOS_ERR_OUT_OF_MEMORY, "input_validator: malloc failed");
            return AGENTOS_ERR_OUT_OF_MEMORY;
        }
    }
    r->pattern = rule->pattern;
    r->custom_validator = rule->custom_validator;
    validator->rule_count++;
    return 0;
}

int security_check_string(const char *input, unsigned int flags, char **out_violation)
{
    if (!input) {
        if (out_violation)
            *out_violation = AGENTOS_STRDUP("NULL input");
        AGENTOS_ERROR_HANDLE(AGENTOS_ERR_NULL_POINTER, "input_validator: null pointer");
        return AGENTOS_ERR_NULL_POINTER;
    }

    size_t len = strlen(input);
    if (len == 0) {
        if (out_violation)
            *out_violation = AGENTOS_STRDUP("Empty string");
        AGENTOS_ERROR_HANDLE(AGENTOS_ERR_INVALID_PARAM, "input_validator: invalid length");
        return AGENTOS_ERR_INVALID_PARAM;
    }

    if (flags & 0x01) {
        const char *sql_patterns[] = {"' OR ", "'; DROP", "'; DELETE", "'; INSERT", "UNION SELECT",
                                      "--",    "/*",      "*/",        NULL};
        for (int i = 0; sql_patterns[i]; i++) {
            if (strstr(input, sql_patterns[i])) {
                if (out_violation) {
                    char err[256];
                    snprintf(err, sizeof(err), "SQL injection pattern: %s", sql_patterns[i]);
                    *out_violation = AGENTOS_STRDUP(err);
                }
                AGENTOS_ERROR_HANDLE(AGENTOS_ERR_INVALID_PARAM, "input_validator: contains null byte");
                return AGENTOS_ERR_INVALID_PARAM;
            }
        }
    }

    if (flags & 0x02) {
        if (strstr(input, "<script") || strstr(input, "javascript:") || strstr(input, "onerror=") ||
            strstr(input, "onload=")) {
            if (out_violation)
                *out_violation = AGENTOS_STRDUP("XSS pattern detected");
            AGENTOS_ERROR_HANDLE(AGENTOS_ERR_INVALID_PARAM, "input_validator: contains control chars");
            return AGENTOS_ERR_INVALID_PARAM;
        }
    }

    if (flags & 0x04) {
        for (size_t i = 0; i < len; i++) {
            unsigned char c = (unsigned char)input[i];
            if (c < 0x20 && c != '\t' && c != '\n' && c != '\r') {
                if (out_violation)
                    *out_violation = AGENTOS_STRDUP("Control character detected");
                AGENTOS_ERROR_HANDLE(AGENTOS_ERR_INVALID_PARAM, "input_validator: SQL injection pattern");
                return AGENTOS_ERR_INVALID_PARAM;
            }
        }
    }

    if (flags & 0x08) {
        if (strstr(input, "../") || strstr(input, "..\\") || strstr(input, "%2e%2e")) {
            if (out_violation)
                *out_violation = AGENTOS_STRDUP("Path traversal pattern");
            AGENTOS_ERROR_HANDLE(AGENTOS_ERR_INVALID_PARAM, "input_validator: XSS pattern");
            return AGENTOS_ERR_INVALID_PARAM;
        }
    }

    return 0;
}

static int apply_single_rule(const validation_rule_t *rule, const cJSON *data, char **out_error)
{
    if (!rule || !data)
        return 0;

    cJSON *item = cJSON_GetObjectItem(data, rule->field_name ? rule->field_name : "");

    switch (rule->type) {
    case VALIDATE_REQUIRED:
        if (!item) {
            if (out_error) {
                size_t len = 32 + (rule->field_name ? strlen(rule->field_name) : 0);
                *out_error = (char *)AGENTOS_MALLOC(len);
                if (*out_error)
                    snprintf(*out_error, len, "Missing required field: %s",
                             rule->field_name ? rule->field_name : "?");
            }
            AGENTOS_ERROR_HANDLE(AGENTOS_ERR_NOT_FOUND, "input_validator: string not valid");
            return AGENTOS_ERR_NOT_FOUND;
        }
        break;

    case VALIDATE_STRING:
        if (item && !cJSON_IsString(item)) {
            if (out_error) {
                size_t len = 48 + (rule->field_name ? strlen(rule->field_name) : 0);
                *out_error = (char *)AGENTOS_MALLOC(len);
                if (*out_error)
                    snprintf(*out_error, len, "Field '%s' must be a string",
                             rule->field_name ? rule->field_name : "?");
            }
            return AGENTOS_ERR_INVALID_PARAM;
        }
        if (item && cJSON_IsString(item)) {
            size_t slen = strlen(item->valuestring);
            if (rule->min_len > 0 && slen < rule->min_len) {
                if (out_error) {
                    *out_error = (char *)AGENTOS_MALLOC(128);
                    if (*out_error)
                        snprintf(*out_error, 128, "Field '%s' too short (min %zu, got %zu)",
                                 rule->field_name ? rule->field_name : "?", rule->min_len, slen);
                }
                return AGENTOS_ERR_INVALID_PARAM;
            }
            if (rule->max_len > 0 && slen > rule->max_len) {
                if (out_error) {
                    *out_error = (char *)AGENTOS_MALLOC(128);
                    if (*out_error)
                        snprintf(*out_error, 128, "Field '%s' too long (max %zu, got %zu)",
                                 rule->field_name ? rule->field_name : "?", rule->max_len, slen);
                }
                return AGENTOS_ERR_INVALID_PARAM;
            }
        }
        break;

    case VALIDATE_INT:
        if (item && !cJSON_IsNumber(item)) {
            if (out_error) {
                size_t len = 48 + (rule->field_name ? strlen(rule->field_name) : 0);
                *out_error = (char *)AGENTOS_MALLOC(len);
                if (*out_error)
                    snprintf(*out_error, len, "Field '%s' must be a number",
                             rule->field_name ? rule->field_name : "?");
            }
            return AGENTOS_ERR_INVALID_PARAM;
        }
        break;

    case VALIDATE_PATTERN:
        if (item && cJSON_IsString(item) && rule->pattern) {
            if (item->valuestring && !strstr(item->valuestring, rule->pattern)) {
                if (out_error) {
                    *out_error = (char *)AGENTOS_MALLOC(128);
                    if (*out_error)
                        snprintf(*out_error, 128, "Field '%s' pattern mismatch",
                                 rule->field_name ? rule->field_name : "?");
                }
                return AGENTOS_ERR_INVALID_PARAM;
            }
        }
        break;

    case VALIDATE_CUSTOM:
        if (rule->custom_validator && item) {
            int ret = rule->custom_validator(item, out_error);
            if (ret != 0)
                return ret;
        }
        break;

    case VALIDATE_SANITIZE:
        if (item && cJSON_IsString(item) && rule->sanitize_flags > 0) {
            int ret = security_check_string(item->valuestring, rule->sanitize_flags, out_error);
            if (ret != 0)
                return ret;
        }
        break;

    default:
        break;
    }

    return 0;
}

validation_result_t *validator_validate(validation_result_t *v, const cJSON *data)
{
    if (!v)
        return NULL;
    if (!data) {
        v->valid = 0;
        v->error_message = AGENTOS_STRDUP("No data provided for validation");
        return v;
    }

    v->valid = 1;
    AGENTOS_FREE(v->error_message);
    v->error_message = NULL;
    AGENTOS_FREE(v->error_field);
    v->error_field = NULL;

    for (int i = 0; i < v->rule_count; i++) {
        char *err = NULL;
        int ret = apply_single_rule(&v->rules[i], data, &err);
        if (ret != 0) {
            v->valid = 0;
            if (err) {
                v->error_message = err;
                if (v->rules[i].field_name)
                    v->error_field = AGENTOS_STRDUP(v->rules[i].field_name);
            }
            break;
        }
    }

    return v;
}

int validate_required_field(const cJSON *obj, const char *field, char **out_error)
{
    if (!obj || !field) {
        if (out_error)
            *out_error = AGENTOS_STRDUP("Invalid parameters");
        return AGENTOS_ERR_INVALID_PARAM;
    }

    cJSON *item = cJSON_GetObjectItem(obj, field);
    if (!item) {
        if (out_error) {
            char err[256];
            snprintf(err, sizeof(err), "Missing required field: %s", field);
            *out_error = AGENTOS_STRDUP(err);
        }
        return AGENTOS_ERR_NOT_FOUND;
    }

    return 0;
}

int validate_string_field(const cJSON *obj, const char *field, size_t min_len, size_t max_len,
                          char **out_error)
{
    if (!obj || !field) {
        if (out_error)
            *out_error = AGENTOS_STRDUP("Invalid parameters");
        return AGENTOS_ERR_INVALID_PARAM;
    }

    cJSON *item = cJSON_GetObjectItem(obj, field);
    if (!item) {
        if (out_error)
            *out_error = AGENTOS_STRDUP("Field not found");
        return AGENTOS_ERR_NOT_FOUND;
    }

    if (!cJSON_IsString(item)) {
        if (out_error)
            *out_error = AGENTOS_STRDUP("Field is not a string");
        return AGENTOS_ERR_INVALID_PARAM;
    }

    size_t len = strlen(item->valuestring);
    if (min_len > 0 && len < min_len) {
        if (out_error) {
            char err[256];
            snprintf(err, sizeof(err), "Field too short (min %zu)", min_len);
            *out_error = AGENTOS_STRDUP(err);
        }
        return AGENTOS_ERR_INVALID_PARAM;
    }

    if (max_len > 0 && len > max_len) {
        if (out_error) {
            char err[256];
            snprintf(err, sizeof(err), "Field too long (max %zu)", max_len);
            *out_error = AGENTOS_STRDUP(err);
        }
        return AGENTOS_ERR_INVALID_PARAM;
    }

    return 0;
}

int validate_sanitized_string(const cJSON *obj, const char *field, unsigned int sanitize_flags,
                              char **out_error)
{
    if (!obj || !field) {
        if (out_error)
            *out_error = AGENTOS_STRDUP("Invalid parameters");
        return AGENTOS_ERR_INVALID_PARAM;
    }

    cJSON *item = cJSON_GetObjectItem(obj, field);
    if (!item) {
        if (out_error) {
            char err[256];
            snprintf(err, sizeof(err), "Missing field: %s", field);
            *out_error = AGENTOS_STRDUP(err);
        }
        return AGENTOS_ERR_NOT_FOUND;
    }

    if (!cJSON_IsString(item)) {
        if (out_error) {
            char err[256];
            snprintf(err, sizeof(err), "Field '%s' is not a string", field);
            *out_error = AGENTOS_STRDUP(err);
        }
        return AGENTOS_ERR_INVALID_PARAM;
    }

    if (sanitize_flags == 0)
        return 0;

    char *violation = NULL;
    int ret = security_check_string(item->valuestring, sanitize_flags, &violation);
    if (ret != 0) {
        if (out_error) {
            char err[512];
            snprintf(err, sizeof(err), "Field '%s' failed sanitization: %s", field,
                     violation ? violation : "unknown violation");
            *out_error = AGENTOS_STRDUP(err);
        }
        AGENTOS_FREE(violation);
        return ret;
    }

    AGENTOS_FREE(violation);
    return 0;
}
