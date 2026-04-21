/**
 * @file memory.h
 * @brief 记忆层公共接口定义
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_CORELOOPTHREE_MEMORY_H
#define AGENTOS_CORELOOPTHREE_MEMORY_H

// API 版本声明 (MAJOR.MINOR.PATCH)
#define MEMORY_API_VERSION_MAJOR 1
#define MEMORY_API_VERSION_MINOR 0
#define MEMORY_API_VERSION_PATCH 0

// ABI 兼容性声明
// 在相同 MAJOR 版本内保证 ABI 兼容
// 破坏性更改需递增 MAJOR 并发布迁移说明

#include "../atoms/corekern/include/agentos.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 前向声明 */
typedef struct agentos_memory_engine agentos_memory_engine_t;
typedef struct agentos_memory_record agentos_memory_record_t;
typedef struct agentos_memory_query agentos_memory_query_t;

/**
 * @brief 记忆记录类型
 */
#ifndef AGENTOS_MEMORY_TYPE_T_DEFINED
#define AGENTOS_MEMORY_TYPE_T_DEFINED
typedef enum {
    MEMORY_TYPE_RAW = 0,
    MEMORY_TYPE_FEATURE,
    MEMORY_TYPE_STRUCTURE,
    MEMORY_TYPE_PATTERN
} agentos_memory_type_t;
#endif

/**
 * @brief 记忆记录
 */
typedef struct agentos_memory_record {
    char* memory_record_id;                     /**< 记录唯一ID */
    size_t memory_record_id_len;                /**< ID长度 */
    agentos_memory_type_t memory_record_type;   /**< 记忆类型 */
    uint64_t memory_record_timestamp_ns;        /**< 时间戳 */
    char* memory_record_source_agent;           /**< 来源Agent ID */
    size_t memory_record_source_len;            /**< 来源长度 */
    char* memory_record_trace_id;               /**< 关联追踪ID */
    size_t memory_record_trace_len;             /**< 追踪ID长度 */
    void* memory_record_data;                   /**< 记忆数据 */
    size_t memory_record_data_len;              /**< 数据长度（字节） */
    float memory_record_importance;             /**< 重要性（0-1） */
    uint32_t memory_record_access_count;        /**< 访问次数 */
} agentos_memory_record_t;

/**
 * @brief 记忆查询条件
 */
typedef struct agentos_memory_query {
    char* memory_query_text;                 /**< 查询文本 */
    size_t memory_query_text_len;            /**< 文本长度 */
    uint64_t memory_query_start_time;        /**< 起始时间（0表示不限制） */
    uint64_t memory_query_end_time;          /**< 结束时间 */
    char* memory_query_source_agent;         /**< 来源Agent（NULL表示不限） */
    char* memory_query_trace_id;             /**< 关联追踪ID */
    uint32_t memory_query_limit;             /**< 返回结果数量上限 */
    uint32_t memory_query_offset;            /**< 偏移量 */
    uint8_t memory_query_include_raw;        /**< 是否包含原始数据 */
} agentos_memory_query_t;

/**
 * @brief 检索结果项
 */
typedef struct agentos_memory_result_item {
    char* memory_result_item_record_id;      /**< 记录ID */
    float memory_result_item_score;          /**< 相似度得分（0-1） */
    agentos_memory_record_t* memory_result_item_record; /**< 完整记录（若include_raw为真） */
} agentos_memory_result_item_t;

/**
 * @brief 检索结果
 */
#ifndef AGENTOS_MEMORY_RESULT_T_DEFINED
#define AGENTOS_MEMORY_RESULT_T_DEFINED
typedef struct agentos_memory_result {
    agentos_memory_result_item_t** memory_result_items;
    size_t memory_result_count;
    uint64_t memory_result_query_time_ns;
} agentos_memory_result_t;
#endif

/* ==================== 记忆引擎接口 ==================== */

/**
 * @brief 创建记忆引擎
 *
 * @param config_path [in] 配置文件路径（可为NULL）
 * @param out_engine [out] 输出引擎句柄（调用者负责销毁）
 * @return agentos_error_t AGENTOS_SUCCESS 成功，其他为错误码
 *
 * @ownership out_engine 由调用者负责通过 agentos_memory_destroy() 释放
 * @threadsafe 否（内部未使用线程安全措施）
 * @reentrant 否
 * @see agentos_memory_destroy()
 */
AGENTOS_API agentos_error_t agentos_memory_create(
    const char* config_path,
    agentos_memory_engine_t** out_engine);

/**
 * @brief 销毁记忆引擎
 *
 * @param engine [in] 引擎句柄（非NULL）
 *
 * @ownership 释放 engine 及其内部所有资源
 * @threadsafe 否（内部未使用线程安全措施）
 * @reentrant 否
 * @see agentos_memory_create()
 */
AGENTOS_API void agentos_memory_destroy(agentos_memory_engine_t* engine);

