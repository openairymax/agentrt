// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
/**
 * @file agentos_frameworks.h
 * @brief Airymax 外部 AI 框架适配器层 — 行业底座兼容驱动接口
 *
 * @details
 * 本模块是 Airymax 作为"行业底座"连接外部 AI 框架的桥梁。通过标准适配器接口，
 * Airymax 可兼容驱动行业内所有其他 AI 框架（如 LangChain、AutoGPT、CrewAI、
 * Semantic Kernel、LlamaIndex 等），使它们接入 Airymax 的 MicroCoreRT 内核、
 * CoreLoopThree 认知循环、MemoryRovol 记忆卷载、Cupolas 安全穹顶等基础设施。
 *
 * 设计原则（微内核思想）：
 * - atoms 内核层仅提供适配器注册表和生命周期管理（薄层）
 * - 实际框架逻辑由外部适配器实现（通过函数指针回调）
 * - 适配器注册/注销/查找/调用全部线程安全
 * - 不持有外部框架的内部状态，仅持有不透明 handle
 *
 * @author SPHARX Ltd. - Airymax Team
 * @date 2026-06-29
 * @version 3.0  重新设计为外部框架桥接层
 */

#ifndef AGENTOS_FRAMEWORKS_H
#define AGENTOS_FRAMEWORKS_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 常量 ==================== */

#define AGENTOS_FW_MAX_NAME_LEN 64        /*!< 框架名最大长度 */
#define AGENTOS_FW_MAX_VERSION_LEN 16     /*!< 版本号最大长度 */
#define AGENTOS_FW_MAX_ADAPTERS 32        /*!< 注册表最大容量 */
#define AGENTOS_FW_MAX_REQUEST_PAYLOAD 4096 /*!< 请求载荷最大长度 */

/* ==================== 错误码 ==================== */

#define AGENTOS_FW_OK 0
#define AGENTOS_FW_ERROR (-1)
#define AGENTOS_FW_NOT_INIT (-2)
#define AGENTOS_FW_INVALID_ARG (-3)
#define AGENTOS_FW_TIMEOUT (-4)
#define AGENTOS_FW_BUSY (-5)
#define AGENTOS_FW_NOT_FOUND (-6)
#define AGENTOS_FW_DENIED (-7)
#define AGENTOS_FW_ALREADY_EXISTS (-8)
#define AGENTOS_FW_CAPACITY_FULL (-9)

/* ==================== 能力位掩码 ==================== */

typedef enum {
    AGENTOS_FW_CAP_NONE        = 0,
    AGENTOS_FW_CAP_COGNITION   = 1 << 0,  /*!< 认知/推理能力 */
    AGENTOS_FW_CAP_EXECUTION   = 1 << 1,  /*!< 代码/工具执行 */
    AGENTOS_FW_CAP_MEMORY      = 1 << 2,  /*!< 记忆存储/检索 */
    AGENTOS_FW_CAP_PLANNING    = 1 << 3,  /*!< 任务规划 */
    AGENTOS_FW_CAP_PROTOCOL    = 1 << 4,  /*!< 协议支持（MCP/A2A/OpenAI） */
    AGENTOS_FW_CAP_SAFETY      = 1 << 5,  /*!< 安全检查 */
    AGENTOS_FW_CAP_STREAMING   = 1 << 6,  /*!< 流式响应 */
    AGENTOS_FW_CAP_MULTI_AGENT = 1 << 7,  /*!< 多智能体协作 */
} agentos_fw_capability_t;

/* ==================== 框架状态 ==================== */

typedef enum {
    AGENTOS_FW_STATE_UNINITIALIZED = 0,
    AGENTOS_FW_STATE_INITIALIZED   = 1,
    AGENTOS_FW_STATE_RUNNING       = 2,
    AGENTOS_FW_STATE_PAUSED        = 3,
    AGENTOS_FW_STATE_ERROR         = 4,
    AGENTOS_FW_STATE_SHUTDOWN      = 5,
} agentos_fw_state_t;

/* ==================== 配置 ==================== */

