/**
 * @file logging.h
 * @brief Standard logging interface for AgentOS
 * @copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 * 
 * @details
 * This is the standard logging interface header that provides
 * AGENTOS_LOG_* macros and logging functions.
 * 
 * This file includes the compatibility logging layer which provides
 * backward compatibility with existing code while offering a path
 * to migrate to the new logging architecture.
 */

#ifndef AGENTOS_LOGGING_H
#define AGENTOS_LOGGING_H

/* Include standard compatibility headers */
#include <agentos/compat/stdbool.h>
#include <agentos/compat/stdint.h>

/* Include the compatibility logging implementation */
#include "../../commons/utils/logging/include/logging_compat.h"

#endif /* AGENTOS_LOGGING_H */