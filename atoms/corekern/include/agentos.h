/**
 * @file agentos.h
 * @brief AgentOS 微内核统一入口头文件
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_AGENTOS_H
#define AGENTOS_AGENTOS_H

#include "export.h"
#include "error.h"
#include "mem.h"
#include "task.h"
#include "ipc.h"
#include "time.h"
#include "observability.h"

#ifdef __cplusplus
extern "C" {
#endif

AGENTOS_API int agentos_core_init(void);

AGENTOS_API void agentos_core_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_AGENTOS_H */
