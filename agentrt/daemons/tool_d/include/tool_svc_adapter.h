/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
 *
 * @file tool_svc_adapter.h
 * @brief 工具服务适配器头文件
 *
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTRT_DAEMON_TOOL_SVC_ADAPTER_H
#define AGENTRT_DAEMON_TOOL_SVC_ADAPTER_H

#include "svc_common.h"
#include "tool_service.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

AGENTRT_API agentrt_error_t tool_service_adapter_create(agentrt_service_t *out_service,
                                                        const agentrt_svc_config_t *config);

AGENTRT_API agentrt_error_t tool_service_adapter_wrap(agentrt_service_t *out_service,
                                                      tool_service_t tool_svc,
                                                      const agentrt_svc_config_t *config);

AGENTRT_API tool_service_t tool_service_adapter_get_original(agentrt_service_t service);

AGENTRT_API agentrt_error_t tool_service_adapter_init(agentrt_service_t service);
AGENTRT_API agentrt_error_t tool_service_adapter_start(agentrt_service_t service);
AGENTRT_API agentrt_error_t tool_service_adapter_stop(agentrt_service_t service, bool force);
AGENTRT_API void tool_service_adapter_destroy(agentrt_service_t service);
AGENTRT_API agentrt_error_t tool_service_adapter_healthcheck(agentrt_service_t service);

AGENTRT_API const agentrt_svc_interface_t *tool_service_adapter_get_interface(void);

#ifdef __cplusplus
}
#endif

#endif /* AGENTRT_DAEMON_TOOL_SVC_ADAPTER_H */
