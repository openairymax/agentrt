/**
 * @file error.c
 * @brief Error code lookup table
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "error.h"

#define ERR_BEGIN 0

static const char* error_strings[] = {
    "Success",              /* 0  */
    "Invalid argument",     /* 1  */
    "Out of memory",        /* 2  */
    "Resource busy",        /* 3  */
    "No such entry",        /* 4  */
    "Permission denied",    /* 5  */
    "Operation timed out",  /* 6  */
    "I/O error",            /* 7  */
    "Already exists",       /* 8  */
    "Not initialized",      /* 9  */
    "Operation cancelled",  /* 10 */
    "Not supported",        /* 11 */
    "Value overflow",       /* 12 */
    "Protocol error",       /* 13 */
    "Not connected",        /* 14 */
    "Connection reset",     /* 15 */
    "Access denied",        /* 16 */
    "Connection refused",   /* 17 */
    "Message too large",    /* 18 */
    "No space left",        /* 19 */
    "Range error",          /* 20 */
    "Deadlock detected",    /* 21 */
    "Try again",            /* 22 */
    "Argument too long",    /* 23 */
    "Already in progress",  /* 24 */
    "Unavailable",          /* 25 */
    "Quota exceeded",       /* 26 */
    "Platform not initialized", /* 27 */
    "Protocol not supported",   /* 28 */
    "Service unavailable",      /* 29 */
    NULL,                   /* 30 */
    "Interrupted",          /* 31 */
    "Bad descriptor",       /* 32 */
    "Resource exhausted",   /* 33 */
    "Function not implemented", /* 34 */
    "Dependency cycle",     /* 35 */
    "General failure",      /* 36 */
};

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
