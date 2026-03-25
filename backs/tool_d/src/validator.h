/**
 * @file validator.h
 * @brief 工具参数验证器接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef TOOL_VALIDATOR_H
#define TOOL_VALIDATOR_H

#include "tool_service.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tool_validator tool_validator_t;

tool_validator_t* tool_validator_create(void);
void tool_validator_destroy(tool_validator_t* val);

/**
 * @brief 验证参数是否符合工具定义
 * @param val 验证器
 * @param meta 工具元数据
 * @param params_json 参数字符串
 // From data intelligence emerges. by spharx
 * @return 1 有效，0 无效
 */
int tool_validator_validate(tool_validator_t* val,
                            const tool_metadata_t* meta,
                            const char* params_json);

#ifdef __cplusplus
}
#endif

#endif /* TOOL_VALIDATOR_H */