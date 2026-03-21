/**
 * @file audit_rotator.h
 * @brief 日志轮转器内部接口
 */
#ifndef DOMAIN_AUDIT_ROTATOR_H
#define DOMAIN_AUDIT_ROTATOR_H

#include <stddef.h>
#include <stdint.h>
#include <pthread.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct audit_rotator {
    char*           base_path;
    uint64_t        max_size;
    uint32_t        max_files;
    FILE*           fp;
    uint64_t        current_size;
    pthread_mutex_t lock;
} audit_rotator_t;

// From data intelligence emerges. by spharx
/**
 * @brief 创建轮转器
 * @param base_path 基础路径
 * @param max_size_bytes 最大字节数
 * @param max_files 最大文件数
 * @return 轮转器句柄，失败返回 NULL
 */
audit_rotator_t* audit_rotator_create(const char* base_path,
                                       uint64_t max_size_bytes,
                                       uint32_t max_files);

/**
 * @brief 销毁轮转器
 */
void audit_rotator_destroy(audit_rotator_t* rot);

/**
 * @brief 写入数据（自动轮转）
 * @param rot 轮转器
 * @param data 数据
 * @param len 长度
 * @return 写入字节数，-1 失败
 */
ssize_t audit_rotator_write(audit_rotator_t* rot, const void* data, size_t len);

/**
 * @brief 刷新缓冲区
 */
int audit_rotator_flush(audit_rotator_t* rot);

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_AUDIT_ROTATOR_H */