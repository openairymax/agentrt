/**
 * @file tool_errors.h
 * @brief 工具服务特有错误码
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef TOOL_ERRORS_H
#define TOOL_ERRORS_H

#include "error.h"

#define TOOL_ERR_BASE               (AGENTOS_ERR_SERVICE_BASE + 100)
#define TOOL_ERR_NOT_FOUND          (TOOL_ERR_BASE + 1)
#define TOOL_ERR_INVALID_PARAMS     (TOOL_ERR_BASE + 2)
#define TOOL_ERR_PERMISSION_DENIED  (TOOL_ERR_BASE + 3)
#define TOOL_ERR_EXEC_FAILED        (TOOL_ERR_BASE + 4)
#define TOOL_ERR_TIMEOUT            (TOOL_ERR_BASE + 5)
#define TOOL_ERR_NOT_IMPLEMENTED    (TOOL_ERR_BASE + 6)
#define TOOL_ERR_IO                 (TOOL_ERR_BASE + 7)
#define TOOL_ERR_FORK               (TOOL_ERR_BASE + 8)

#endif /* TOOL_ERRORS_H */