/**
 * @file error.c
 * @brief 错误码转字符串实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "error.h"

static const char* error_strings[] = {
    /* AGENTOS_SUCCESS (0) */           "Success",
    /* AGENTOS_EINVAL (-1) */           "Invalid argument",
    /* AGENTOS_ENOMEM (-2) */           "Out of memory",
    /* AGENTOS_EBUSY (-3) */            "Resource busy",
    /* AGENTOS_ENOENT (-4) */           "No such entry",
    /* AGENTOS_EPERM (-5) */            "Permission denied",
    /* AGENTOS_ETIMEDOUT (-6) */        "Operation timed out",
    /* AGENTOS_EEXIST (-7) */           "Already exists",
    /* AGENTOS_ECANCELED (-8) */        "Operation canceled",
    /* AGENTOS_ENOTSUP (-9) */          "Not supported",
    /* AGENTOS_EIO (-10) */             "I/O error",
    /* AGENTOS_EINTR (-11) */           "Interrupted",
    /* AGENTOS_EOVERFLOW (-12) */       "Value overflow",
    /* AGENTOS_EBADF (-13) */           "Bad file descriptor",
    /* AGENTOS_ENOTINIT (-14) */        "Not initialized",
    /* AGENTOS_ERESOURCE (-15) */       "Resource exhausted",
    /* AGENTOS_ENOSYS (-16) */           "Function not implemented"
};

#define ERROR_COUNT (sizeof(error_strings) / sizeof(error_strings[0]))

const char* agentos_strerror(agentos_error_t err) {
    if (err == AGENTOS_EUNKNOWN) {
        return "Generic error";
    }
    if (err > 0 || err <= -(agentos_error_t)ERROR_COUNT) {
        return "Unknown error";
    }
    return error_strings[-err];
}
