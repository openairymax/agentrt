/**
 * @file revive.c
 * @brief 记忆复活（从归档恢复�?
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "../include/forgetting.h"
#include "../include/layer2_feature.h"
#include "../include/layer1_raw.h"
#include "agentos.h"
#include <stdio.h>
#include <stdlib.h>

/* Unified base library compatibility layer */
#include <agentos/utils/memory/memory_compat.h>
#include <agentos/utils/string/string_compat.h>
#include <string.h>

static int is_path_component_safe(const char* id) {
    if (!id || !*id) return 0;
    for (const char* p = id; *p; p++) {
        if (*p == '/' || *p == '\\' || *p == ':' || *p == '*' ||
            *p == '?' || *p == '"' || *p == '<' || *p == '>' || *p == '|') {
            return 0;
        }
    }
    if (strstr(id, "..") != NULL) return 0;
    return 1;
}

agentos_error_t agentos_forgetting_revive(
    agentos_forgetting_engine_t* engine,
    const char* record_id,
    void** out_data,
    size_t* out_len) {

    if (!engine || !record_id || !out_data || !out_len) return AGENTOS_EINVAL;
    if (!is_path_component_safe(record_id)) return AGENTOS_EINVAL;

    const char* archive_path = engine->manager.archive_path;
    if (!archive_path) {
        AGENTOS_LOG_ERROR("Archive path not configured");
        return AGENTOS_EINVAL;
    }

    char archive_file[512];
    snprintf(archive_file, sizeof(archive_file), "%s/%s.raw", archive_path, record_id);

    FILE* f = fopen(archive_file, "rb");
    if (!f) return AGENTOS_ENOENT;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size < 0) {
        fclose(f);
        return AGENTOS_EIO;
    }

    void* data = AGENTOS_MALLOC(size);
    if (!data) {
        fclose(f);
        return AGENTOS_ENOMEM;
    }

    size_t read = fread(data, 1, size, f);
    fclose(f);
    if (read != (size_t)size) {
        AGENTOS_FREE(data);
        return AGENTOS_EIO;
    }

    // 重新写入 L1
    agentos_error_t err = agentos_layer1_raw_write(engine->layer1, record_id, data, (size_t)size);
    if (err != AGENTOS_SUCCESS) {
        AGENTOS_FREE(data);
        return err;
    }

    // 删除归档文件
    if (remove(archive_file) != 0) {
        AGENTOS_LOG_WARN("Failed to remove archive file: %s", archive_file);
    }

    *out_data = data;
    *out_len = size;
    return AGENTOS_SUCCESS;
}
