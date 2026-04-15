/**
 * @file memory.h
 * @brief AgentOS 统一内存管理标准接口
 * @copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 * 
 * @details
 * This is the standard memory management header for AgentOS.
 * It includes the core memory API and compatibility macros.
 * 
 * All source files should use #include <agentos/memory.h> to access
 * memory management functionality.
 */

#ifndef AGENTOS_MEMORY_STANDARD_H
#define AGENTOS_MEMORY_STANDARD_H

#include <agentos/compat/stdint.h>
#include <agentos/compat/stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "../../commons/utils/memory/include/memory_compat.h"

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_MEMORY_STANDARD_H */
