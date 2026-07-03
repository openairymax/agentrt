/**
 * @file llm_svc_adapter.h
 * @brief C-L02: CoreLoopThree → llm_d IPC 适配器
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 提供 CoreLoopThree 认知引擎与 llm_d 守护进程之间的 IPC 桥梁。
 * 内部使用 ServiceDiscovery 动态发现 llm_d 端点，
 * 通过 IPC Bus 发送请求并接收响应。
 *
 * 使用方式：
 * @code
 *   // 1. 初始化适配器
 *   llm_svc_adapter_t *adapter = llm_svc_adapter_create(NULL);
 *
 *   // 2. 注入到认知引擎（IPC adapter 路径，P1.2.1 首选）
 *   agentos_cognition_set_llm_adapter(engine, adapter);
 *
 *   // 3. 关闭时销毁
 *   llm_svc_adapter_destroy(adapter);
 * @endcode
 *
 * 注意：llm_svc_adapter_get_service() 保留为接口兼容，始终返回 NULL
 * （llm_service_t 由 llm_d daemon 内部管理，coreloopthree 不直接持有）。
 *
 * @see service_discovery.h
 * @see ipc_service_bus.h
 * @see P1.2 C-L02 连接线
 */

#ifndef AGENTOS_CORELOOPTHREE_LLM_SVC_ADAPTER_H
#define AGENTOS_CORELOOPTHREE_LLM_SVC_ADAPTER_H

#include "llm_service.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 适配器句柄 ==================== */

typedef struct llm_svc_adapter_s llm_svc_adapter_t;

/* ==================== 适配器配置 ==================== */

typedef struct {
    const char *llm_d_service_name;  /**< llm_d 服务名称，默认 "llm_d" */
    const char *channel_name;        /**< IPC 通道名称，默认 "coreloopthree-llm" */
    uint32_t request_timeout_ms;     /**< 请求超时（毫秒），0 使用默认 30000 */
    uint32_t sd_poll_interval_ms;    /**< 服务发现轮询间隔，0 使用默认 5000 */
    bool enable_streaming;           /**< 是否启用流式响应 */
} llm_svc_adapter_config_t;

/* ==================== 生命周期 ==================== */

/**
 * @brief 创建 LLM 服务适配器
 *
 * 初始化 ServiceDiscovery 和 IPC Bus 客户端，
 * 自动发现 llm_d 守护进程端点。
 *
 * @param config 适配器配置（NULL 使用默认）
 * @return 适配器句柄，失败返回 NULL
 *
 * @ownership return: OWNER
 */
llm_svc_adapter_t *llm_svc_adapter_create(const llm_svc_adapter_config_t *config);

/**
 * @brief 销毁 LLM 服务适配器
 *
 * 释放 IPC 资源和 ServiceDiscovery 客户端。
 *
 * @param adapter 适配器句柄
 *
 * @ownership adapter: TRANSFER
 */
void llm_svc_adapter_destroy(llm_svc_adapter_t *adapter);

/* ==================== 服务接口 ==================== */

/**
 * @brief 获取 llm_service_t 句柄（接口兼容，始终返回 NULL）
 *
 * llm_service_t 是 llm_d daemon 的内部结构（含 registry/cache/cost/lock 等），
 * 由 daemon 构造和管理，coreloopthree 无法真实构造。本函数保留为接口兼容，
 * 始终返回 NULL。应改用 agentos_cognition_set_llm_adapter() 注入 IPC adapter。
 *
 * @param adapter 适配器句柄
 * @return 始终返回 NULL
 *
 * @ownership return: NONE
 */
llm_service_t *llm_svc_adapter_get_service(llm_svc_adapter_t *adapter);

/**
 * @brief 通过 IPC 发送 LLM 请求并获取响应
 *
 * 直接使用适配器的 IPC 通道发送请求到 llm_d。
 * 内部流程：
 *   1. 通过 ServiceDiscovery 查找 llm_d 端点
 *   2. 序列化请求为 JSON-RPC 消息
 *   3. 通过 IPC Bus 发送到 llm_d
 *   4. 等待响应并反序列化
 *
 * @param adapter 适配器句柄
 * @param config  请求配置
 * @param out_response 输出响应（需调用者通过 llm_response_free 释放）
 * @return 0 成功，非0 失败
 *
 * @ownership config: BORROW, out_response: OWNER
 */
int llm_svc_adapter_complete(llm_svc_adapter_t *adapter,
                             const llm_request_config_t *config,
                             llm_response_t **out_response);

/**
 * @brief 通过 IPC 发送 LLM 请求并在响应到达后回调
 *
 * 当前实现为批处理回调模式：通过 IPC Bus 同步等待 llm_d 完整响应，
 * 收齐后调用 callback 一次，传入整段 JSON（非 token 级流式推送）。
 * token 级流式需要 IPC Bus 流式原语支持，规划在 1.0.x。
 *
 * @param adapter 适配器句柄
 * @param config  请求配置
 * @param callback 响应到达后调用的回调（传入整段 JSON）
 * @param callback_data 回调数据
 * @param out_response 输出完整响应
 * @return 0 成功，非0 失败
 *
 * @ownership config: BORROW, callback: BORROW, out_response: OWNER
 */
int llm_svc_adapter_complete_stream(llm_svc_adapter_t *adapter,
                                    const llm_request_config_t *config,
                                    llm_stream_callback_t callback,
                                    void *callback_data,
                                    llm_response_t **out_response);

/* ==================== 状态查询 ==================== */

/**
 * @brief 检查适配器是否已连接到 llm_d
 *
 * @param adapter 适配器句柄
 * @return true 已连接
 */
bool llm_svc_adapter_is_connected(llm_svc_adapter_t *adapter);

/**
 * @brief 获取适配器统计信息
 *
 * @param adapter 适配器句柄
 * @param out_total_requests 输出总请求数
 * @param out_total_errors 输出总错误数
 * @param out_avg_latency_ms 输出平均延迟（毫秒）
 */
void llm_svc_adapter_get_stats(llm_svc_adapter_t *adapter,
                               uint64_t *out_total_requests,
                               uint64_t *out_total_errors,
                               uint64_t *out_avg_latency_ms);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_CORELOOPTHREE_LLM_SVC_ADAPTER_H */