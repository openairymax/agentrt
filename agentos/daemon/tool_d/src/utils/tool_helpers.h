/**
 * @file tool_helpers.h
 * @brief 工具辅助函数头文件
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef TOOL_HELPERS_H
#define TOOL_HELPERS_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool tool_is_valid_id(const char* id);

size_t tool_sanitize_name(const char* src, char* dst, size_t dst_size);

int tool_parse_version(const char* version_str, int* major, int* minor, int* patch);

#ifdef __cplusplus
}
#endif

#endif /* TOOL_HELPERS_H */
