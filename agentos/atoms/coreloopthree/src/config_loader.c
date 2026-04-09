/**
 * @file config_loader.c
 * @brief 配置加载器实�?
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "config_loader.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../../../agentos/commons/utils/memory/include/memory_compat.h"
#include "../../../agentos/commons/utils/string/include/string_compat.h"
#include <string.h>

#define AGENTOS_MAX_CONFIG_SIZE (4 * 1024 * 1024)

/**
 * @brief 从文件加载配置内�?
 * @param path 文件路径
 * @param out_json 输出 JSON 字符串（需调用者释放）
 * @return AGENTOS_SUCCESS 或错误码
 */
agentos_error_t agentos_config_load(const char* path, char** out_json) {
    if (!path || !out_json) return AGENTOS_EINVAL;

    FILE* file = fopen(path, "rb");
    if (!file) {
        AGENTOS_LOG_ERROR("Failed to open manager file: %s", path);
        return AGENTOS_ENOENT;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (size < 0 || size > AGENTOS_MAX_CONFIG_SIZE) {
        fclose(file);
        return (size < 0) ? AGENTOS_EIO : AGENTOS_EMSGSIZE;
    }

    char* buffer = (char*)AGENTOS_MALLOC((size_t)size + 1);
    if (!buffer) {
        fclose(file);
        return AGENTOS_ENOMEM;
    }

    size_t read = fread(buffer, 1, (size_t)size, file);
    buffer[read] = '\0';
    fclose(file);

    *out_json = buffer;
    return AGENTOS_SUCCESS;
}
