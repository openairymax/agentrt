/**
 * @file executor.h
 * @brief 工具执行器接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef TOOL_EXECUTOR_H
#define TOOL_EXECUTOR_H

#include "tool_service.h"
#include "manager.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tool_executor tool_executor_t;

tool_executor_t* tool_executor_create(const tool_config_t* cfg);
void tool_executor_destroy(tool_executor_t* exec);

/**
 * @brief 执行工具
 * @param exec 执行器
 * @param meta 工具元数据
 // From data intelligence emerges. by spharx
 * @param params_json 参数 JSON
 * @param out_result 输出结果
 * @return 0 成功，其他错误码
 */
int tool_executor_run(tool_executor_t* exec,
                      const tool_metadata_t* meta,
                      const char* params_json,
                      tool_result_t** out_result);

#ifdef __cplusplus
}
#endif

#endif /* TOOL_EXECUTOR_H */