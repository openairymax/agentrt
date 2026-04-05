/**
 * @file revive.c
 * @brief 记忆复活（从归档恢复�?
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "../include/forgetting.h"
#include "../include/layer2_feature.h"
#include "agentos.h"
#include <stdio.h>
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../../../agentos/commons/utils/memory/include/memory_compat.h"
#include "../../../agentos/commons/utils/string/include/string_compat.h"
#include <string.h>

agentos_error_t agentos_forgetting_revive(
    agentos_forgetting_engine_t* engine,
    const char* record_id,
    void** out_data,
    size_t* out_len) {

    if (!engine || !record_id || !out_data || !out_len) return AGENTOS_EINVAL;

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
    char* new_id = NULL;
    agentos_error_t err = agentos_layer1_raw_write(engine->layer1, data, size, NULL, &new_id);
    if (err != AGENTOS_SUCCESS) {
        AGENTOS_FREE(data);
        return err;
    }
    AGENTOS_FREE(new_id);

    // 删除归档文件
    remove(archive_file);

    *out_data = data;
    *out_len = size;
    return AGENTOS_SUCCESS;
}
