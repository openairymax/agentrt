// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file config_manager.h
 * @brief 统一配置管理系统
 *
 * 提供跨守护进程的统一配置管理，支持：
 * - 多格式配置（JSON/YAML/TOML/ENV）
 * - 配置热更新（文件监视+回调通知）
 * - 配置版本控制（变更历史+回滚）
 * - 环境差异化配置（dev/staging/prod）
 * - 配置校验与默认值
 * - 跨进程配置同步（基于共享内存）
 *
 * @see svc_common.h 服务管理框架
 */

#ifndef AGENTOS_CONFIG_MANAGER_H
#define AGENTOS_CONFIG_MANAGER_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 常量定义 ==================== */

#define CM_MAX_KEY_LEN          128
#define CM_MAX_VALUE_LEN        2048
#define CM_MAX_ENTRIES          512
#define CM_MAX_WATCHERS         32
#define CM_MAX_HISTORY          64
#define CM_MAX_NAMESPACE_LEN    32
#define CM_MAX_PATH_LEN         512

/* ==================== 配置值类型 ==================== */

typedef enum {
    CM_TYPE_STRING  = 0,
    CM_TYPE_INT     = 1,
    CM_TYPE_DOUBLE  = 2,
    CM_TYPE_BOOL    = 3,
    CM_TYPE_NULL    = 4
} cm_value_type_t;

/* ==================== 配置条目 ==================== */

typedef struct {
    char key[CM_MAX_KEY_LEN];
    char value[CM_MAX_VALUE_LEN];
    cm_value_type_t type;
    char namespace_[CM_MAX_NAMESPACE_LEN];
    uint64_t version;
    uint64_t last_modified;
    bool is_default;
    bool is_overridden;
    char source[64];
} cm_entry_t;

/* ==================== 配置变更记录 ==================== */

typedef struct {
    char key[CM_MAX_KEY_LEN];
    char old_value[CM_MAX_VALUE_LEN];
    char new_value[CM_MAX_VALUE_LEN];
    uint64_t timestamp;
    char source[64];
} cm_change_record_t;

/* ==================== 配置管理器配置 ==================== */

typedef struct {
    char base_path[CM_MAX_PATH_LEN];
    char environment[32];
    uint32_t watch_interval_ms;
    uint32_t max_history;
    bool enable_hot_reload;
    bool enable_validation;
    bool enable_cross_process_sync;
    char sync_shm_name[256];
} cm_config_t;

/* ==================== 配置变更回调 ==================== */

typedef void (*cm_change_callback_t)(
    const char* key,
    const char* old_value,
    const char* new_value,
    void* user_data
);

/* ==================== 配置校验回调 ==================== */

typedef bool (*cm_validator_t)(
    const char* key,
    const char* value,
    char* error_msg,
    size_t error_msg_size
);

/* ==================== 生命周期管理 ==================== */

/**
 * @brief 初始化配置管理器
 * @param config 配置参数（NULL使用默认）
 * @return 0成功，非0失败
 */
int cm_init(const cm_config_t* config);

/**
 * @brief 关闭配置管理器
 */
void cm_shutdown(void);

/* ==================== 配置读写 ==================== */

/**
 * @brief 获取配置值
 * @param key 配置键（格式：namespace.key 或 key）
 * @param default_value 默认值（配置不存在时返回）
 * @return 配置值字符串
 */
const char* cm_get(const char* key, const char* default_value);

/**
 * @brief 获取整数配置值
 * @param key 配置键
 * @param default_value 默认值
 * @return 配置值
 */
int64_t cm_get_int(const char* key, int64_t default_value);

/**
 * @brief 获取浮点配置值
 * @param key 配置键
 * @param default_value 默认值
 * @return 配置值
 */
double cm_get_double(const char* key, double default_value);

/**
 * @brief 获取布尔配置值
 * @param key 配置键
 * @param default_value 默认值
 * @return 配置值
 */
bool cm_get_bool(const char* key, bool default_value);

