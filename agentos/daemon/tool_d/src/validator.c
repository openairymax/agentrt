/**
 * @file validator.c
 * @brief 工具参数验证器实现（基于 cJSON Schema 验证）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "validator.h"
#include "svc_logger.h"
#include <cjson/cJSON.h>
#include <string.h>
#include <stdlib.h>

#define MAX_STRING_LEN  8192
#define MAX_ARRAY_LEN   256
#define MIN_NUMBER_VAL  -1e18
#define MAX_NUMBER_VAL  1e18

struct tool_validator {
    int strict_mode;
};

tool_validator_t* tool_validator_create(void) {
    tool_validator_t* val = calloc(1, sizeof(tool_validator_t));
    if (val) val->strict_mode = 1;
    return val;
}

void tool_validator_destroy(tool_validator_t* val) {
    free(val);
}

static int validate_type_mismatch(const char* param_name,
                                   const char* expected,
                                   const char* actual) {
    SVC_LOG_WARN("Parameter '%s': expected type '%s', got '%s'",
                 param_name ? param_name : "?",
                 expected ? expected : "?",
                 actual ? actual : "?");
    return 0;
}

static int validate_single_param(const char* param_name,
                                  const char* schema_str,
                                  const cJSON* value) {
    if (!schema_str || !value) return 1;

    cJSON* schema = cJSON_Parse(schema_str);
    if (!schema) return 1;

    const char* type_str = NULL;
    cJSON* type_item = cJSON_GetObjectItem(schema, "type");
    if (cJSON_IsString(type_item)) type_str = type_item->valuestring;

    int valid = 1;

    if (type_str) {
        if (strcmp(type_str, "string") == 0) {
            if (!cJSON_IsString(value)) {
                valid = validate_type_mismatch(param_name, "string",
                    cJSON_IsNumber(value) ? "number" :
                    cJSON_IsBool(value) ? "boolean" :
                    cJSON_IsArray(value) ? "array" :
                    cJSON_IsObject(value) ? "object" : "null");
                goto cleanup;
            }
            cJSON* min_len = cJSON_GetObjectItem(schema, "minLength");
            cJSON* max_len = cJSON_GetObjectItem(schema, "maxLength");
            size_t vlen = strlen(value->valuestring);
            if (cJSON_IsNumber(min_len) && vlen < (size_t)min_len->valuedouble) {
                SVC_LOG_WARN("Parameter '%s': string length %zu < minLength %.0f",
                             param_name, vlen, min_len->valuedouble);
                valid = 0; goto cleanup;
            }
            if (cJSON_IsNumber(max_len) && vlen > (size_t)max_len->valuedouble) {
                SVC_LOG_WARN("Parameter '%s': string length %zu > maxLength %.0f",
                             param_name, vlen, max_len->valuedouble);
                valid = 0; goto cleanup;
            }
            if (vlen > MAX_STRING_LEN) {
                SVC_LOG_WARN("Parameter '%s': string exceeds max length %d",
                             param_name, MAX_STRING_LEN);
                valid = 0; goto cleanup;
            }

        } else if (strcmp(type_str, "number") == 0 ||
                   strcmp(type_str, "integer") == 0) {
            if (!cJSON_IsNumber(value)) {
                valid = validate_type_mismatch(param_name, type_str,
                    cJSON_IsString(value) ? "string" : "non-numeric");
                goto cleanup;
            }
            if (strcmp(type_str, "integer") == 0 &&
                value->valuedouble != (double)(int64_t)value->valuedouble) {
                SVC_LOG_WARN("Parameter '%s': not an integer value", param_name);
                valid = 0; goto cleanup;
            }
            cJSON* min_val = cJSON_GetObjectItem(schema, "minimum");
            cJSON* max_val = cJSON_GetObjectItem(schema, "maximum");
            if (cJSON_IsNumber(min_val) && value->valuedouble < min_val->valuedouble) {
                SVC_LOG_WARN("Parameter '%s': value %g < minimum %g",
                             param_name, value->valuedouble, min_val->valuedouble);
                valid = 0; goto cleanup;
            }
            if (cJSON_IsNumber(max_val) && value->valuedouble > max_val->valuedouble) {
                SVC_LOG_WARN("Parameter '%s': value %g > maximum %g",
                             param_name, value->valuedouble, max_val->valuedouble);
                valid = 0; goto cleanup;
            }

        } else if (strcmp(type_str, "boolean") == 0) {
            if (!cJSON_IsBool(value)) {
                valid = validate_type_mismatch(param_name, "boolean",
                    cJSON_IsNumber(value) ? "number" : "non-boolean");
                goto cleanup;
            }

        } else if (strcmp(type_str, "array") == 0) {
            if (!cJSON_IsArray(value)) {
                valid = validate_type_mismatch(param_name, "array", "non-array");
                goto cleanup;
            }
            int arr_len = cJSON_GetArraySize(value);
            cJSON* max_items = cJSON_GetObjectItem(schema, "maxItems");
            if (cJSON_IsNumber(max_items) && arr_len > max_items->valuedouble) {
                SVC_LOG_WARN("Parameter '%s': array length %d > maxItems %.0f",
                             param_name, arr_len, max_items->valuedouble);
                valid = 0; goto cleanup;
            }
            if (arr_len > MAX_ARRAY_LEN) {
                SVC_LOG_WARN("Parameter '%s': array exceeds max items %d",
                             param_name, MAX_ARRAY_LEN);
                valid = 0; goto cleanup;
            }

        } else if (strcmp(type_str, "object") == 0) {
            if (!cJSON_IsObject(value)) {
                valid = validate_type_mismatch(param_name, "object", "non-object");
                goto cleanup;
            }
        }
    }

cleanup:
    cJSON_Delete(schema);
    return valid;
}

int tool_validator_validate(tool_validator_t* val,
                            const tool_metadata_t* meta,
                            const char* params_json) {
    if (!meta || !params_json) return 0;
    (void)val;

    cJSON* root = cJSON_Parse(params_json);
    if (!root) {
        SVC_LOG_WARN("Invalid JSON params for tool %s", meta->id);
        return 0;
    }

    if (meta->param_count > 0 && meta->params) {
        for (size_t i = 0; i < meta->param_count; ++i) {
            const char* pname = meta->params[i].name;
            const char* pschema = meta->params[i].schema;
            cJSON* item = cJSON_GetObjectItem(root, pname);

            if (!item) {
                SVC_LOG_WARN("Missing required parameter '%s' for tool %s",
                             pname, meta->id);
                cJSON_Delete(root);
                return 0;
            }

            if (!validate_single_param(pname, pschema, item)) {
                SVC_LOG_WARN("Validation failed for parameter '%s' in tool %s",
                             pname, meta->id);
                cJSON_Delete(root);
                return 0;
            }
        }
    }

    cJSON_Delete(root);
    return 1;
}
