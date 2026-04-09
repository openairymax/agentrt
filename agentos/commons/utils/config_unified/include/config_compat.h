/**
 * @file config_compat.h
 * @brief 统一配置模块 - 向后兼容层接�? * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 向后兼容层提供与现有配置API的兼容性，确保现有代码无需修改即可继续工作�? * 支持兼容的API�? * 1. agentos/commons/utils/manager.h (agentos_config_*)
 * 2. agentos/daemon/agentos/commons/include/svc_config.h (svc_config_*)
 * 3. 其他相关配置接口
 */

#ifndef AGENTOS_CONFIG_COMPAT_H
#define AGENTOS_CONFIG_COMPAT_H

#include "core_config.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 兼容模式定义 ==================== */

typedef enum {
    COMPAT_MODE_NONE = 0,          // 无兼容模�?    COMPAT_MODE_AGENTOS_CONFIG = 1, // 兼容 agentos_config_* API
    COMPAT_MODE_SVC_CONFIG = 2,    // 兼容 svc_config_* API
    COMPAT_MODE_MIXED = 3          // 混合兼容模式
} config_compat_mode_t;

/* ==================== 兼容层配�?==================== */

typedef struct {
    config_compat_mode_t mode;     // 兼容模式
    bool auto_init;                // 是否自动初始�?    bool lazy_load;                // 是否延迟加载
    const char* default_config_path; // 默认配置文件路径
    const char* env_prefix;        // 环境变量前缀
    int argc;                      // 命令行参数数�?    char** argv;                   // 命令行参数数�?} config_compat_config_t;

/* ==================== 兼容层统计信�?==================== */

typedef struct {
    size_t total_calls;           // 总调用次�?    size_t agentos_config_calls;  // agentos_config_* 调用次数
    size_t svc_config_calls;      // svc_config_* 调用次数
    size_t migration_count;       // 已迁移配置项数量
    size_t error_count;           // 错误次数
} config_compat_stats_t;

/* ==================== 兼容层初始化 ==================== */

/**
 * @brief 初始化配置兼容层
 * @param manager 兼容层配�? * @return 错误�? */
config_error_t config_compat_init(const config_compat_config_t* manager);

/**
 * @brief 销毁配置兼容层
 */
void config_compat_destroy(void);

/**
 * @brief 获取兼容层是否已初始�? * @return 是否已初始化
 */
bool config_compat_is_initialized(void);

/**
 * @brief 获取兼容层统计信�? * @param stats 统计信息输出
 */
void config_compat_get_stats(config_compat_stats_t* stats);

/**
 * @brief 重置兼容层统计信�? */
void config_compat_reset_stats(void);

/* ==================== agentos_config_* API 兼容�?==================== */

/**
 * @brief 兼容层代理：agentos_config_create
 * @return 兼容配置对象
 */
void* agentos_config_create(void);

/**
 * @brief 兼容层代理：agentos_config_destroy
 * @param manager 兼容配置对象
 */
void agentos_config_destroy(void* manager);

/**
 * @brief 兼容层代理：agentos_config_parse
 * @param manager 兼容配置对象
 * @param text 配置文本
 * @return 错误�? */
int agentos_config_parse(void* manager, const char* text);

/**
 * @brief 兼容层代理：agentos_config_load_file
 * @param manager 兼容配置对象
 * @param path 文件路径
 * @return 错误�? */
int agentos_config_load_file(void* manager, const char* path);

/**
 * @brief 兼容层代理：agentos_config_save_file
 * @param manager 兼容配置对象
 * @param path 文件路径
 * @return 错误�? */
int agentos_config_save_file(void* manager, const char* path);

/**
 * @brief 兼容层代理：agentos_config_get_string
 * @param manager 兼容配置对象
 * @param key 配置�? * @param default_value 默认�? * @return 配置�? */
const char* agentos_config_get_string(void* manager, const char* key, const char* default_value);

/**
 * @brief 兼容层代理：agentos_config_get_int
 * @param manager 兼容配置对象
 * @param key 配置�? * @param default_value 默认�? * @return 配置�? */
int agentos_config_get_int(void* manager, const char* key, int default_value);

/**
 * @brief 兼容层代理：agentos_config_get_double
 * @param manager 兼容配置对象
 * @param key 配置�? * @param default_value 默认�? * @return 配置�? */
double agentos_config_get_double(void* manager, const char* key, double default_value);

/**
 * @brief 兼容层代理：agentos_config_get_bool
 * @param manager 兼容配置对象
 * @param key 配置�? * @param default_value 默认�? * @return 配置�? */
int agentos_config_get_bool(void* manager, const char* key, int default_value);

/**
 * @brief 兼容层代理：agentos_config_set_string
 * @param manager 兼容配置对象
 * @param key 配置�? * @param value 配置�? * @return 错误�? */
int agentos_config_set_string(void* manager, const char* key, const char* value);

/**
 * @brief 兼容层代理：agentos_config_set_int
 * @param manager 兼容配置对象
 * @param key 配置�? * @param value 配置�? * @return 错误�? */
int agentos_config_set_int(void* manager, const char* key, int value);

/**
 * @brief 兼容层代理：agentos_config_set_double
 * @param manager 兼容配置对象
 * @param key 配置�? * @param value 配置�? * @return 错误�? */
int agentos_config_set_double(void* manager, const char* key, double value);

/**
 * @brief 兼容层代理：agentos_config_set_bool
 * @param manager 兼容配置对象
 * @param key 配置�? * @param value 配置�? * @return 错误�? */
int agentos_config_set_bool(void* manager, const char* key, int value);

/**
 * @brief 兼容层代理：agentos_config_remove
 * @param manager 兼容配置对象
 * @param key 配置�? * @return 错误�? */
int agentos_config_remove(void* manager, const char* key);

/**
 * @brief 兼容层代理：agentos_config_has
 * @param manager 兼容配置对象
 * @param key 配置�? * @return 是否存在
 */
int agentos_config_has(void* manager, const char* key);

/* ==================== svc_config_* API 兼容�?==================== */

/**
 * @brief 兼容层代理：svc_config_create
 * @param service_name 服务名称
 * @return 服务配置上下�? */
void* svc_config_create(const char* service_name);

/**
 * @brief 兼容层代理：svc_config_destroy
 * @param ctx 服务配置上下�? */
void svc_config_destroy(void* ctx);

/**
 * @brief 兼容层代理：svc_config_load_file
 * @param ctx 服务配置上下�? * @param file_path 文件路径
 * @return 错误�? */
int svc_config_load_file(void* ctx, const char* file_path);

/**
 * @brief 兼容层代理：svc_config_load_string
 * @param ctx 服务配置上下�? * @param yaml_content YAML内容
 * @return 错误�? */
int svc_config_load_string(void* ctx, const char* yaml_content);

/**
 * @brief 兼容层代理：svc_config_load_env
 * @param ctx 服务配置上下�? * @param prefix 环境变量前缀
 * @return 加载的配置项数量
 */
int svc_config_load_env(void* ctx, const char* prefix);

/**
 * @brief 兼容层代理：svc_config_reload
 * @param ctx 服务配置上下�? * @return 错误�? */
int svc_config_reload(void* ctx);

/**
 * @brief 兼容层代理：svc_config_save
 * @param ctx 服务配置上下�? * @param file_path 文件路径
 * @return 错误�? */
int svc_config_save(void* ctx, const char* file_path);

/**
 * @brief 兼容层代理：svc_config_get_string
 * @param ctx 服务配置上下�? * @param key 配置�? * @param default_value 默认�? * @return 配置�? */
const char* svc_config_get_string(void* ctx, const char* key, const char* default_value);

/**
 * @brief 兼容层代理：svc_config_get_int
 * @param ctx 服务配置上下�? * @param key 配置�? * @param default_value 默认�? * @return 配置�? */
int svc_config_get_int(void* ctx, const char* key, int default_value);

/**
 * @brief 兼容层代理：svc_config_get_int64
 * @param ctx 服务配置上下�? * @param key 配置�? * @param default_value 默认�? * @return 配置�? */
int64_t svc_config_get_int64(void* ctx, const char* key, int64_t default_value);

/**
 * @brief 兼容层代理：svc_config_get_float
 * @param ctx 服务配置上下�? * @param key 配置�? * @param default_value 默认�? * @return 配置�? */
double svc_config_get_float(void* ctx, const char* key, double default_value);

/**
 * @brief 兼容层代理：svc_config_get_bool
 * @param ctx 服务配置上下�? * @param key 配置�? * @param default_value 默认�? * @return 配置�? */
bool svc_config_get_bool(void* ctx, const char* key, bool default_value);

/**
 * @brief 兼容层代理：svc_config_set_string
 * @param ctx 服务配置上下�? * @param key 配置�? * @param value 配置�? * @return 错误�? */
int svc_config_set_string(void* ctx, const char* key, const char* value);

/**
 * @brief 兼容层代理：svc_config_set_int
 * @param ctx 服务配置上下�? * @param key 配置�? * @param value 配置�? * @return 错误�? */
int svc_config_set_int(void* ctx, const char* key, int value);

/**
 * @brief 兼容层代理：svc_config_set_float
 * @param ctx 服务配置上下�? * @param key 配置�? * @param value 配置�? * @return 错误�? */
int svc_config_set_float(void* ctx, const char* key, double value);

/**
 * @brief 兼容层代理：svc_config_set_bool
 * @param ctx 服务配置上下�? * @param key 配置�? * @param value 配置�? * @return 错误�? */
int svc_config_set_bool(void* ctx, const char* key, bool value);

/**
 * @brief 兼容层代理：svc_config_has
 * @param ctx 服务配置上下�? * @param key 配置�? * @return 是否存在
 */
int svc_config_has(void* ctx, const char* key);

/**
 * @brief 兼容层代理：svc_config_delete
 * @param ctx 服务配置上下�? * @param key 配置�? * @return 错误�? */
int svc_config_delete(void* ctx, const char* key);

/**
 * @brief 兼容层代理：svc_config_count
 * @param ctx 服务配置上下�? * @return 配置项数�? */
size_t svc_config_count(void* ctx);

/* ==================== 迁移辅助函数 ==================== */

/**
 * @brief 将agentos_config对象迁移到统一配置上下�? * @param agentos_config agentos配置对象
 * @param target_ctx 目标配置上下�? * @return 错误�? */
config_error_t config_compat_migrate_agentos_config(void* agentos_config, config_context_t* target_ctx);

/**
 * @brief 将svc_config对象迁移到统一配置上下�? * @param svc_config svc配置对象
 * @param target_ctx 目标配置上下�? * @return 错误�? */
config_error_t config_compat_migrate_svc_config(void* svc_config, config_context_t* target_ctx);

/**
 * @brief 从统一配置上下文创建agentos_config对象
 * @param source_ctx 源配置上下文
 * @return agentos配置对象，失败返回NULL
 */
void* config_compat_create_agentos_config_from_ctx(const config_context_t* source_ctx);

/**
 * @brief 从统一配置上下文创建svc_config对象
 * @param source_ctx 源配置上下文
 * @param service_name 服务名称
 * @return svc配置对象，失败返回NULL
 */
void* config_compat_create_svc_config_from_ctx(const config_context_t* source_ctx, const char* service_name);

/**
 * @brief 检查代码中是否使用旧配置API
 * @param file_path 文件路径
 * @param issues 问题输出缓冲�? * @param issues_size 缓冲区大�? * @return 发现的问题数�? */
size_t config_compat_check_legacy_usage(const char* file_path, char* issues, size_t issues_size);

/**
 * @brief 生成迁移报告
 * @param report 报告输出缓冲�? * @param report_size 缓冲区大�? * @return 报告长度
 */
size_t config_compat_generate_migration_report(char* report, size_t report_size);

/* ==================== 自动迁移工具 ==================== */

/**
 * @brief 自动迁移文件中的旧配置API
 * @param file_path 文件路径
 * @param backup 是否创建备份
 * @return 迁移的API数量，失败返�?1
 */
int config_compat_auto_migrate_file(const char* file_path, bool backup);

/**
 * @brief 批量迁移目录中的文件
 * @param dir_path 目录路径
 * @param pattern 文件模式（如"*.c"�? * @param recursive 是否递归
 * @param backup 是否创建备份
 * @return 迁移的文件数量，失败返回-1
 */
int config_compat_batch_migrate_files(const char* dir_path, const char* pattern, bool recursive, bool backup);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_CONFIG_COMPAT_H */
