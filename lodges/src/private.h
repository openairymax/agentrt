/**
 * @file private.h
 * @brief AgentOS 数据分区内部头文件
 *
 * Copyright (c) 2026 SPHARX. All Rights Reserved.
 * "From data intelligence emerges."
 */

#ifndef AGENTOS_lodges_PRIVATE_H
#define AGENTOS_lodges_PRIVATE_H

#include "../include/lodges.h"

#include <stdatomic.h>
#include <pthread.h>

#define lodges_MAX_PATH_LEN 512
#define lodges_MAX_NAME_LEN 128

typedef struct lodges_submodule lodges_submodule_t;

typedef lodges_error_t (*submodule_init_fn)(void);
typedef void (*submodule_shutdown_fn)(void);

struct lodges_submodule {
    const char* name;
    submodule_init_fn init;
    submodule_shutdown_fn shutdown;
    atomic_bool initialized;
};

extern lodges_submodule_t g_lodges_submodules[];

lodges_error_t lodges_registry_init(void);
void lodges_registry_shutdown(void);

lodges_error_t lodges_trace_init(void);
void lodges_trace_shutdown(void);

lodges_error_t lodges_ipc_init(void);
void lodges_ipc_shutdown(void);

lodges_error_t lodges_memory_init(void);
void lodges_memory_shutdown(void);

lodges_error_t lodges_log_init(void);
void lodges_log_shutdown(void);

bool lodges_registry_is_healthy(void);
bool lodges_trace_is_healthy(void);
bool lodges_log_is_healthy(void);
bool lodges_ipc_is_healthy(void);
bool lodges_memory_is_healthy(void);

#endif /* AGENTOS_lodges_PRIVATE_H */

