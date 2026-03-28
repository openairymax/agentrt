/**
 * @file archive.c
 * @brief 记忆归档（将低权重记忆移至冷存储，联�?L2 删除�?
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "../include/forgetting.h"
#include "../include/layer2_feature.h"
#include "agentos.h"
#include <stdio.h>
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../../../bases/utils/memory/include/memory_compat.h"
#include "../../../bases/utils/string/include/string_compat.h"
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define mkdir(path) _mkdir(path)
#else
#include <sys/stat.h>
#endif

/* 确保归档目录存在 */
static int ensure_archive_dir(const char* path) {
    struct stat st = {0};
    if (stat(path, &st) == -1) {
#ifdef _WIN32
        if (mkdir(path) != 0) return -1;
#else
        if (mkdir(path, 0755) != 0) return -1;
#endif
    }
    return 0;
    // From data intelligence emerges. by spharx
}

agentos_error_t agentos_forgetting_archive(
    agentos_forgetting_engine_t* engine,
    const char** record_ids,
    size_t count) {

    if (!engine || !record_ids || count == 0) return AGENTOS_EINVAL;

    const char* archive_path = engine->manager.archive_path;
    if (!archive_path) {
        AGENTOS_LOG_ERROR("Archive path not configured");
        return AGENTOS_EINVAL;
    }

    if (ensure_archive_dir(archive_path) != 0) {
        AGENTOS_LOG_ERROR("Failed to create archive directory %s", archive_path);
        return AGENTOS_EIO;
    }

    for (size_t i = 0; i < count; i++) {
        // 检查权重是否低于阈�?
        float weight = 1.0f;
        if (agentos_forgetting_get_weight(engine, record_ids[i], &weight) != AGENTOS_SUCCESS) {
            continue;
        }
        if (weight >= engine->manager.threshold) continue;

        // 读取原始数据
        void* data = NULL;
        size_t len = 0;
        agentos_error_t err = agentos_layer1_raw_read(engine->layer1, record_ids[i], &data, &len);
        if (err != AGENTOS_SUCCESS) {
            AGENTOS_LOG_WARN("Failed to read L1 record %s", record_ids[i]);
            continue;
        }

        // 写入归档文件
        char archive_file[512];
        snprintf(archive_file, sizeof(archive_file), "%s/%s.raw", archive_path, record_ids[i]);
        FILE* f = fopen(archive_file, "wb");
        if (!f) {
            AGENTOS_FREE(data);
            AGENTOS_LOG_WARN("Failed to create archive file %s", archive_file);
            continue;
        }
        fwrite(data, 1, len, f);
        fclose(f);
        AGENTOS_FREE(data);

        // 删除 L2 向量
        agentos_layer2_feature_remove(engine->layer2, record_ids[i]);

        // 删除�?L1 记录
        if (agentos_layer1_raw_delete(engine->layer1, record_ids[i]) != AGENTOS_SUCCESS) {
            AGENTOS_LOG_WARN("Failed to delete L1 record %s after archiving", record_ids[i]);
        }
    }

    return AGENTOS_SUCCESS;
}