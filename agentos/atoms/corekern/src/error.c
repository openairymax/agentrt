/**
 * @file error.c
 * @brief Error code lookup table
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "error.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

#define ERR_BEGIN 0

static const char* error_strings[] = {
    [0]         "Success",
    [1]         "Invalid argument",
    [2]         "Out of memory",
    [3]         "Resource busy",
    [4]         "No such entry",
    [5]         "Permission denied",
    [6]         "Operation timed out",
    [7]         "I/O error",
    [8]         "Already exists",
    [9]         "Not initialized",
    [10]        "Operation cancelled",
    [11]        "Not supported",
    [12]        "Value overflow",
    [13]        "Protocol error",
    [14]        "Not connected",
    [15]        "Connection reset",
    [16]        "Access denied",
    [17]        "Connection refused",
    [18]        "Message too large",
    [19]        "No space left",
    [20]        "Range error",
    [21]        "Deadlock detected",
    [22]        "Try again",
    [23]        "Argument too long",
    [24]        "Already in progress",
    [25]        "Unavailable",
    [26]        "Quota exceeded",
    [27]        "Platform not initialized",
    [28]        "Protocol not supported",
    [29]        "Service unavailable",
    [30]        NULL,
    [31]        "Interrupted",
    [32]        "Bad descriptor",
    [33]        "Resource exhausted",
    [34]        "Function not implemented",
    [35]        "Dependency cycle",
    [36]        "General failure",
};

#pragma GCC diagnostic pop

#define ERROR_COUNT (sizeof(error_strings) / sizeof(error_strings[0]))

const char* agentos_strerror(agentos_error_t err) {
    if (err == AGENTOS_EUNKNOWN) {
        return "Generic error";
    }
    if (err > ERR_BEGIN || err <= -(agentos_error_t)ERROR_COUNT) {
        return "Unknown error";
    }
    const char* s = error_strings[-err];
    return s ? s : "Unknown error";
}
