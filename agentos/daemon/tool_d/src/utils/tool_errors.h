/**
 * @file tool_errors.h
 * @brief 工具服务特有错误码
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef TOOL_ERRORS_H
#define TOOL_ERRORS_H

#include "error.h"

#ifndef AGENTOS_ERR_SERVICE_BASE
#define AGENTOS_ERR_SERVICE_BASE        3000
#endif

#define TOOL_ERR_BASE               (AGENTOS_ERR_SERVICE_BASE + 100)
#define TOOL_ERR_NOT_FOUND          (TOOL_ERR_BASE + 1)
#define TOOL_ERR_INVALID_PARAMS     (TOOL_ERR_BASE + 2)
#define TOOL_ERR_PERMISSION_DENIED  (TOOL_ERR_BASE + 3)
#define TOOL_ERR_EXEC_FAILED        (TOOL_ERR_BASE + 4)
#define TOOL_ERR_TIMEOUT            (TOOL_ERR_BASE + 5)
#define TOOL_ERR_NOT_IMPLEMENTED    (TOOL_ERR_BASE + 6)
#define TOOL_ERR_IO                 (TOOL_ERR_BASE + 7)
#define TOOL_ERR_FORK               (TOOL_ERR_BASE + 8)

#ifndef AGENTOS_ERR_TOOL_NOT_FOUND
#define AGENTOS_ERR_TOOL_NOT_FOUND       TOOL_ERR_NOT_FOUND
#endif
#ifndef AGENTOS_ERR_TOOL_VALIDATION
#define AGENTOS_ERR_TOOL_VALIDATION      TOOL_ERR_INVALID_PARAMS
#endif

#ifndef AGENTOS_ERR_TOOL_EXEC_FAIL
#define AGENTOS_ERR_TOOL_EXEC_FAIL        TOOL_ERR_EXEC_FAILED
#endif

#ifndef AGENTOS_ERR_TOOL_TIMEOUT
#define AGENTOS_ERR_TOOL_TIMEOUT          TOOL_ERR_TIMEOUT
#endif

#endif /* TOOL_ERRORS_H */