/**
 * @file market_service.h
 * @brief 市场服务接口定义
 * @details 负责 Agent 和 Skill 的注册、发现、安装和管理
 * @copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
 */

#ifndef AGENTOS_MARKET_SERVICE_H
#define AGENTOS_MARKET_SERVICE_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief 市场服务配置
 */
typedef struct {
    char* registry_url;              /**< 注册中心 URL */
    char* storage_path;              /**< 存储路径 */
    uint32_t sync_interval_ms;       /**< 同步间隔（毫秒） */
    uint32_t cache_ttl_ms;           /**< 缓存过期时间（毫秒） */
    bool enable_remote_registry;     /**< 是否启用远程注册中心 */
    bool enable_auto_update;         /**< 是否启用自动更新 */
} market_config_t;

/**
 * @brief Agent 类型
 */
typedef enum {
    AGENT_TYPE_ASSISTANT,            /**< 助手型 Agent */
    AGENT_TYPE_EXPERT,               /**< 专家型 Agent */
    AGENT_TYPE_SPECIALIZED,          /**< 专业型 Agent */
    AGENT_TYPE_CUSTOM,               /**< 自定义 Agent */
    AGENT_TYPE_COUNT
} agent_type_t;

/**
 * @brief Agent 状态
 */
typedef enum {
    AGENT_STATUS_AVAILABLE,          /**< 可用 */
    AGENT_STATUS_INSTALLING,         /**< 安装中 */
    AGENT_STATUS_ERROR,              /**< 错误 */
    AGENT_STATUS_DISABLED,           /**< 禁用 */
    AGENT_STATUS_COUNT
} agent_status_t;

/**
 * @brief Skill 类型
 */
typedef enum {
    SKILL_TYPE_TOOL,                 /**< 工具型 Skill */
    SKILL_TYPE_KNOWLEDGE,            /**< 知识型 Skill */
    SKILL_TYPE_INTEGRATION,          /**< 集成型 Skill */
    SKILL_TYPE_CUSTOM,               /**< 自定义 Skill */
    SKILL_TYPE_COUNT
} skill_type_t;

/**
 * @brief Agent 信息
 */
typedef struct {
    char* agent_id;                  /**< Agent ID */
    char* name;                      /**< Agent 名称 */
    char* version;                   /**< 版本 */
    char* description;               /**< 描述 */
    agent_type_t type;               /**< Agent 类型 */
    agent_status_t status;           /**< Agent 状态 */
    char* author;                    /**< 作者 */
    char* repository;                /**< 仓库地址 */
    char* dependencies;              /**< 依赖项 */
    float rating;                    /**< 评分 */
    uint32_t download_count;         /**< 下载次数 */
    uint64_t last_updated;           /**< 最后更新时间 */
} agent_info_t;

/**
 * @brief Skill 信息
 */
typedef struct {
    char* skill_id;                  /**< Skill ID */
    char* name;                      /**< Skill 名称 */
    char* version;                   /**< 版本 */
    char* description;               /**< 描述 */
    skill_type_t type;               /**< Skill 类型 */
    char* author;                    /**< 作者 */
    char* repository;                /**< 仓库地址 */
    char* dependencies;              /**< 依赖项 */
    float rating;                    /**< 评分 */
    uint32_t download_count;         /**< 下载次数 */
    uint64_t last_updated;           /**< 最后更新时间 */
} skill_info_t;

/**
 * @brief 安装请求
 */
typedef struct {
    char* id;                        /**< Agent 或 Skill ID */
    char* version;                   /**< 版本（可选，为空表示最新版本） */
    bool force_update;               /**< 是否强制更新 */
    char* install_path;              /**< 安装路径（可选） */
} install_request_t;

/**
 * @brief 安装结果
 */
typedef struct {
    bool success;                    /**< 是否成功 */
    char* message;                   /**< 消息 */
    char* installed_version;         /**< 安装的版本 */
    char* install_path;              /**< 安装路径 */
    int error_code;                  /**< 错误码 */
} install_result_t;

/**
 * @brief 搜索参数
 */
typedef struct {
    char* query;                     /**< 搜索关键词 */
    agent_type_t agent_type;         /**< Agent 类型（仅搜索 Agent 时使用） */
    skill_type_t skill_type;         /**< Skill 类型（仅搜索 Skill 时使用） */
    bool only_installed;             /**< 仅显示已安装的 */
    bool sort_by_rating;             /**< 按评分排序 */
    bool sort_by_download;           /**< 按下载量排序 */
    size_t limit;                    /**< 结果数量限制 */
    size_t offset;                   /**< 结果偏移量 */
} search_params_t;

