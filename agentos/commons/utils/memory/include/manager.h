/**
 * @file manager.h
 * @brief 向后兼容层 — 旧版 manager.h API 兼容
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 原 agentos/manager/ 目录已迁移至 ecosystem/manager/。
 * 本文件为 C 测试代码提供向后兼容的类型和函数声明。
 *
 * @owner team-A
 */

#ifndef AGENTOS_COMMONS_UTILS_MANAGER_H
#define AGENTOS_COMMONS_UTILS_MANAGER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 兼容的配置句柄类型（不透明指针）
 */
typedef struct agentos_config_s agentos_config_t;

/**
 * @brief 向后兼容：加载配置文件
 *
 * @param path 配置文件路径
 * @return 配置句柄，失败返回 NULL
 */
agentos_config_t *agentos_config_load(const char *path);

/**
 * @brief 向后兼容：释放配置资源
 *
 * @param config 配置句柄
 */
void agentos_config_free(agentos_config_t *config);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_COMMONS_UTILS_MANAGER_H */