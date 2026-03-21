/**
 * @file svc_common.h
 * @brief 服务层公共接口（IPC、配置、日志）
 * @copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
 */

#ifndef AGENTOS_SVC_COMMON_H
#define AGENTOS_SVC_COMMON_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 日志封装（复用 utils/logger） ==================== */
#include "logger.h"  // 来自 utils/observability

/* ==================== 通用错误码 ==================== */
typedef enum {
    SVC_OK = 0,
    SVC_ERR_INVALID_PARAM = -1,
    SVC_ERR_OUT_OF_MEMORY = -2,
    SVC_ERR_IO = -3,
    SVC_ERR_NOT_FOUND = -4,
    SVC_ERR_PERMISSION = -5,
    SVC_ERR_TIMEOUT = -6,
    SVC_ERR_RPC = -7,
} svc_error_t;

/* ==================== IPC 客户端 ==================== */

/**
 * @brief 初始化 IPC 客户端
 * @param baseruntime_url BaseRuntime HTTP 地址，如 "http://127.0.0.1:18789"
 * @return 0 成功，-1 失败
 */
int svc_ipc_init(const char* baseruntime_url);

/**
 * @brief 清理 IPC 客户端资源
 */
void svc_ipc_cleanup(void);

/**
 * @brief 执行 JSON-RPC 调用
 * @param method 方法名
 * @param params JSON 格式的参数（可为 NULL）
 * @param out_result 输出结果 JSON 字符串（需调用者 free）
 * @param timeout_ms 超时（毫秒）
 * @return 0 成功，负值错误码
 */
int svc_rpc_call(const char* method, const char* params, char** out_result, uint32_t timeout_ms);

/* ==================== 配置加载 ==================== */

/**
 * @brief 服务通用配置
 */
typedef struct svc_config {
    char* service_name;          /**< 服务名称 */
    char* listen_addr;           /**< 监听地址（如 "0.0.0.0:8080"） */
    int   log_level;             /**< 日志级别（与 logger.h 一致） */
    // 可以扩展更多通用字段
} svc_config_t;

/**
 * @brief 加载 YAML 配置文件
 * @param path 文件路径
 * @param out_config 输出配置结构体（需调用 svc_config_free 释放）
 * @return 0 成功，负值错误码
 */
int svc_config_load(const char* path, svc_config_t** out_config);

/**
 * @brief 释放配置结构体
 */
void svc_config_free(svc_config_t* config);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_SVC_COMMON_H */