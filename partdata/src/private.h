/**
 * @file private.h
 * @brief AgentOS 数据分区内部头文件
 *
 * Copyright (c) 2026 SPHARX. All Rights Reserved.
 * "From data intelligence emerges."
 */

#ifndef AGENTOS_PARTDATA_PRIVATE_H
#define AGENTOS_PARTDATA_PRIVATE_H

#include "../include/partdata.h"

#include <stdatomic.h>
#include <pthread.h>

#define PARTDATA_MAX_PATH_LEN 512
#define PARTDATA_MAX_NAME_LEN 128

typedef struct partdata_submodule partdata_submodule_t;

typedef partdata_error_t (*submodule_init_fn)(void);
typedef void (*submodule_shutdown_fn)(void);

struct partdata_submodule {
    const char* name;
    submodule_init_fn init;
    submodule_shutdown_fn shutdown;
    atomic_bool initialized;
};

extern partdata_submodule_t g_partdata_submodules[];

partdata_error_t partdata_registry_init(void);
void partdata_registry_shutdown(void);

partdata_error_t partdata_trace_init(void);
void partdata_trace_shutdown(void);

partdata_error_t partdata_ipc_init(void);
void partdata_ipc_shutdown(void);

partdata_error_t partdata_memory_init(void);
void partdata_memory_shutdown(void);

partdata_error_t partdata_log_init(void);
void partdata_log_shutdown(void);

bool partdata_registry_is_healthy(void);
bool partdata_trace_is_healthy(void);
bool partdata_log_is_healthy(void);
bool partdata_ipc_is_healthy(void);
bool partdata_memory_is_healthy(void);

#endif /* AGENTOS_PARTDATA_PRIVATE_H */
