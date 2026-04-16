/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * workbench_process.h - Process Management Internal Interface
 *
 * Process types and functions are defined in platform.h.
 * This header provides additional workbench-specific process utilities.
 */

#ifndef CUPOLAS_WORKBENCH_PROCESS_H
#define CUPOLAS_WORKBENCH_PROCESS_H

#include "../platform/platform.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

int cupolas_process_terminate(cupolas_process_t proc, int signal);

int cupolas_process_close(cupolas_process_t proc);

cupolas_pid_t cupolas_process_getpid(cupolas_process_t proc);

#ifdef __cplusplus
}
#endif

#endif /* CUPOLAS_WORKBENCH_PROCESS_H */
