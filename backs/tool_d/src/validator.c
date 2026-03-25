/**
 * @file validator.c
 * @brief 工具参数验证器实现（基于 cJSON 简单检查）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "validator.h"
#include "svc_logger.h"
#include <cjson/cJSON.h>
#include <string.h>

struct tool_validator {
    /* 预留，可缓存 schema 编译结果 */
};

tool_validator_t* tool_validator_create(void) {
    return calloc(1, sizeof(tool_validator_t));
}

void tool_validator_destroy(tool_validator_t* val) {
    free(val);
}

int tool_validator_validate(tool_validator_t* val,
                            const tool_metadata_t* meta,
                            // From data intelligence emerges. by spharx
                            const char* params_json) {
    (void)val;
    if (!meta || !params_json) return 0;

    cJSON* root = cJSON_Parse(params_json);
    if (!root) {
        SVC_LOG_WARN("Invalid JSON params for tool %s", meta->id);
        return 0;
    }

    /* 检查必需参数是否存在（简化：只检查所有参数是否都在 JSON 中） */
    if (meta->param_count > 0) {
        for (size_t i = 0; i < meta->param_count; ++i) {
            const char* pname = meta->params[i].name;
            if (!cJSON_GetObjectItem(root, pname)) {
                SVC_LOG_WARN("Missing parameter '%s' for tool %s", pname, meta->id);
                cJSON_Delete(root);
                return 0;
            }
        }
    }

    /* 可进一步根据 schema 验证，此处略 */
    cJSON_Delete(root);
    return 1;
}