/**
 * @brief 设置配置值
 * @param key 配置键
 * @param value 配置值
 * @param source 来源标识
 * @return 0成功，非0失败
 */
int cm_set(const char* key, const char* value, const char* source);

/**
 * @brief 设置带命名空间的配置值
 * @param namespace_ 命名空间
 * @param key 配置键
 * @param value 配置值
 * @param source 来源标识
 * @return 0成功，非0失败
 */
int cm_set_namespaced(const char* namespace_, const char* key,
                      const char* value, const char* source);

/* ==================== 配置加载 ==================== */

/**
 * @brief 从JSON文件加载配置
 * @param path 文件路径
 * @param namespace_ 命名空间（NULL使用默认）
 * @return 0成功，非0失败
 */
int cm_load_json(const char* path, const char* namespace_);

/**
 * @brief 从环境变量加载配置
 * @param prefix 环境变量前缀（如"AGENTOS_"）
 * @param namespace_ 命名空间
 * @return 加载的配置项数量
 */
int cm_load_env(const char* prefix, const char* namespace_);

/**
 * @brief 从命令行参数加载配置
 * @param argc 参数数量
 * @param argv 参数数组
 * @return 加载的配置项数量
 */
int cm_load_args(int argc, char** argv);

/* ==================== 配置监视与热更新 ==================== */

/**
 * @brief 注册配置变更回调
 * @param key_pattern 键模式（支持通配符*，NULL监视所有）
 * @param callback 回调函数
 * @param user_data 用户数据
 * @return 0成功，非0失败
 */
int cm_watch(const char* key_pattern, cm_change_callback_t callback,
             void* user_data);

/**
 * @brief 取消配置监视
 * @param key_pattern 键模式
 * @param callback 回调函数
 * @return 0成功，非0失败
 */
int cm_unwatch(const char* key_pattern, cm_change_callback_t callback);

/**
 * @brief 手动触发配置重新加载
 * @return 0成功，非0失败
 */
int cm_reload(void);

/* ==================== 配置校验 ==================== */

/**
 * @brief 注册配置校验器
 * @param key_pattern 键模式
 * @param validator 校验函数
 * @return 0成功，非0失败
 */
int cm_register_validator(const char* key_pattern, cm_validator_t validator);

/**
 * @brief 校验所有配置
 * @return 校验失败的配置数量
 */
int cm_validate_all(void);

/* ==================== 版本控制 ==================== */

/**
 * @brief 获取配置变更历史
 * @param key 配置键（NULL表示所有）
 * @param records [out] 变更记录数组
 * @param max_count 数组最大容量
 * @param found_count [out] 实际数量
 * @return 0成功，非0失败
 */
int cm_get_history(const char* key, cm_change_record_t* records,
                   uint32_t max_count, uint32_t* found_count);

/**
 * @brief 回滚配置到指定版本
 * @param key 配置键
 * @param version 目标版本（0表示上一版本）
 * @return 0成功，非0失败
 */
int cm_rollback(const char* key, uint64_t version);

/* ==================== 环境差异化 ==================== */

/**
 * @brief 获取当前环境
 * @return 环境名称
 */
const char* cm_get_environment(void);

/**
 * @brief 设置环境
 * @param env 环境名称（dev/staging/prod）
 * @return 0成功，非0失败
 */
int cm_set_environment(const char* env);

/**
 * @brief 加载环境特定配置
 * @param env 环境名称
 * @return 加载的配置项数量
 */
int cm_load_environment_config(const char* env);

/* ==================== 导出 ==================== */

/**
 * @brief 导出配置为JSON字符串
 * @param namespace_ 命名空间（NULL导出全部）
 * @return JSON字符串（需调用者释放），失败返回NULL
 */
char* cm_export_json(const char* namespace_);

/**
 * @brief 获取配置条目数量
 * @return 条目数量
 */
uint32_t cm_entry_count(void);

/**
 * @brief 创建默认配置
 * @return 默认配置
 */
cm_config_t cm_create_default_config(void);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_CONFIG_MANAGER_H */
