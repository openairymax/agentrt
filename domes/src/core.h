/**
 * @file core.h
 * @brief domes 模块核心内部接口
 * @author Spharx
 * @date 2024
 */

#ifndef DOMES_CORE_H
#define DOMES_CORE_H

#include "platform/platform.h"
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* domes 实例结构 */
typedef struct domes_instance {
    struct permission_engine* permission;
    struct audit_logger* audit;
    struct sanitizer* sanitizer;
    domes_rwlock_t lock;
    domes_atomic32_t ref_count;
    bool initialized;
    char* config_path;
    char* log_dir;
} domes_instance_t;

/* 全局实例 */
extern domes_instance_t* g_domes_instance;

/* 内部初始化函数 */
int domes_init_internal(const char* config_path);
void domes_cleanup_internal(void);

#ifdef __cplusplus
}
#endif

#endif /* DOMES_CORE_H */