/**
 * @brief 市场服务句柄
 */
typedef struct market_service market_service_t;

/**
 * @brief 创建市场服务
 * @param config 配置信息
 * @param service 输出参数，返回创建的服务句柄
 * @return 0 表示成功，非 0 表示错误码
 */
int market_service_create(const market_config_t* config, market_service_t** service);

/**
 * @brief 销毁市场服务
 * @param service 服务句柄
 * @return 0 表示成功，非 0 表示错误码
 */
int market_service_destroy(market_service_t* service);

/**
 * @brief 注册 Agent
 * @param service 服务句柄
 * @param agent_info Agent 信息
 * @return 0 表示成功，非 0 表示错误码
 */
int market_service_register_agent(market_service_t* service, const agent_info_t* agent_info);

/**
 * @brief 注册 Skill
 * @param service 服务句柄
 * @param skill_info Skill 信息
 * @return 0 表示成功，非 0 表示错误码
 */
int market_service_register_skill(market_service_t* service, const skill_info_t* skill_info);

/**
 * @brief 搜索 Agent
 * @param service 服务句柄
 * @param params 搜索参数
 * @param agents 输出参数，返回 Agent 信息数组
 * @param count 输出参数，返回 Agent 数量
 * @return 0 表示成功，非 0 表示错误码
 */
int market_service_search_agents(market_service_t* service, const search_params_t* params, agent_info_t*** agents, size_t* count);

/**
 * @brief 搜索 Skill
 * @param service 服务句柄
 * @param params 搜索参数
 * @param skills 输出参数，返回 Skill 信息数组
 * @param count 输出参数，返回 Skill 数量
 * @return 0 表示成功，非 0 表示错误码
 */
int market_service_search_skills(market_service_t* service, const search_params_t* params, skill_info_t*** skills, size_t* count);

/**
 * @brief 安装 Agent
 * @param service 服务句柄
 * @param request 安装请求
 * @param result 输出参数，返回安装结果
 * @return 0 表示成功，非 0 表示错误码
 */
int market_service_install_agent(market_service_t* service, const install_request_t* request, install_result_t** result);

/**
 * @brief 安装 Skill
 * @param service 服务句柄
 * @param request 安装请求
 * @param result 输出参数，返回安装结果
 * @return 0 表示成功，非 0 表示错误码
 */
int market_service_install_skill(market_service_t* service, const install_request_t* request, install_result_t** result);

/**
 * @brief 卸载 Agent
 * @param service 服务句柄
 * @param agent_id Agent ID
 * @return 0 表示成功，非 0 表示错误码
 */
int market_service_uninstall_agent(market_service_t* service, const char* agent_id);

/**
 * @brief 卸载 Skill
 * @param service 服务句柄
 * @param skill_id Skill ID
 * @return 0 表示成功，非 0 表示错误码
 */
int market_service_uninstall_skill(market_service_t* service, const char* skill_id);

/**
 * @brief 获取已安装的 Agent 列表
 * @param service 服务句柄
 * @param agents 输出参数，返回 Agent 信息数组
 * @param count 输出参数，返回 Agent 数量
 * @return 0 表示成功，非 0 表示错误码
 */
int market_service_get_installed_agents(market_service_t* service, agent_info_t*** agents, size_t* count);

/**
 * @brief 获取已安装的 Skill 列表
 * @param service 服务句柄
 * @param skills 输出参数，返回 Skill 信息数组
 * @param count 输出参数，返回 Skill 数量
 * @return 0 表示成功，非 0 表示错误码
 */
int market_service_get_installed_skills(market_service_t* service, skill_info_t*** skills, size_t* count);

/**
 * @brief 检查更新
 * @param service 服务句柄
 * @param id Agent 或 Skill ID
 * @param has_update 输出参数，返回是否有更新
 * @param latest_version 输出参数，返回最新版本
 * @return 0 表示成功，非 0 表示错误码
 */
int market_service_check_update(market_service_t* service, const char* id, bool* has_update, char** latest_version);

/**
 * @brief 重载配置
 * @param service 服务句柄
 * @param config 新的配置信息
 * @return 0 表示成功，非 0 表示错误码
 */
int market_service_reload_config(market_service_t* service, const market_config_t* config);

/**
 * @brief 同步注册中心
 * @param service 服务句柄
 * @return 0 表示成功，非 0 表示错误码
 */
int market_service_sync_registry(market_service_t* service);

#endif /* AGENTOS_MARKET_SERVICE_H */
