/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file monit_svc_adapter.h
 * @brief 监控服务适配器头文件
 *
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_DAEMON_MONIT_SVC_ADAPTER_H
#define AGENTOS_DAEMON_MONIT_SVC_ADAPTER_H

#include "monitor_service.h"
#include "svc_common.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

AGENTOS_API agentos_error_t monit_service_adapter_create(
    agentos_service_t* out_service,
    const agentos_svc_config_t* config
);

AGENTOS_API agentos_error_t monit_service_adapter_wrap(
    agentos_service_t* out_service,
    monitor_service_t monit_svc,
    const agentos_svc_config_t* config
);

AGENTOS_API monitor_service_t monit_service_adapter_get_original(
    agentos_service_t service
);

AGENTOS_API agentos_error_t monit_service_adapter_init(agentos_service_t service);
AGENTOS_API agentos_error_t monit_service_adapter_start(agentos_service_t service);
AGENTOS_API agentos_error_t monit_service_adapter_stop(agentos_service_t service, bool force);
AGENTOS_API void monit_service_adapter_destroy(agentos_service_t service);
AGENTOS_API agentos_error_t monit_service_adapter_healthcheck(agentos_service_t service);

AGENTOS_API const agentos_svc_interface_t* monit_service_adapter_get_interface(void);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_DAEMON_MONIT_SVC_ADAPTER_H */
