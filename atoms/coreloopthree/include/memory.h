/**
 * @file memory.h
 * @brief 记忆层公共接口定义
 * @copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
 */

#ifndef AGENTOS_MEMORY_H
#define AGENTOS_MEMORY_H

#include "agentos.h"
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
typedef enum {
    MEMORY_TYPE_RAW = 0,        /**< 原始事件 */
    MEMORY_TYPE_FEATURE,         /**< 特征向量 */
    MEMORY_TYPE_STRUCTURE,       /**< 结构化关系 */
    MEMORY_TYPE_PATTERN          /**< 抽象模式 */
} agentos_memory_type_t;

/**
 * @brief 记忆记录
 */
typedef struct agentos_memory_record {
    char* record_id;                     /**< 记录唯一ID */
    size_t id_len;                        /**< ID长度 */
    agentos_memory_type_t type;           /**< 记忆类型 */
    uint64_t timestamp_ns;                 /**< 时间戳 */
    char* source_agent;                    /**< 来源Agent ID */
    size_t source_len;                     /**< 来源长度 */
    char* trace_id;                        /**< 关联追踪ID */
    size_t trace_len;                      /**< 追踪ID长度 */
    void* data;                            /**< 记忆数据 */
    size_t data_len;                       /**< 数据长度（字节） */
    float importance;                      /**< 重要性（0-1） */
    uint32_t access_count;                  /**< 访问次数 */
} agentos_memory_record_t;

/**
 * @brief 记忆查询条件
 */
typedef struct agentos_memory_query {
    char* text;                            /**< 查询文本 */
    size_t text_len;                        /**< 文本长度 */
    uint64_t start_time;                     /**< 起始时间（0表示不限制） */
    uint64_t end_time;                       /**< 结束时间 */
    char* source_agent;                      /**< 来源Agent（NULL表示不限） */
    char* trace_id;                          /**< 关联追踪ID */
    uint32_t limit;                          /**< 返回结果数量上限 */
    uint32_t offset;                         /**< 偏移量 */
    uint8_t include_raw;                     /**< 是否包含原始数据 */
} agentos_memory_query_t;

/**
 * @brief 检索结果项
 */
typedef struct agentos_memory_result_item {
    char* record_id;                         /**< 记录ID */
    float score;                             /**< 相似度得分（0-1） */
    agentos_memory_record_t* record;          /**< 完整记录（若include_raw为真） */
} agentos_memory_result_item_t;

/**
 * @brief 检索结果
 */
typedef struct agentos_memory_result {
    agentos_memory_result_item_t** items;    /**< 结果数组 */
    size_t count;                             /**< 结果数量 */
    uint64_t query_time_ns;                    /**< 查询耗时（纳秒） */
} agentos_memory_result_t;

/* ==================== 记忆引擎接口 ==================== */

/**
 * @brief 创建记忆引擎
 * @param config_path 配置文件路径（可为NULL）
 * @param out_engine 输出引擎句柄
 * @return agentos_error_t
 */
agentos_error_t agentos_memory_create(
    const char* config_path,
    agentos_memory_engine_t** out_engine);

/**
 * @brief 销毁记忆引擎
 * @param engine 引擎句柄
 */
void agentos_memory_destroy(agentos_memory_engine_t* engine);

/**
 * @brief 写入记忆记录
 * @param engine 记忆引擎
 * @param record 记忆记录（引擎会复制内部数据）
 * @param out_record_id 输出分配的记录ID（需调用者释放）
 * @return agentos_error_t
 */
agentos_error_t agentos_memory_write(
    agentos_memory_engine_t* engine,
    const agentos_memory_record_t* record,
    char** out_record_id);

/**
 * @brief 查询记忆
 * @param engine 记忆引擎
 * @param query 查询条件
 * @param out_result 输出结果（需调用 agentos_memory_result_free 释放）
 * @return agentos_error_t
 */
agentos_error_t agentos_memory_query(
    agentos_memory_engine_t* engine,
    const agentos_memory_query_t* query,
    agentos_memory_result_t** out_result);

/**
 * @brief 根据ID获取记忆记录
 * @param engine 记忆引擎
 * @param record_id 记录ID
 * @param include_raw 是否加载原始数据
 * @param out_record 输出记录（需调用 agentos_memory_record_free 释放）
 * @return agentos_error_t
 */
agentos_error_t agentos_memory_get(
    agentos_memory_engine_t* engine,
    const char* record_id,
    int include_raw,
    agentos_memory_record_t** out_record);

/**
 * @brief 挂载记忆到当前上下文（相当于通知引擎该记忆被使用）
 * @param engine 记忆引擎
 * @param record_id 记录ID
 * @param context 当前上下文标识（如任务ID）
 * @return agentos_error_t
 */
agentos_error_t agentos_memory_mount(
    agentos_memory_engine_t* engine,
    const char* record_id,
    const char* context);

/**
 * @brief 释放记忆结果
 * @param result 结果对象
 */
void agentos_memory_result_free(agentos_memory_result_t* result);

/**
 * @brief 释放单个记忆记录
 * @param record 记录对象
 */
void agentos_memory_record_free(agentos_memory_record_t* record);

/**
 * @brief 触发记忆进化（模式挖掘）
 * @param engine 记忆引擎
 * @param force 是否强制立即执行
 * @return agentos_error_t
 */
agentos_error_t agentos_memory_evolve(
    agentos_memory_engine_t* engine,
    int force);

/**
 * @brief 获取记忆引擎健康状态
 * @param engine 记忆引擎句柄
 * @param out_json 输出 JSON 状态字符串（需调用者释放）
 * @return agentos_error_t
 */
agentos_error_t agentos_memory_health_check(
    agentos_memory_engine_t* engine,
    char** out_json);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_MEMORY_H */