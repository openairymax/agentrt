/**
 * @file uuid_generator.h
 * @brief UUID v4 生成器接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTRT_UUID_GENERATOR_H
#define AGENTRT_UUID_GENERATOR_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief UUID 格式
 */
#define AGENTRT_UUID_STR_LEN 37
#define AGENTRT_UUID_PREFIXED_STR_LEN 64

/**
 * @brief UUID 生成错误码
 */
typedef enum agentrt_uuid_error {
    AGENTRT_UUID_SUCCESS = 0,
    AGENTRT_UUID_EINVALID = -1,
    AGENTRT_UUID_ENOMEM = -2,
    AGENTRT_UUID_EUNAVAIL = -3
} agentrt_uuid_error_t;

/**
 * @brief 初始化 UUID 生成器
 * @return 成功返回 AGENTRT_UUID_SUCCESS
 */
agentrt_uuid_error_t agentrt_uuid_init(void);

/**
 * @brief 清理 UUID 生成器
 */
void agentrt_uuid_cleanup(void);

/**
 * @brief 生成标准 UUID v4 字符串
 * @param out_buf 输出缓冲区（至少 AGENTRT_UUID_STR_LEN 字节）
 * @param buf_len 缓冲区长度
 * @return 成功返回 AGENTRT_UUID_SUCCESS
 */
agentrt_uuid_error_t agentrt_uuid_v4(char *out_buf, size_t buf_len);

/**
 * @brief 生成带前缀的 UUID
 * @param prefix 前缀字符串（如 "mem_"、"task_"）
 * @param out_buf 输出缓冲区（至少 AGENTRT_UUID_PREFIXED_STR_LEN 字节）
 * @param buf_len 缓冲区长度
 * @return 成功返回 AGENTRT_UUID_SUCCESS
 */
agentrt_uuid_error_t agentrt_uuid_with_prefix(const char *prefix, char *out_buf, size_t buf_len);

/**
 * @brief 验证 UUID 格式是否有效
 * @param uuid UUID 字符串
 * @return 有效返回 1，无效返回 0
 */
int agentrt_uuid_is_valid(const char *uuid);

/**
 * @brief 将原始 UUID 二进制转换为字符串
 * @param uuid_bin 16 字节原始 UUID
 * @param out_buf 输出缓冲区（至少 AGENTRT_UUID_STR_LEN 字节）
 * @param buf_len 缓冲区长度
 * @return 成功返回 AGENTRT_UUID_SUCCESS
 */
agentrt_uuid_error_t agentrt_uuid_bin_to_str(const uint8_t *uuid_bin, char *out_buf,
                                             size_t buf_len);

/**
 * @brief 将 UUID 字符串转换为原始二进制
 * @param uuid_str UUID 字符串
 * @param out_bin 输出缓冲区（至少 16 字节）
 * @return 成功返回 AGENTRT_UUID_SUCCESS
 */
agentrt_uuid_error_t agentrt_uuid_str_to_bin(const char *uuid_str, uint8_t *out_bin);

#ifdef __cplusplus
}
#endif

#endif /* AGENTRT_UUID_GENERATOR_H */