typedef struct {
    char name[AGENTOS_FW_MAX_NAME_LEN];       /*!< 框架实例名 */
    uint32_t max_retries;                      /*!< 最大重试次数 */
    uint32_t timeout_ms;                       /*!< 超时毫秒 */
    bool enable_metrics;                       /*!< 启用指标采集 */
    bool enable_tracing;                       /*!< 启用链路追踪 */
    char config_json[512];                     /*!< 框架特定配置（JSON 字符串） */
} agentos_fw_config_t;

/* ==================== 请求/响应 ==================== */

typedef struct {
    char method[64];                           /*!< 请求方法名 */
    char payload[AGENTOS_FW_MAX_REQUEST_PAYLOAD]; /*!< 请求载荷（JSON） */
    uint32_t payload_len;                      /*!< 载荷长度 */
    uint32_t timeout_ms;                       /*!< 请求超时 */
} agentos_fw_request_t;

typedef struct {
    char payload[AGENTOS_FW_MAX_REQUEST_PAYLOAD]; /*!< 响应载荷（JSON） */
    uint32_t payload_len;                      /*!< 载荷长度 */
    int32_t status_code;                       /*!< 框架特定状态码 */
    uint64_t latency_ms;                       /*!< 处理延迟 */
} agentos_fw_response_t;

/* ==================== 适配器接口 ==================== */

/**
 * @brief 外部 AI 框架适配器接口
 *
 * 外部框架通过实现此接口的回调函数，接入 Airymax 基础设施。
 * 适配器是无状态的函数指针集合，框架实例状态由 handle 携带。
 */
typedef struct agentos_framework_adapter_s {
    /* —— 元信息 —— */
    const char *name;                          /*!< 框架名（如 "langchain"） */
    const char *version;                       /*!< 适配器版本 */
    uint32_t capabilities;                     /*!< 能力位掩码 */

    /* —— 生命周期回调（由外部框架实现）—— */

    /**
     * @brief 初始化框架实例
     * @param config 配置参数
     * @param out_handle 输出实例句柄（由适配器分配）
     * @return AGENTOS_FW_OK 或负错误码
     */
    int32_t (*init)(const agentos_fw_config_t *config, void **out_handle);

    /**
     * @brief 启动框架实例
     * @param handle 实例句柄
     * @return AGENTOS_FW_OK 或负错误码
     */
    int32_t (*start)(void *handle);

    /**
     * @brief 停止框架实例（可重启）
     * @param handle 实例句柄
     * @return AGENTOS_FW_OK 或负错误码
     */
    int32_t (*stop)(void *handle);

    /**
     * @brief 销毁框架实例，释放资源
     * @param handle 实例句柄
     */
    void (*destroy)(void *handle);

    /**
     * @brief 健康检查
     * @param handle 实例句柄
     * @return AGENTOS_FW_OK=健康，负值=异常
     */
    int32_t (*health_check)(void *handle);

    /**
     * @brief 处理请求（可选，NULL 表示不支持请求路由）
     * @param handle 实例句柄
     * @param request 请求参数
     * @param response 响应输出
     * @return AGENTOS_FW_OK 或负错误码
     */
    int32_t (*process_request)(void *handle,
                                const agentos_fw_request_t *request,
                                agentos_fw_response_t *response);
} agentos_framework_adapter_t;

/* ==================== 框架实例信息 ==================== */

typedef struct {
    char name[AGENTOS_FW_MAX_NAME_LEN];        /*!< 实例名 */
    char adapter_name[AGENTOS_FW_MAX_NAME_LEN]; /*!< 适配器名 */
    char adapter_version[AGENTOS_FW_MAX_VERSION_LEN];
    uint32_t capabilities;                     /*!< 能力位掩码 */
    agentos_fw_state_t state;                  /*!< 当前状态 */
    uint64_t init_time_ms;                     /*!< 初始化时间戳 */
    uint64_t request_count;                    /*!< 累计请求数 */
    uint64_t error_count;                      /*!< 累计错误数 */
} agentos_fw_info_t;

/* ==================== 注册表 API ==================== */

/**
 * @brief 初始化框架注册表
 * @return AGENTOS_FW_OK 或负错误码
 * @note 线程安全，幂等（重复调用安全）
 */
