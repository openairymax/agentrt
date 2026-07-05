/**
 * @file plugin_discovery.h
 * @brief P2.2.1: 插件发现 — 扫描目录 + 解析 manifest.yaml
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 扫描 ecosystem/plugins/ 目录，发现所有插件。
 * 每个插件目录下必须包含 manifest.yaml 描述文件。
 *
 * manifest.yaml 格式：
 * @code
 *   name: my_plugin
 *   version: 1.0.0
 *   author: SPHARX
 *   description: My plugin description
 *   type: tool_provider
 *   api_version: 1
 *   min_agentrt_version: 0.1.1
 *   library: libmy_plugin.so
 *   permissions:
 *     - file_read
 *     - network_outbound
 *     - tool_execute
 *   config:
 *     timeout_ms: 5000
 * @endcode
 */

#ifndef AGENTRT_PLUGIN_DISCOVERY_H
#define AGENTRT_PLUGIN_DISCOVERY_H

#include "plugin_service.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 常量 ==================== */

#define PLUGIN_DISCOVERY_MAX_PLUGINS  128     /**< 最大发现插件数 */
#define PLUGIN_DISCOVERY_MAX_PATH     512     /**< 最大路径长度 */
#define PLUGIN_DISCOVERY_MAX_PERMISSIONS 32   /**< 最大权限声明数 */

/* ==================== 发现结果 ==================== */

typedef struct {
    char name[64];                          /**< 插件名称 */
    char version[32];                       /**< 插件版本 */
    char author[64];                        /**< 作者 */
    char description[256];                  /**< 描述 */
    plugin_type_t type;                     /**< 插件类型 */
    uint32_t api_version;                   /**< API 版本 */
    uint32_t min_agentrt_version;           /**< 最低版本要求 */
    char library_path[PLUGIN_DISCOVERY_MAX_PATH];  /**< 动态库路径 */
    char config_path[PLUGIN_DISCOVERY_MAX_PATH];   /**< 配置文件路径 */
    char permissions[PLUGIN_DISCOVERY_MAX_PERMISSIONS][64]; /**< 权限声明 */
    uint32_t permission_count;              /**< 权限数量 */
    bool valid;                             /**< 是否有效 */
    char error_reason[256];                 /**< 无效原因 */
} plugin_discovery_result_t;

/* ==================== 发现配置 ==================== */

typedef struct {
    const char *plugins_dir;                /**< 插件目录，默认 "ecosystem/plugins/" */
    bool auto_load;                         /**< 是否自动加载发现的插件 */
    bool fail_on_invalid;                   /**< 无效插件是否导致失败 */
    uint32_t scan_depth;                    /**< 扫描深度，默认 1 */
} plugin_discovery_config_t;

/* ==================== 发现 API ==================== */

/**
 * @brief 初始化插件发现模块
 *
 * @param config 配置（NULL 使用默认）
 * @return 0 成功，非0 失败
 */
int plugin_discovery_init(const plugin_discovery_config_t *config);

/**
 * @brief 销毁插件发现模块
 */
void plugin_discovery_destroy(void);

/**
 * @brief 扫描插件目录，发现所有可用插件
 *
 * 扫描 plugins_dir 下的每个子目录，查找 manifest.yaml。
 * 解析后返回所有发现的插件元数据。
 *
 * @param out_results 输出发现结果数组（需调用者释放）
 * @param out_count   输出数量
 * @return 0 成功，非0 失败
 */
int plugin_discovery_scan(plugin_discovery_result_t **out_results,
                          size_t *out_count);

/**
 * @brief 解析单个 manifest.yaml 文件
 *
 * @param yaml_path   manifest.yaml 文件路径
 * @param plugin_dir  插件目录（用于解析相对路径）
 * @param out_result  输出解析结果
 * @return 0 成功，非0 失败
 */
int plugin_discovery_parse_manifest(const char *yaml_path,
                                    const char *plugin_dir,
                                    plugin_discovery_result_t *out_result);

/**
 * @brief 自动加载所有发现的插件
 *
 * 调用 plugin_discovery_scan() 发现所有插件，
 * 然后对每个有效插件调用 plugin_service_load()。
 *
 * @return 0 成功，非0 失败
 */
int plugin_discovery_auto_load(void);

/**
 * @brief 获取发现的插件数量
 *
 * @return 插件数量
 */
size_t plugin_discovery_count(void);

/**
 * @brief 释放发现结果
 *
 * @param results 结果数组
 * @param count   数量
 */
void plugin_discovery_free_results(plugin_discovery_result_t *results,
                                   size_t count);

#ifdef __cplusplus
}
#endif

#endif /* AGENTRT_PLUGIN_DISCOVERY_H */