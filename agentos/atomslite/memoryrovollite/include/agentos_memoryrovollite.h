/**
 * @file agentos_memoryrovollite.h
 * @brief AgentOS Lite MemoryRovol - 轻量化内存管理与持久化
 * @version 1.0.0
 * @date 2026-03-26
 * 
 * 轻量化版本的MemoryRovol模块，提供简化的内存管理和持久化功能：
 * 1. 轻量化存储（Storage Lite）：基于SQLite的高效元数据存储
 * 2. 向量检索（Vector Lite）：简化的向量相似度搜索
 * 3. 持久化管理（Persistence Lite）：数据持久化和恢复
 * 4. 检索优化（Retrieval Lite）：高效的内存检索机制
 * 
 * 设计目标：
 * - 内存占用降低60%以上
 * - 检索延迟减少40%以上
 * - 存储空间优化50%以上
 * - 保持核心检索和持久化功能
 */

#ifndef AGENTOS_MEMORYROVOLLITE_H
#define AGENTOS_MEMORYROVOLLITE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup memoryrovollite AgentOS Lite MemoryRovol
 * @brief 轻量化内存管理与持久化
 * @{
 */

/**
 * @brief 错误码定义
 */
typedef enum {
    AGENTOS_MRL_SUCCESS = 0,               /**< 操作成功 */
    AGENTOS_MRL_ERROR = -1,                /**< 通用错误 */
    AGENTOS_MRL_INVALID_PARAM = -2,        /**< 无效参数 */
    AGENTOS_MRL_OUT_OF_MEMORY = -3,        /**< 内存不足 */
    AGENTOS_MRL_STORAGE_FULL = -4,         /**< 存储空间不足 */
    AGENTOS_MRL_ITEM_NOT_FOUND = -5,       /**< 条目未找到 */
    AGENTOS_MRL_DB_ERROR = -6,             /**< 数据库错误 */
    AGENTOS_MRL_IO_ERROR = -7,             /**< 输入输出错误 */
    AGENTOS_MRL_NOT_INITIALIZED = -8,      /**< 未初始化 */
    AGENTOS_MRL_OPERATION_TIMEOUT = -9,    /**< 操作超时 */
} agentos_mrl_error_t;

/**
 * @brief 存储类型
 */
typedef enum {
    AGENTOS_MRL_STORAGE_MEMORY = 0,        /**< 内存存储（临时） */
    AGENTOS_MRL_STORAGE_DISK = 1,          /**< 磁盘存储（持久化） */
    AGENTOS_MRL_STORAGE_HYBRID = 2,        /**< 混合存储（内存+磁盘） */
} agentos_mrl_storage_type_t;

/**
 * @brief 向量距离度量
 */
typedef enum {
    AGENTOS_MRL_DISTANCE_L2 = 0,           /**< L2距离（欧氏距离） */
    AGENTOS_MRL_DISTANCE_COSINE = 1,       /**< 余弦相似度 */
    AGENTOS_MRL_DISTANCE_INNER_PRODUCT = 2, /**< 内积距离 */
} agentos_mrl_distance_metric_t;

/**
 * @brief 记忆条目句柄（不透明类型）
 */
typedef struct agentos_mrl_item_handle_s agentos_mrl_item_handle_t;

/**
 * @brief 记忆存储句柄（不透明类型）
 */
typedef struct agentos_mrl_storage_handle_s agentos_mrl_storage_handle_t;

/**
 * @brief 检索结果句柄（不透明类型）
 */
typedef struct agentos_mrl_result_handle_s agentos_mrl_result_handle_t;

/**
 * @brief 向量数据
 */
typedef struct {
    float* data;                           /**< 向量数据指针 */
    size_t dimension;                      /**< 向量维度 */
    size_t id;                             /**< 向量ID */
    float norm;                            /**< 向量范数（用于优化计算） */
} agentos_mrl_vector_t;

/**
 * @brief 记忆条目元数据
 */
typedef struct {
    size_t id;                             /**< 条目ID */
    const char* type;                      /**< 条目类型 */
    const char* content;                   /**< 内容描述 */
    uint64_t timestamp;                    /**< 时间戳 */
    float importance;                      /**< 重要性分数（0.0-1.0） */
    uint32_t access_count;                 /**< 访问次数 */
    uint64_t last_access_time;             /**< 最后访问时间 */
} agentos_mrl_item_metadata_t;

/**
 * @brief 检索参数
 */
typedef struct {
    size_t max_results;                    /**< 最大返回结果数 */
    float similarity_threshold;            /**< 相似度阈值（0.0-1.0） */
    agentos_mrl_distance_metric_t metric;  /**< 距离度量方法 */
    bool include_vectors;                  /**< 是否包含向量数据 */
    bool include_metadata;                 /**< 是否包含元数据 */
} agentos_mrl_retrieval_params_t;

/**
 * @brief 初始化记忆存储
 * @param db_path 数据库文件路径（NULL表示内存数据库）
 * @param storage_type 存储类型
 * @param max_memory_items 最大内存条目数（0表示无限制）
 * @return 存储句柄，失败返回NULL
 */
agentos_mrl_storage_handle_t* agentos_mrl_storage_init(
    const char* db_path,
    agentos_mrl_storage_type_t storage_type,
    size_t max_memory_items
);

/**
 * @brief 销毁记忆存储
 * @param storage 存储句柄
 * @return 错误码
 */
agentos_mrl_error_t agentos_mrl_storage_destroy(
    agentos_mrl_storage_handle_t* storage
);

/**
 * @brief 保存记忆条目
 * @param storage 存储句柄
 * @param metadata 条目元数据
 * @param vector 向量数据（可为NULL）
 * @param raw_data 原始数据（可为NULL）
 * @param raw_data_len 原始数据长度
 * @return 条目句柄，失败返回NULL
 */
agentos_mrl_item_handle_t* agentos_mrl_item_save(
    agentos_mrl_storage_handle_t* storage,
    const agentos_mrl_item_metadata_t* metadata,
    const agentos_mrl_vector_t* vector,
    const void* raw_data,
    size_t raw_data_len
);

/**
 * @brief 根据ID获取记忆条目
 * @param storage 存储句柄
 * @param item_id 条目ID
 * @return 条目句柄，失败返回NULL
 */
agentos_mrl_item_handle_t* agentos_mrl_item_get_by_id(
    agentos_mrl_storage_handle_t* storage,
    size_t item_id
);

/**
 * @brief 根据内容搜索记忆条目
 * @param storage 存储句柄
 * @param query_text 查询文本
 * @param params 检索参数
 * @return 结果句柄，失败返回NULL
 */
agentos_mrl_result_handle_t* agentos_mrl_item_search_by_text(
    agentos_mrl_storage_handle_t* storage,
    const char* query_text,
    const agentos_mrl_retrieval_params_t* params
);

/**
 * @brief 根据向量搜索记忆条目
 * @param storage 存储句柄
 * @param query_vector 查询向量
 * @param params 检索参数
 * @return 结果句柄，失败返回NULL
 */
agentos_mrl_result_handle_t* agentos_mrl_item_search_by_vector(
    agentos_mrl_storage_handle_t* storage,
    const agentos_mrl_vector_t* query_vector,
    const agentos_mrl_retrieval_params_t* params
);

/**
 * @brief 删除记忆条目
 * @param item_handle 条目句柄
 * @return 错误码
 */
agentos_mrl_error_t agentos_mrl_item_delete(
    agentos_mrl_item_handle_t* item_handle
);

/**
 * @brief 更新记忆条目重要性
 * @param item_handle 条目句柄
 * @param importance 新的重要性分数（0.0-1.0）
 * @return 错误码
 */
agentos_mrl_error_t agentos_mrl_item_update_importance(
    agentos_mrl_item_handle_t* item_handle,
    float importance
);

/**
 * @brief 获取结果数量
 * @param result_handle 结果句柄
 * @return 结果数量
 */
size_t agentos_mrl_result_get_count(
    const agentos_mrl_result_handle_t* result_handle
);

/**
 * @brief 获取结果条目
 * @param result_handle 结果句柄
 * @param index 结果索引（0到count-1）
 * @return 条目句柄，失败返回NULL
 */
agentos_mrl_item_handle_t* agentos_mrl_result_get_item(
    const agentos_mrl_result_handle_t* result_handle,
    size_t index
);

/**
 * @brief 获取结果相似度分数
 * @param result_handle 结果句柄
 * @param index 结果索引（0到count-1）
 * @return 相似度分数（0.0-1.0），失败返回-1.0
 */
float agentos_mrl_result_get_similarity(
    const agentos_mrl_result_handle_t* result_handle,
    size_t index
);

/**
 * @brief 销毁结果句柄
 * @param result_handle 结果句柄
 * @return 错误码
 */
agentos_mrl_error_t agentos_mrl_result_destroy(
    agentos_mrl_result_handle_t* result_handle
);

/**
 * @brief 获取条目元数据
 * @param item_handle 条目句柄
 * @param metadata 输出参数：元数据
 * @return 错误码
 * 
 * @note 元数据中的字符串指针仅在条目句柄有效期间有效
 */
agentos_mrl_error_t agentos_mrl_item_get_metadata(
    const agentos_mrl_item_handle_t* item_handle,
    agentos_mrl_item_metadata_t* metadata
);

/**
 * @brief 获取条目向量数据
 * @param item_handle 条目句柄
 * @param vector 输出参数：向量数据
 * @return 错误码
 * 
 * @note 向量数据指针仅在条目句柄有效期间有效
 */
agentos_mrl_error_t agentos_mrl_item_get_vector(
    const agentos_mrl_item_handle_t* item_handle,
    agentos_mrl_vector_t* vector
);

/**
 * @brief 获取条目原始数据
 * @param item_handle 条目句柄
 * @param raw_data 输出参数：原始数据指针
 * @param raw_data_len 输出参数：原始数据长度
 * @return 错误码
 * 
 * @note 原始数据指针仅在条目句柄有效期间有效
 */
agentos_mrl_error_t agentos_mrl_item_get_raw_data(
    const agentos_mrl_item_handle_t* item_handle,
    const void** raw_data,
    size_t* raw_data_len
);

/**
 * @brief 执行记忆压缩（清理不重要的条目）
 * @param storage 存储句柄
 * @param target_size 目标大小（条目数，0表示使用默认值）
 * @return 被删除的条目数量
 */
size_t agentos_mrl_storage_compress(
    agentos_mrl_storage_handle_t* storage,
    size_t target_size
);

/**
 * @brief 导出存储数据到文件
 * @param storage 存储句柄
 * @param file_path 导出文件路径
 * @return 错误码
 */
agentos_mrl_error_t agentos_mrl_storage_export(
    const agentos_mrl_storage_handle_t* storage,
    const char* file_path
);

/**
 * @brief 从文件导入存储数据
 * @param storage 存储句柄
 * @param file_path 导入文件路径
 * @return 错误码
 */
agentos_mrl_error_t agentos_mrl_storage_import(
    agentos_mrl_storage_handle_t* storage,
    const char* file_path
);

/**
 * @brief 获取存储统计信息
 * @param storage 存储句柄
 * @param stats 输出参数：统计信息（JSON格式字符串）
 * @param stats_len 输出参数：统计信息长度
 * @return 错误码
 * 
 * @note 统计信息缓冲区由调用方释放（使用agentos_mrl_free_buffer）
 */
agentos_mrl_error_t agentos_mrl_storage_get_stats(
    const agentos_mrl_storage_handle_t* storage,
    char** stats,
    size_t* stats_len
);

/**
 * @brief 释放缓冲区
 * @param buffer 缓冲区指针
 */
void agentos_mrl_free_buffer(void* buffer);

/**
 * @brief 获取最后错误信息
 * @return 错误信息字符串
 */
const char* agentos_mrl_get_last_error(void);

/**
 * @brief 获取版本信息
 * @return 版本字符串
 */
const char* agentos_mrl_get_version(void);

/** @} */ // end of memoryrovollite group

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_MEMORYROVOLLITE_H */