/**
 * @brief 写入记忆记录
 *
 * @param engine [in] 记忆引擎（非NULL）
 * @param record [in] 记忆记录（引擎会复制内部数据，非NULL）
 * @param out_record_id [out] 输出分配的记录ID（调用者负责释放）
 * @return agentos_error_t AGENTOS_SUCCESS 成功，其他为错误码
 *
 * @ownership out_record_id 由调用者负责释放
 * @threadsafe 是（内部使用互斥锁保护）
 * @reentrant 否
 */
AGENTOS_API agentos_error_t agentos_memory_write(
    agentos_memory_engine_t* engine,
    const agentos_memory_record_t* record,
    char** out_record_id);

/**
 * @brief 查询记忆
 *
 * @param engine [in] 记忆引擎（非NULL）
 * @param query [in] 查询条件（非NULL）
 * @param out_result [out] 输出结果（调用者负责释放）
 * @return agentos_error_t AGENTOS_SUCCESS 成功，其他为错误码
 *
 * @ownership out_result 由调用者负责通过 agentos_memory_result_free() 释放
 * @threadsafe 是（内部使用互斥锁保护）
 * @reentrant 否
 * @see agentos_memory_result_free()
 */
AGENTOS_API agentos_error_t agentos_memory_query(
    agentos_memory_engine_t* engine,
    const agentos_memory_query_t* query,
    agentos_memory_result_t** out_result);

/**
 * @brief 根据ID获取记忆记录
 *
 * @param engine [in] 记忆引擎（非NULL）
 * @param record_id [in] 记录ID（非NULL）
 * @param include_raw [in] 是否加载原始数据
 * @param out_record [out] 输出记录（调用者负责释放）
 * @return agentos_error_t AGENTOS_SUCCESS 成功，其他为错误码
 *
 * @ownership out_record 由调用者负责通过 agentos_memory_record_free() 释放
 * @threadsafe 是（内部使用互斥锁保护）
 * @reentrant 否
 *
 * @concurrency 并发合约：
 * - 该函数是线程安全的，可以在多个线程同时调用
 * - 内部使用互斥锁保护内存访问，避免并发冲突
 * - 调用者无需额外的同步措施
 * - 但返回的记录对象不是线程安全的，需要调用者自行管理其并发访问
 *
 * @see agentos_memory_record_free()
 */
AGENTOS_API agentos_error_t agentos_memory_get(
    agentos_memory_engine_t* engine,
    const char* record_id,
    int include_raw,
    agentos_memory_record_t** out_record);

/**
 * @brief 挂载记忆到当前上下文（相当于通知引擎该记忆被使用）
 *
 * @param engine [in] 记忆引擎（非NULL）
 * @param record_id [in] 记录ID（非NULL）
 * @param context [in] 当前上下文标识（如任务ID，非NULL）
 * @return agentos_error_t AGENTOS_SUCCESS 成功，其他为错误码
 *
 * @threadsafe 是（内部使用互斥锁保护）
 * @reentrant 否
 */
AGENTOS_API agentos_error_t agentos_memory_mount(
    agentos_memory_engine_t* engine,
    const char* record_id,
    const char* context);

/**
 * @brief 释放记忆结果
 *
 * @param result [in] 结果对象（可为NULL）
 *
 * @ownership 释放 result 及其内部所有资源
 * @threadsafe 否（内部未使用线程安全措施）
 * @reentrant 否
 * @see agentos_memory_query()
 */
AGENTOS_API void agentos_memory_result_free(agentos_memory_result_t* result);

/**
 * @brief 释放单个记忆记录
 *
 * @param record [in] 记录对象（可为NULL）
 *
 * @ownership 释放 record 及其内部所有资源
 * @threadsafe 否（内部未使用线程安全措施）
 * @reentrant 否
 * @see agentos_memory_get()
 */
AGENTOS_API void agentos_memory_record_free(agentos_memory_record_t* record);

/**
 * @brief 触发记忆进化（模式挖掘）
 *
 * @param engine [in] 记忆引擎（非NULL）
 * @param force [in] 是否强制立即执行
 * @return agentos_error_t AGENTOS_SUCCESS 成功，其他为错误码
 *
 * @threadsafe 否（内部未使用线程安全措施）
 * @reentrant 否
 */
AGENTOS_API agentos_error_t agentos_memory_evolve(
    agentos_memory_engine_t* engine,
    int force);

/**
 * @brief 获取记忆引擎健康状态
 *
 * @param engine [in] 记忆引擎句柄（非NULL）
 * @param out_json [out] 输出 JSON 状态字符串（调用者负责释放）
 * @return agentos_error_t AGENTOS_SUCCESS 成功，其他为错误码
 *
 * @ownership out_json 由调用者负责释放
 * @threadsafe 是（内部使用互斥锁保护）
 * @reentrant 否
 */
AGENTOS_API agentos_error_t agentos_memory_health_check(
    agentos_memory_engine_t* engine,
    char** out_json);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_CORELOOPTHREE_MEMORY_H */
