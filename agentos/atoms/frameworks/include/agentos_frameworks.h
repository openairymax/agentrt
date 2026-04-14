// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file agentos_frameworks.h
 * @brief AgentOS统一框架抽象层 - 五大框架生产级接口
 *
 * 提供AgentOS五大核心框架的统一抽象接口：
 * - Agent框架 (CoreLoopThree): 认知-行动-记忆三层循环
 * - Memory框架 (MemoryRovol): L1-L4四层卷载记忆架构
 * - Task框架 (CoreKern): 加权轮询任务调度
 * - Safety框架 (Cupolas): 四重安全防护
 * - Tool框架 (tool_d): 工具服务管理
 *
 * 设计原则：
 * 1. 统一入口：所有框架通过统一API访问
 * 2. 协议感知：框架操作支持多协议（JSON-RPC/MCP/A2A/OpenAI）
 * 3. 生产级：内置错误处理、重试、监控
 * 4. 可组合：框架间可自由组合编排
 *
 * @see coreloopthree.h Agent核心循环
 * @see memoryrovol.h 记忆系统
 * @see corekern.h 微内核
 * @see cupolas.h 安全穹顶
 */

#ifndef AGENTOS_FRAMEWORKS_H
#define AGENTOS_FRAMEWORKS_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 框架类型枚举 ==================== */

typedef enum {
    AGENTOS_FW_AGENT   = 0,
    AGENTOS_FW_MEMORY  = 1,
    AGENTOS_FW_TASK    = 2,
    AGENTOS_FW_SAFETY  = 3,
    AGENTOS_FW_TOOL    = 4,
    AGENTOS_FW_COUNT   = 5
} agentos_framework_t;

/* ==================== 框架状态 ==================== */

typedef enum {
    AGENTOS_FW_STATE_UNINITIALIZED = 0,
    AGENTOS_FW_STATE_INITIALIZED   = 1,
    AGENTOS_FW_STATE_RUNNING       = 2,
    AGENTOS_FW_STATE_PAUSED        = 3,
    AGENTOS_FW_STATE_ERROR         = 4,
    AGENTOS_FW_STATE_SHUTDOWN      = 5
} agentos_fw_state_t;

/* ==================== 框架能力标志 ==================== */

typedef enum {
    AGENTOS_CAP_COGNITION       = (1 << 0),
    AGENTOS_CAP_EXECUTION       = (1 << 1),
    AGENTOS_CAP_MEMORY_STORE    = (1 << 2),
    AGENTOS_CAP_MEMORY_RETRIEVE = (1 << 3),
    AGENTOS_CAP_TASK_SCHEDULE   = (1 << 4),
    AGENTOS_CAP_TASK_EXECUTE    = (1 << 5),
    AGENTOS_CAP_SAFETY_CHECK    = (1 << 6),
    AGENTOS_CAP_SANDBOX         = (1 << 7),
    AGENTOS_CAP_TOOL_REGISTER   = (1 << 8),
    AGENTOS_CAP_TOOL_INVOKE     = (1 << 9),
    AGENTOS_CAP_PROTOCOL_MCP    = (1 << 10),
    AGENTOS_CAP_PROTOCOL_A2A    = (1 << 11),
    AGENTOS_CAP_PROTOCOL_OPENAI = (1 << 12),
    AGENTOS_CAP_ALL             = 0x1FFF
} agentos_capability_t;

/* ==================== 框架信息 ==================== */

typedef struct {
    agentos_framework_t type;
    char name[32];
    char version[16];
    agentos_fw_state_t state;
    uint32_t capabilities;
    uint64_t init_time_ms;
    uint64_t last_activity_ms;
    uint32_t error_count;
    uint32_t operation_count;
} agentos_fw_info_t;

/* ==================== 框架操作结果 ==================== */

typedef struct {
    int32_t code;
    char message[256];
    void* data;
    size_t data_size;
    uint64_t latency_us;
} agentos_fw_result_t;

#define AGENTOS_FW_OK           0
#define AGENTOS_FW_ERROR       -1
#define AGENTOS_FW_NOT_INIT    -2
#define AGENTOS_FW_INVALID_ARG -3
#define AGENTOS_FW_TIMEOUT     -4
#define AGENTOS_FW_BUSY        -5
#define AGENTOS_FW_NOT_FOUND   -6
#define AGENTOS_FW_DENIED      -7

/* ==================== 框架配置 ==================== */

typedef struct {
    agentos_framework_t type;
    uint32_t max_retries;
    uint32_t timeout_ms;
    bool enable_metrics;
    bool enable_tracing;
    bool enable_protocol_support;
    char config_path[256];
} agentos_fw_config_t;

/* ==================== 框架事件回调 ==================== */

typedef enum {
    AGENTOS_FW_EVENT_INIT       = 1,
    AGENTOS_FW_EVENT_START      = 2,
    AGENTOS_FW_EVENT_STOP       = 3,
    AGENTOS_FW_EVENT_ERROR      = 4,
    AGENTOS_FW_EVENT_STATE_CHANGE = 5,
    AGENTOS_FW_EVENT_OPERATION  = 6
} agentos_fw_event_type_t;

typedef struct {
    agentos_fw_event_type_t type;
    agentos_framework_t framework;
    const char* detail;
    uint64_t timestamp;
    int32_t error_code;
} agentos_fw_event_t;

typedef void (*agentos_fw_event_callback_t)(
    const agentos_fw_event_t* event,
    void* user_data
);

