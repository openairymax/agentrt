/**
 * @file tool_helpers.c
 * @brief 辅助函数
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "tool_helpers.h"

#include "daemon_errors.h"
#include "error.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool tool_is_valid_id(const char *id)
{
    if (!id || id[0] == '\0')
        return false;
    for (const char *p = id; *p; p++) {
        if (!isalnum((unsigned char)*p) && *p != '_' && *p != '-' && *p != '.') {
            return false;
        }
    }
    return true;
}

size_t tool_sanitize_name(const char *src, char *dst, size_t dst_size)
{
    if (!src || !dst || dst_size == 0)
        return 0;
    size_t i = 0;
    for (const char *p = src; *p && i < dst_size - 1; p++) {
        if (isalnum((unsigned char)*p) || *p == '_' || *p == '-') {
            dst[i++] = *p;
        } else {
            dst[i++] = '_';
        }
    }
    dst[i] = '\0';
    return i;
}

int tool_parse_version(const char *version_str, int *major, int *minor, int *patch)
{
    if (!version_str)
        return AGENTOS_ERR_INVALID_PARAM;
    int m = 0, n = 0, p = 0;
    int fields = sscanf(version_str, "%d.%d.%d", &m, &n, &p);
    if (fields < 1)
        return AGENTOS_ERR_PARSE_ERROR;
    if (major)
        *major = m;
    if (minor)
        *minor = (fields >= 2) ? n : 0;
    if (patch)
        *patch = (fields >= 3) ? p : 0;
    return 0;
}
