/**
 * @file executor.h
 * @brief 工具执行器接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef TOOL_EXECUTOR_H
#define TOOL_EXECUTOR_H

#include "tool_service.h"
#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tool_executor tool_executor_t;

typedef struct {
    int max_workers;
    int timeout_sec;
    char* workbench_type;
} tool_executor_config_t;

tool_executor_t* tool_executor_create(const tool_executor_config_t* cfg);
tool_executor_t* tool_executor_create_ex(const tool_executor_config_t* ecfg);
void tool_executor_destroy(tool_executor_t* exec);

/**
 * @brief 执行工具
 * @param exec 执行器
 * @param meta 工具元数据
 * @param params_json 参数 JSON
 * @param out_result 输出结果
 * @return 0 成功，其他错误码
 */
int tool_executor_run(tool_executor_t* exec,
                      const tool_metadata_t* meta,
                      const char* params_json,
                      tool_result_t** out_result);

typedef void (*tool_execute_callback_t)(tool_result_t* result, void* user_data);
int tool_executor_run_async(tool_executor_t* exec,
                           const tool_metadata_t* meta,
                           const char* params_json,
                           tool_execute_callback_t callback,
                           void* user_data,
                           tool_result_t** out_result);

#ifdef __cplusplus
}
#endif

#endif /* TOOL_EXECUTOR_H */