int32_t agentos_fw_registry_init(void);

/**
 * @brief 关闭注册表，销毁所有活跃实例
 * @return AGENTOS_FW_OK 或负错误码
 */
int32_t agentos_fw_registry_shutdown(void);

/**
 * @brief 注册外部框架适配器
 * @param adapter 适配器接口（必须填充 name/init/destroy 回调）
 * @return AGENTOS_FW_OK / ALREADY_EXISTS / CAPACITY_FULL / INVALID_ARG
 */
int32_t agentos_fw_register_adapter(const agentos_framework_adapter_t *adapter);

/**
 * @brief 注销适配器（活跃实例将被先停止+销毁）
 * @param name 适配器名
 * @return AGENTOS_FW_OK / NOT_FOUND / BUSY
 */
int32_t agentos_fw_unregister_adapter(const char *name);

/**
 * @brief 查找适配器
 * @param name 适配器名
 * @return 适配器指针（只读），NULL=未找到
 */
const agentos_framework_adapter_t *agentos_fw_find_adapter(const char *name);

/**
 * @brief 列出所有已注册适配器名
 * @param names 输出名数组
 * @param max_count 数组容量
 * @param found_count 实际填充数
 * @return AGENTOS_FW_OK 或负错误码
 */
int32_t agentos_fw_list_adapters(char names[][AGENTOS_FW_MAX_NAME_LEN],
                                  uint32_t max_count, uint32_t *found_count);

/* ==================== 实例生命周期 API ==================== */

/**
 * @brief 创建并初始化框架实例
 * @param adapter_name 适配器名
 * @param config 配置参数（NULL=默认）
 * @param out_instance_name 输出实例名（可用 NULL）
 * @return AGENTOS_FW_OK / NOT_FOUND / INVALID_ARG
 */
int32_t agentos_fw_create_instance(const char *adapter_name,
                                    const agentos_fw_config_t *config,
                                    char *out_instance_name);

/**
 * @brief 启动框架实例
 * @param instance_name 实例名
 * @return AGENTOS_FW_OK / NOT_FOUND / NOT_INIT / ERROR
 */
int32_t agentos_fw_start_instance(const char *instance_name);

/**
 * @brief 停止框架实例（可重启）
 * @param instance_name 实例名
 * @return AGENTOS_FW_OK / NOT_FOUND
 */
int32_t agentos_fw_stop_instance(const char *instance_name);

/**
 * @brief 销毁框架实例，释放资源
 * @param instance_name 实例名
 * @return AGENTOS_FW_OK / NOT_FOUND
 */
int32_t agentos_fw_destroy_instance(const char *instance_name);

/**
 * @brief 查询实例健康状态
 * @param instance_name 实例名
 * @return AGENTOS_FW_OK=健康，负值=异常
 */
int32_t agentos_fw_health_check(const char *instance_name);

/**
 * @brief 获取实例信息
 * @param instance_name 实例名
 * @param out_info 输出信息结构
 * @return AGENTOS_FW_OK / NOT_FOUND / INVALID_ARG
 */
int32_t agentos_fw_get_info(const char *instance_name, agentos_fw_info_t *out_info);

/**
 * @brief 向框架实例发送请求
 * @param instance_name 实例名
 * @param request 请求参数
 * @param response 响应输出
 * @return AGENTOS_FW_OK / NOT_FOUND / NOT_SUPPORTED / ERROR
 */
int32_t agentos_fw_process_request(const char *instance_name,
                                    const agentos_fw_request_t *request,
                                    agentos_fw_response_t *response);

/* ==================== 工具函数 ==================== */

/**
 * @brief 状态枚举转字符串
 */
const char *agentos_fw_state_to_string(agentos_fw_state_t state);

/**
 * @brief 错误码转字符串
 */
const char *agentos_fw_error_to_string(int32_t code);

/**
 * @brief 检查适配器是否具有指定能力
 */
bool agentos_fw_has_capability(const char *instance_name, agentos_fw_capability_t capability);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_FRAMEWORKS_H */
