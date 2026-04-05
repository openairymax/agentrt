/**
 * @file private.h
 * @brief AgentOS 数据分区内部头文件
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *
 * "From data intelligence emerges."
 */

#ifndef AGENTOS_heapstore_PRIVATE_H
#define AGENTOS_heapstore_PRIVATE_H

#include "../include/heapstore.h"

#include <stdatomic.h>
#include <pthread.h>

#define heapstore_MAX_PATH_LEN 512
#define heapstore_MAX_NAME_LEN 128

typedef struct heapstore_submodule heapstore_submodule_t;

typedef heapstore_error_t (*submodule_init_fn)(void);
typedef void (*submodule_shutdown_fn)(void);

struct heapstore_submodule {
    const char* name;
    submodule_init_fn init;
    submodule_shutdown_fn shutdown;
    atomic_bool initialized;
};

extern heapstore_submodule_t g_heapstore_submodules[];

heapstore_error_t heapstore_registry_init(void);
void heapstore_registry_shutdown(void);

heapstore_error_t heapstore_trace_init(void);
void heapstore_trace_shutdown(void);

heapstore_error_t heapstore_ipc_init(void);
void heapstore_ipc_shutdown(void);

heapstore_error_t heapstore_memory_init(void);
void heapstore_memory_shutdown(void);

heapstore_error_t heapstore_log_init(void);
void heapstore_log_shutdown(void);

bool heapstore_registry_is_healthy(void);
bool heapstore_trace_is_healthy(void);
bool heapstore_log_is_healthy(void);
bool heapstore_ipc_is_healthy(void);
bool heapstore_memory_is_healthy(void);

#endif /* AGENTOS_heapstore_PRIVATE_H */
