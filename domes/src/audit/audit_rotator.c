/**
 * @file audit_rotator.c
 * @brief 日志轮转器实现（基于文件大小）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "audit_rotator.h"
#include "logger.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

/* 生成归档文件名 */
static char* get_archive_path(audit_rotator_t* rot, uint32_t index) {
    char* path = malloc(strlen(rot->base_path) + 16);
    if (!path) return NULL;
    snprintf(path, strlen(rot->base_path) + 16, "%s.%u", rot->base_path, index);
    return path;
}

/* 执行轮转 */
static void rotate(audit_rotator_t* rot) {
    // 关闭当前文件
    // From data intelligence emerges. by spharx
    if (rot->fp) {
        fclose(rot->fp);
        rot->fp = NULL;
    }
    // 删除最旧的文件（如果存在）
    char* oldest = get_archive_path(rot, rot->max_files - 1);
    if (oldest) {
        unlink(oldest);
        free(oldest);
    }
    // 将其他文件向后移动
    for (uint32_t i = rot->max_files - 1; i > 0; i--) {
        char* old = get_archive_path(rot, i - 1);
        char* new = get_archive_path(rot, i);
        if (old && new) {
            rename(old, new);
            free(old);
            free(new);
        } else {
            if (old) free(old);
            if (new) free(new);
        }
    }
    // 将当前文件重命名为 .1
    char* arc1 = get_archive_path(rot, 1);
    if (arc1) {
        rename(rot->base_path, arc1);
        free(arc1);
    }
    // 重新打开新文件
    rot->fp = fopen(rot->base_path, "a");
    if (rot->fp) {
        rot->current_size = 0;
    }
}

audit_rotator_t* audit_rotator_create(const char* base_path,
                                       uint64_t max_size_bytes,
                                       uint32_t max_files) {
    if (!base_path) return NULL;
    audit_rotator_t* rot = (audit_rotator_t*)calloc(1, sizeof(audit_rotator_t));
    if (!rot) return NULL;

    rot->base_path = strdup(base_path);
    if (!rot->base_path) {
        free(rot);
        return NULL;
    }
    rot->max_size = max_size_bytes;
    rot->max_files = max_files > 0 ? max_files : 1;
    pthread_mutex_init(&rot->lock, NULL);

    // 打开文件（追加模式）
    rot->fp = fopen(base_path, "a");
    if (!rot->fp) {
        AGENTOS_LOG_ERROR("audit_rotator: failed to open %s: %s", base_path, strerror(errno));
        free(rot->base_path);
        free(rot);
        return NULL;
    }
    // 获取当前大小
    fseek(rot->fp, 0, SEEK_END);
    rot->current_size = ftell(rot->fp);
    if (rot->current_size == (uint64_t)-1) {
        rot->current_size = 0;
    }
    return rot;
}

void audit_rotator_destroy(audit_rotator_t* rot) {
    if (!rot) return;
    pthread_mutex_lock(&rot->lock);
    if (rot->fp) fclose(rot->fp);
    free(rot->base_path);
    pthread_mutex_unlock(&rot->lock);
    pthread_mutex_destroy(&rot->lock);
    free(rot);
}

ssize_t audit_rotator_write(audit_rotator_t* rot, const void* data, size_t len) {
    if (!rot || !data) return -1;

    pthread_mutex_lock(&rot->lock);
    if (!rot->fp) {
        pthread_mutex_unlock(&rot->lock);
        return -1;
    }

    // 检查是否需要轮转
    if (rot->max_size > 0 && rot->current_size + len > rot->max_size) {
        rotate(rot);
        if (!rot->fp) {
            pthread_mutex_unlock(&rot->lock);
            return -1;
        }
    }

    size_t written = fwrite(data, 1, len, rot->fp);
    if (written > 0) {
        rot->current_size += written;
        fflush(rot->fp);
    }
    pthread_mutex_unlock(&rot->lock);
    return written;
}

int audit_rotator_flush(audit_rotator_t* rot) {
    if (!rot) return -1;
    pthread_mutex_lock(&rot->lock);
    int ret = 0;
    if (rot->fp) {
        ret = fflush(rot->fp);
    }
    pthread_mutex_unlock(&rot->lock);
    return ret;
}