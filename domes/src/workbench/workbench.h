/**
 * @file workbench.h
 * @brief 虚拟工位管理器接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
 */

#ifndef DOMAIN_WORKBENCH_H
#define DOMAIN_WORKBENCH_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 前向声明 */
typedef struct workbench_manager workbench_manager_t;

/**
 * @brief 创建工位管理器
 * @param type 后端类型 ("process" 或 "container")
 * @param memory_bytes 内存限制（字节），0表示无限制
 * @param cpu_quota CPU 配额（核心数），0表示无限制
 * @param network 是否启用网络（1启用，0禁用）
 * @param rootfs 容器根文件系统路径（仅在 container 类型时使用，可为NULL）
 * @return 管理器句柄，失败返回 NULL
 */
workbench_manager_t* workbench_manager_create(const char* type,
                                              uint64_t memory_bytes,
                                              float cpu_quota,
                                              int network,
                                              const char* rootfs);

/**
 * @brief 销毁管理器，释放所有资源（会销毁所有工位）
 * @param mgr 管理器句柄
 */
void workbench_manager_destroy(workbench_manager_t* mgr);

/**
 * @brief 为指定 Agent 创建虚拟工位
 * @param mgr 管理器句柄
 * @param agent_id Agent ID
 * @param out_workbench_id 输出工位 ID（需调用者 free）
 * @return 0 成功，-1 失败
 */
int workbench_manager_create_workbench(workbench_manager_t* mgr,
                                       const char* agent_id,
                                       char** out_workbench_id);

/**
 * @brief 在工位中执行命令
 * @param mgr 管理器句柄
 * @param workbench_id 工位 ID
 * @param argv 命令及参数数组（以 NULL 结尾）
 * @param timeout_ms 超时（毫秒），0 表示不等待立即返回（需通过其他方式获取结果）
 * @param out_stdout 输出 stdout（需调用者 free）
 * @param out_stderr 输出 stderr（需调用者 free）
 * @param out_exit_code 退出码
 * @param out_error 详细错误信息（需调用者 free）
 * @return 0 成功，-1 失败（工位不存在或执行错误）
 */
int workbench_manager_exec(workbench_manager_t* mgr,
                           const char* workbench_id,
                           const char* const* argv,
                           uint32_t timeout_ms,
                           char** out_stdout,
                           char** out_stderr,
                           int* out_exit_code,
                           char** out_error);

/**
 * @brief 销毁指定工位
 * @param mgr 管理器句柄
 * @param workbench_id 工位 ID
 */
void workbench_manager_destroy_workbench(workbench_manager_t* mgr,
                                         const char* workbench_id);

/**
 * @brief 列出所有活跃工位 ID
 * @param mgr 管理器句柄
 * @param out_ids 输出工位 ID 数组（需调用者 free 每个元素及数组）
 * @param out_count 输出数量
 * @return 0 成功，-1 失败
 */
int workbench_manager_list(workbench_manager_t* mgr,
                           char*** out_ids,
                           size_t* out_count);

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_WORKBENCH_H */