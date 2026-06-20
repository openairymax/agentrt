/**
 * @file tool_svc_adapter.h
 * @brief C-L04: CoreLoopThree → tool_d IPC 适配器
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 提供 CoreLoopThree 执行引擎与 tool_d 守护进程之间的 IPC 桥梁。
 * 内部使用 ServiceDiscovery 动态发现 tool_d 端点，
 * 通过 IPC Bus 发送工具执行请求并接收结果。
 *
 * 使用方式：
 * @code
 *   // 1. 初始化适配器
 *   tool_svc_adapter_t *adapter = tool_svc_adapter_create(NULL);
 *
 *   // 2. 获取模拟的 tool_service_t 句柄
 *   tool_service_t *svc = tool_svc_adapter_get_service(adapter);
 *
 *   // 3. 注入到认知引擎
 *   agentos_cognition_set_tool_service(engine, svc);
 *
 *   // 4. 关闭时销毁
 *   tool_svc_adapter_destroy(adapter);
 * @endcode
 *
 * @see service_discovery.h
 * @see ipc_service_bus.h
 * @see tool_approval.h
 * @see P1.3 C-L04 连接线
 */

#ifndef AGENTOS_CORELOOPTHREE_TOOL_SVC_ADAPTER_H
#define AGENTOS_CORELOOPTHREE_TOOL_SVC_ADAPTER_H

#include "tool_service.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 适配器句柄 ==================== */

typedef struct tool_svc_adapter_s tool_svc_adapter_t;

/* ==================== 适配器配置 ==================== */

typedef struct {
    const char *tool_d_service_name; /**< tool_d 服务名称，默认 "tool_d" */
    const char *channel_name;        /**< IPC 通道名称，默认 "coreloopthree-tool" */
    uint32_t request_timeout_ms;     /**< 请求超时（毫秒），0 使用默认 60000 */
    uint32_t sd_poll_interval_ms;    /**< 服务发现轮询间隔，0 使用默认 5000 */
    bool enable_approval;            /**< 是否启用工具审批（C-L05） */
    const char *agent_id;            /**< 审批 Agent ID */
} tool_svc_adapter_config_t;

/* ==================== 生命周期 ==================== */

/**
 * @brief 创建工具服务适配器
 *
 * 初始化 ServiceDiscovery 和 IPC Bus 客户端，
 * 自动发现 tool_d 守护进程端点。
 *
 * @param config 适配器配置（NULL 使用默认）
 * @return 适配器句柄，失败返回 NULL
 *
 * @ownership return: OWNER
 */
tool_svc_adapter_t *tool_svc_adapter_create(const tool_svc_adapter_config_t *config);

/**
 * @brief 销毁工具服务适配器
 *
 * 释放 IPC 资源和 ServiceDiscovery 客户端。
 *
 * @param adapter 适配器句柄
 *
 * @ownership adapter: TRANSFER
 */
void tool_svc_adapter_destroy(tool_svc_adapter_t *adapter);

/* ==================== 服务接口 ==================== */

/**
 * @brief 获取适配器包装的 tool_service_t 句柄
 *
 * 返回的句柄可注入到认知/执行引擎，通过 IPC 透明调用 tool_d。
 * 句柄生命周期由适配器管理，在 adapter 销毁后失效。
 *
 * @param adapter 适配器句柄
 * @return tool_service_t 句柄（BORROW），NULL 如果适配器未就绪
 *
 * @ownership return: BORROW
 */
tool_service_t *tool_svc_adapter_get_service(tool_svc_adapter_t *adapter);

/**
 * @brief 通过 IPC 执行工具调用
 *
 * 内部流程：
 *   1. 通过 ServiceDiscovery 查找 tool_d 端点
 *   2. 序列化请求为 JSON-RPC 消息
 *   3. 通过 IPC Bus 发送到 tool_d
 *   4. 等待执行结果并反序列化
 *
 * @param adapter 适配器句柄
 * @param req     执行请求
 * @param out_result 输出结果（需调用者通过 tool_result_free 释放）
 * @return 0 成功，非0 失败
 *
 * @ownership req: BORROW, out_result: OWNER
 */
int tool_svc_adapter_execute(tool_svc_adapter_t *adapter,
                             const tool_execute_request_t *req,
                             tool_result_t **out_result);

/**
 * @brief 通过 IPC 流式执行工具
 *
 * @param adapter 适配器句柄
 * @param req     执行请求
 * @param callback 流式回调
 * @param callback_data 回调数据
 * @param out_result 输出最终结果
 * @return 0 成功，非0 失败
 *
 * @ownership req: BORROW, callback: BORROW, out_result: OWNER
 */
int tool_svc_adapter_execute_stream(tool_svc_adapter_t *adapter,
                                    const tool_execute_request_t *req,
                                    tool_stream_callback_t callback,
                                    void *callback_data,
                                    tool_result_t **out_result);

/**
 * @brief 通过 IPC 注册工具到 tool_d
 *
 * @param adapter 适配器句柄
 * @param meta    工具元数据
 * @return 0 成功，非0 失败
 *
 * @ownership meta: BORROW
 */
int tool_svc_adapter_register(tool_svc_adapter_t *adapter,
                              const tool_metadata_t *meta);

/**
 * @brief 通过 IPC 获取工具列表
 *
 * @param adapter 适配器句柄
 * @param out_json 输出 JSON 工具列表（需调用者释放）
 * @return 0 成功，非0 失败
 *
 * @ownership out_json: OWNER
 */
int tool_svc_adapter_list(tool_svc_adapter_t *adapter, char **out_json);

/* ==================== 状态查询 ==================== */

/**
 * @brief 检查适配器是否已连接到 tool_d
 *
 * @param adapter 适配器句柄
 * @return true 已连接
 */
bool tool_svc_adapter_is_connected(tool_svc_adapter_t *adapter);

/**
 * @brief 获取适配器统计信息
 *
 * @param adapter 适配器句柄
 * @param out_total_executions 输出总执行次数
 * @param out_total_errors 输出总错误数
 * @param out_avg_latency_ms 输出平均延迟（毫秒）
 */
void tool_svc_adapter_get_stats(tool_svc_adapter_t *adapter,
                                uint64_t *out_total_executions,
                                uint64_t *out_total_errors,
                                uint64_t *out_avg_latency_ms);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_CORELOOPTHREE_TOOL_SVC_ADAPTER_H */