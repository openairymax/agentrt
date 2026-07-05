/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
 *
 * @file llm_svc_adapter.h
 * @brief LLM服务适配器头文件
 *
 * 提供LLM服务与AgentRT统一服务管理框架的适配接口。
 *
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTRT_DAEMON_LLM_SVC_ADAPTER_H
#define AGENTRT_DAEMON_LLM_SVC_ADAPTER_H

#include "llm_service.h"
#include "svc_common.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

AGENTRT_API agentrt_error_t llm_service_adapter_create(agentrt_service_t *out_service,
                                                       const agentrt_svc_config_t *config);

AGENTRT_API agentrt_error_t llm_service_adapter_wrap(agentrt_service_t *out_service,
                                                     llm_service_t llm_svc,
                                                     const agentrt_svc_config_t *config);

AGENTRT_API llm_service_t llm_service_adapter_get_original(agentrt_service_t service);

AGENTRT_API agentrt_error_t llm_service_adapter_init(agentrt_service_t service);
AGENTRT_API agentrt_error_t llm_service_adapter_start(agentrt_service_t service);
AGENTRT_API agentrt_error_t llm_service_adapter_stop(agentrt_service_t service, bool force);
AGENTRT_API void llm_service_adapter_destroy(agentrt_service_t service);
AGENTRT_API agentrt_error_t llm_service_adapter_healthcheck(agentrt_service_t service);

AGENTRT_API const agentrt_svc_interface_t *llm_service_adapter_get_interface(void);

#ifdef __cplusplus
}
#endif

#endif /* AGENTRT_DAEMON_LLM_SVC_ADAPTER_H */
