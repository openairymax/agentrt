/**
 * @file plugin_service.h
 * @brief Plugin 守护进程服务接口
 *
 * Plugin daemon 管理动态插件的加载、卸载、生命周期和沙箱。
 * 支持的插件类型：
 *   - TOOL_PROVIDER:  工具提供者插件
 *   - PROTOCOL_ADAPTER: 协议适配器插件
 *   - MEMORY_PROVIDER:  记忆提供商插件
 *   - HOOK_EXTENSION:   Hook 扩展插件
 *
 * @owner team-A
 * @see contracts/contract_A_B.h 第3节（协议适配器 vtable）
 */

#ifndef AGENTOS_DAEMON_PLUGIN_D_PLUGIN_SERVICE_H
#define AGENTOS_DAEMON_PLUGIN_D_PLUGIN_SERVICE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── 插件类型 ── */

typedef enum {
    PLUGIN_TYPE_TOOL_PROVIDER     = 0,  /**< 工具提供者 */
    PLUGIN_TYPE_PROTOCOL_ADAPTER  = 1,  /**< 协议适配器 */
    PLUGIN_TYPE_MEMORY_PROVIDER   = 2,  /**< 记忆提供商 */
    PLUGIN_TYPE_HOOK_EXTENSION    = 3,  /**< Hook 扩展 */
    PLUGIN_TYPE_COUNT             = 4   /**< 插件类型总数 */
} plugin_type_t;

/* ── 插件状态 ── */

typedef enum {
    PLUGIN_STATE_UNLOADED  = 0,  /**< 未加载 */
    PLUGIN_STATE_LOADED    = 1,  /**< 已加载 */
    PLUGIN_STATE_INITIALIZED = 2, /**< 已初始化 */
    PLUGIN_STATE_RUNNING   = 3,  /**< 运行中 */
    PLUGIN_STATE_ERROR     = 4,  /**< 错误 */
    PLUGIN_STATE_DISABLED  = 5   /**< 已禁用 */
} plugin_state_t;

/* ── 插件元数据 ── */

typedef struct {
    char name[64];                /**< 插件名称（全局唯一） */
    char version[32];             /**< 插件版本 */
    char author[64];              /**< 作者 */
    char description[256];        /**< 描述 */
    plugin_type_t type;           /**< 插件类型 */
    uint32_t api_version;         /**< 插件 API 版本 */
    uint32_t min_agentos_version; /**< 最低 AgentRT 版本要求 */
} plugin_metadata_t;

/* ── 插件入口点 ── */

/**
 * @brief 插件初始化回调
 * @param config_path 配置文件路径
 * @param user_data   用户数据输出
 * @return 0 成功，非0失败
 */
typedef int (*plugin_init_fn)(const char *config_path, void **user_data);

/**
 * @brief 插件销毁回调
 * @param user_data 用户数据
 */
typedef void (*plugin_destroy_fn)(void *user_data);

/**
 * @brief 插件启动回调
 * @param user_data 用户数据
 * @return 0 成功，非0失败
 */
typedef int (*plugin_start_fn)(void *user_data);

/**
 * @brief 插件停止回调
 * @param user_data 用户数据
 * @return 0 成功，非0失败
 */
typedef int (*plugin_stop_fn)(void *user_data);

/* ── 插件描述符 ── */

typedef struct {
    plugin_metadata_t metadata;   /**< 元数据 */
    plugin_init_fn init;          /**< 初始化函数 */
    plugin_destroy_fn destroy;    /**< 销毁函数 */
    plugin_start_fn start;        /**< 启动函数 */
    plugin_stop_fn stop;          /**< 停止函数 */
    void *handle;                 /**< 动态库句柄（不透明） */
    void *user_data;              /**< 用户数据 */
    plugin_state_t state;         /**< 当前状态 */
    char config_path[256];        /**< 配置路径 */
    char library_path[256];       /**< 库文件路径 */
} plugin_descriptor_t;

/* ── 插件统计 ── */

typedef struct {
    uint64_t load_count;          /**< 加载次数 */
    uint64_t error_count;         /**< 错误次数 */
    uint64_t uptime_ns;           /**< 运行时间（纳秒） */
    uint64_t memory_bytes;        /**< 内存占用（字节） */
} plugin_stats_t;

/* ── 服务 API ── */

/**
 * @brief 从动态库加载插件
 * @param library_path 动态库路径
 * @param config_path  配置文件路径
 * @param out_name     输出插件名称
 * @return 0 成功，非0失败
 */
int plugin_service_load(const char *library_path, const char *config_path,
                        const char **out_name);

/**
 * @brief 卸载插件
 * @param name 插件名称
 * @return 0 成功，非0失败
 */
int plugin_service_unload(const char *name);

/**
 * @brief 启动插件
 * @param name 插件名称
 * @return 0 成功，非0失败
 */
int plugin_service_start(const char *name);

/**
 * @brief 停止插件
 * @param name 插件名称
 * @return 0 成功，非0失败
 */
int plugin_service_stop(const char *name);

/**
 * @brief 获取插件元数据
 * @param name     插件名称
 * @param metadata 输出元数据
 * @return 0 成功，非0失败
 */
int plugin_service_get_metadata(const char *name, plugin_metadata_t *metadata);

/**
 * @brief 获取插件状态
 * @param name  插件名称
 * @return 插件状态
 */
plugin_state_t plugin_service_get_state(const char *name);

/**
 * @brief 获取插件统计
 * @param name  插件名称
 * @param stats 输出统计
 * @return 0 成功，非0失败
 */
int plugin_service_get_stats(const char *name, plugin_stats_t *stats);

/**
 * @brief 列出所有已加载插件
 * @param names    输出名称数组（需调用者释放）
 * @param count    输出数量
 * @param type_filter 类型过滤（-1 表示所有类型）
 * @return 0 成功，非0失败
 */
int plugin_service_list(char ***names, size_t *count, int type_filter);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_DAEMON_PLUGIN_D_PLUGIN_SERVICE_H */
