/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
 *
 * @file sched_svc_adapter.h
 * @brief 调度器服务适配器头文件
 *
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTRT_DAEMON_SCHED_SVC_ADAPTER_H
#define AGENTRT_DAEMON_SCHED_SVC_ADAPTER_H

#include "scheduler_service.h"
#include "svc_common.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

AGENTRT_API agentrt_error_t sched_service_adapter_create(agentrt_service_t *out_service,
                                                         const agentrt_svc_config_t *config);

AGENTRT_API agentrt_error_t sched_service_adapter_wrap(agentrt_service_t *out_service,
                                                       sched_service_t sched_svc,
                                                       const agentrt_svc_config_t *config);

AGENTRT_API sched_service_t sched_service_adapter_get_original(agentrt_service_t service);

AGENTRT_API agentrt_error_t sched_service_adapter_init(agentrt_service_t service);
AGENTRT_API agentrt_error_t sched_service_adapter_start(agentrt_service_t service);
AGENTRT_API agentrt_error_t sched_service_adapter_stop(agentrt_service_t service, bool force);
AGENTRT_API void sched_service_adapter_destroy(agentrt_service_t service);
AGENTRT_API agentrt_error_t sched_service_adapter_healthcheck(agentrt_service_t service);

AGENTRT_API const agentrt_svc_interface_t *sched_service_adapter_get_interface(void);

#ifdef __cplusplus
}
#endif

#endif /* AGENTRT_DAEMON_SCHED_SVC_ADAPTER_H */
