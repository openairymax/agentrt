/**
 * @file llm_client.h
 * @brief LLM客户端接口 - CoreLoopThree本地副本
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 本文件是 llm_client 的公共接口，
 * 仅包含 CoreLoopThree 所需的类型声明和函数原型。
 * 实现位于 MemoryRovol 独立仓库或内置 LLM 适配器中。
 *
 * BAN-35 合规：CoreLoopThree 使用内置 memory 子系统
 */

#ifndef AGENTRT_LLM_CLIENT_H
#define AGENTRT_LLM_CLIENT_H

#include "agentrt.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct agentrt_llm_service agentrt_llm_service_t;

typedef struct agentrt_llm_config {
    const char *model_name;
    const char *api_key;
    const char *base_url;
    uint32_t timeout_ms;
    float temperature;
    uint32_t max_tokens;
} agentrt_llm_config_t;

typedef struct agentrt_llm_request {
    const char *model;
    const char *prompt;
    float temperature;
    uint32_t max_tokens;
    const char *system_prompt;
} agentrt_llm_request_t;

typedef struct agentrt_llm_response {
    char *text;
    uint32_t usage_tokens;
    uint32_t total_tokens;
    uint32_t finish_reason;
} agentrt_llm_response_t;

#ifndef MEMORYROVOL_OSS

typedef struct agentrt_dual_think_config agentrt_dual_think_config_t;

typedef struct agentrt_dual_think_result agentrt_dual_think_result_t;

agentrt_error_t agentrt_llm_dual_think(agentrt_llm_service_t *service,
                                       const agentrt_dual_think_config_t *config,
                                       const char *user_prompt,
                                       agentrt_dual_think_result_t **out_result);

void agentrt_llm_dual_result_free(agentrt_dual_think_result_t *result);

agentrt_error_t agentrt_llm_dual_think_simple(agentrt_llm_service_t *service,
                                              const char *user_prompt, char **out_response);

const agentrt_dual_think_config_t *agentrt_dual_think_config_default(void);

#endif

agentrt_error_t agentrt_llm_service_create(const agentrt_llm_config_t *manager,
                                           agentrt_llm_service_t **out_service);

/**
 * @brief P2.7: 便捷构造 — 创建未配置的 LLM service（等价于 create(NULL)）
 *
 * 供无法包含 llm_client.h 的 TU（如 loop.c，存在 agentrt_llm_config_t
 * 与 yaml_loader.h 的 typedef 冲突）使用。后续通过 set_adapter 注入 IPC adapter。
 *
 * @param out_service [out] 创建的 service（调用方负责 destroy）
 * @return AGENTRT_SUCCESS 或错误码
 *
 * @ownership out_service: OWNER
 */
agentrt_error_t agentrt_llm_service_create_default(agentrt_llm_service_t **out_service);

void agentrt_llm_service_destroy(agentrt_llm_service_t *service);

/**
 * @brief P2.7: 注入 IPC adapter，激活 LLM 调用路径
 *
 * 将 llm_svc_adapter_t (IPC 通道) 注入到 service 中。注入后：
 *   - 若 adapter 已连接到 llm_d，is_available() 返回 1
 *   - call()/complete()/dual_think() 通过 IPC 桥接到 llm_d
 *   - adapter 所有权仍由调用方管理（borrowed），destroy 时不释放
 *
 * @param service LLM 服务句柄
 * @param adapter llm_svc_adapter_t*（不透明指针，NULL 表示卸载 adapter）
 * @return AGENTRT_SUCCESS 或错误码
 *
 * @ownership adapter: BORROW
 */
agentrt_error_t agentrt_llm_service_set_adapter(agentrt_llm_service_t *service, void *adapter);

agentrt_error_t agentrt_llm_service_call(agentrt_llm_service_t *service, const char *prompt,
                                         char **out_response);

int agentrt_llm_service_is_available(const agentrt_llm_service_t *service);

agentrt_error_t agentrt_llm_complete(agentrt_llm_service_t *service,
                                     const agentrt_llm_request_t *request,
                                     agentrt_llm_response_t **out_response);

void agentrt_llm_response_free(agentrt_llm_response_t *response);

#ifdef __cplusplus
}
#endif

#endif
