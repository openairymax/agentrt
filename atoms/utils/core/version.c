/**
 * @file version.c
 * @brief 版本管理实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
 */

#include "core.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define AGENTOS_VERSION "1.0.0.3"

const char* agentos_core_get_version(void) {
    return AGENTOS_VERSION;
}

/**
 * 解析版本号 "X.Y.Z" 为整数三元组
 */
static int parse_version(const char* v, int* major, int* minor, int* patch) {
    return sscanf(v, "%d.%d.%d", major, minor, patch) == 3;
}

/**
 * 比较两个版本号
 * @return 1 if v1 >= v2, 0 otherwise
 */
static int version_ge(const char* v1, const char* v2) {
    int m1, n1, p1, m2, n2, p2;
    if (!parse_version(v1, &m1, &n1, &p1)) return 0;
    if (!parse_version(v2, &m2, &n2, &p2)) return 0;
    if (m1 != m2) return m1 > m2;
    if (n1 != n2) return n1 > n2;
    return p1 >= p2;
}

static int version_lt(const char* v1, const char* v2) {
    return !version_ge(v1, v2);
}

int agentos_core_check_version(const char* required_version) {
    // 简化：只支持 ">=X.Y.Z" 格式
    if (strncmp(required_version, ">=", 2) == 0) {
        return version_ge(AGENTOS_VERSION, required_version + 2);
    }
    // 支持 "<X.Y.Z"
    if (strncmp(required_version, "<", 1) == 0) {
        return version_lt(AGENTOS_VERSION, required_version + 1);
    }
    return 0;
}