/* ==================== 框架管理器句柄 ==================== */

typedef struct agentos_fw_manager_s* agentos_fw_manager_t;

/* ==================== 框架管理器生命周期 ==================== */

/**
 * @brief 创建框架管理器
 * @return 管理器句柄，失败返回NULL
 */
agentos_fw_manager_t agentos_fw_manager_create(void);

/**
 * @brief 销毁框架管理器
 * @param manager 管理器句柄
 */
void agentos_fw_manager_destroy(agentos_fw_manager_t manager);

/**
 * @brief 初始化指定框架
 * @param manager 管理器句柄
 * @param framework 框架类型
 * @param config 框架配置（NULL使用默认）
 * @return AGENTOS_FW_OK成功，其他失败
 */
int32_t agentos_fw_init(agentos_fw_manager_t manager,
                        agentos_framework_t framework,
                        const agentos_fw_config_t* config);

/**
 * @brief 初始化所有框架
 * @param manager 管理器句柄
 * @return 成功初始化的框架数量（负数表示错误）
 */
int32_t agentos_fw_init_all(agentos_fw_manager_t manager);

/**
 * @brief 启动指定框架
 * @param manager 管理器句柄
 * @param framework 框架类型
 * @return AGENTOS_FW_OK成功，其他失败
 */
int32_t agentos_fw_start(agentos_fw_manager_t manager,
                         agentos_framework_t framework);

/**
 * @brief 启动所有已初始化框架
 * @param manager 管理器句柄
 * @return 成功启动的框架数量
 */
int32_t agentos_fw_start_all(agentos_fw_manager_t manager);

/**
 * @brief 停止指定框架
 * @param manager 管理器句柄
 * @param framework 框架类型
 * @return AGENTOS_FW_OK成功，其他失败
 */
int32_t agentos_fw_stop(agentos_fw_manager_t manager,
                        agentos_framework_t framework);

/**
 * @brief 停止所有框架
 * @param manager 管理器句柄
 */
void agentos_fw_stop_all(agentos_fw_manager_t manager);

/* ==================== 框架查询 ==================== */

/**
 * @brief 获取框架信息
 * @param manager 管理器句柄
 * @param framework 框架类型
 * @param info [out] 框架信息
 * @return AGENTOS_FW_OK成功，其他失败
 */
int32_t agentos_fw_get_info(agentos_fw_manager_t manager,
                            agentos_framework_t framework,
                            agentos_fw_info_t* info);

/**
 * @brief 获取所有框架信息
 * @param manager 管理器句柄
 * @param infos [out] 框架信息数组
 * @param max_count 数组最大容量
 * @param found_count [out] 实际数量
 * @return AGENTOS_FW_OK成功，其他失败
 */
int32_t agentos_fw_get_all_info(agentos_fw_manager_t manager,
                                agentos_fw_info_t* infos,
                                uint32_t max_count,
                                uint32_t* found_count);

/**
 * @brief 检查框架是否支持指定能力
 * @param manager 管理器句柄
 * @param framework 框架类型
 * @param capability 能力标志
 * @return true支持，false不支持
 */
bool agentos_fw_has_capability(agentos_fw_manager_t manager,
                               agentos_framework_t framework,
                               agentos_capability_t capability);

/**
 * @brief 获取框架状态
 * @param manager 管理器句柄
 * @param framework 框架类型
 * @return 框架状态
 */
agentos_fw_state_t agentos_fw_get_state(agentos_fw_manager_t manager,
                                        agentos_framework_t framework);

/* ==================== 框架事件 ==================== */

/**
 * @brief 注册框架事件回调
 * @param manager 管理器句柄
 * @param framework 框架类型（AGENTOS_FW_COUNT表示所有框架）
 * @param callback 回调函数
 * @param user_data 用户数据
 * @return AGENTOS_FW_OK成功，其他失败
 */
int32_t agentos_fw_register_event_callback(agentos_fw_manager_t manager,
                                           agentos_framework_t framework,
                                           agentos_fw_event_callback_t callback,
                                           void* user_data);

/* ==================== 框架健康检查 ==================== */

/**
 * @brief 执行框架健康检查
 * @param manager 管理器句柄
 * @param framework 框架类型
 * @return AGENTOS_FW_OK健康，其他不健康
 */
int32_t agentos_fw_health_check(agentos_fw_manager_t manager,
                                agentos_framework_t framework);

/**
 * @brief 执行所有框架健康检查
 * @param manager 管理器句柄
 * @return 健康框架数量（负数表示错误）
 */
int32_t agentos_fw_health_check_all(agentos_fw_manager_t manager);

/* ==================== 工具函数 ==================== */

/**
 * @brief 框架类型转字符串
 * @param framework 框架类型
 * @return 框架名称
 */
const char* agentos_fw_type_to_string(agentos_framework_t framework);

/**
 * @brief 框架状态转字符串
 * @param state 框架状态
 * @return 状态名称
 */
const char* agentos_fw_state_to_string(agentos_fw_state_t state);

/**
 * @brief 错误码转字符串
 * @param code 错误码
 * @return 错误描述
 */
const char* agentos_fw_error_to_string(int32_t code);

/**
 * @brief 创建默认框架配置
 * @param framework 框架类型
 * @return 默认配置
 */
agentos_fw_config_t agentos_fw_create_default_config(agentos_framework_t framework);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_FRAMEWORKS_H */
