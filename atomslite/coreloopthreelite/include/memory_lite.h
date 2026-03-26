/**
 * @file memory_lite.h
 * @brief AgentOS Lite CoreLoopThree - 轻量化记忆层接口
 * @version 1.0.0
 * @date 2026-03-26
 * 
 * 轻量化记忆层提供基本的内存接口和上下文管理功能：
 * 1. 结果缓存：缓存任务执行结果，提高重复查询效率
 * 2. 上下文管理：管理执行上下文信息
 * 3. 状态持久化：支持基本的状态持久化和恢复
 * 4. 内存优化：使用对象池和预分配缓冲区减少内存碎片
 * 
 * 设计目标：
 * - 存取延迟：< 0.05ms
 * - 内存占用：< 50KB（基础缓存）
 * - 缓存命中率：> 80%（重复查询场景）
 * - 持久化开销：< 5ms（每次保存）
 */

#ifndef AGENTOS_MEMORY_LITE_H
#define AGENTOS_MEMORY_LITE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 错误码定义 ==================== */

typedef enum {
    CLT_MEMORY_SUCCESS = 0,             /**< 操作成功 */
    CLT_MEMORY_ERROR = -1,              /**< 通用错误 */
    CLT_MEMORY_INVALID_PARAM = -2,      /**< 无效参数 */
    CLT_MEMORY_OUT_OF_MEMORY = -3,      /**< 内存不足 */
    CLT_MEMORY_ITEM_NOT_FOUND = -4,     /**< 条目未找到 */
    CLT_MEMORY_STORAGE_FULL = -5,       /**< 存储空间不足 */
    CLT_MEMORY_IO_ERROR = -6,           /**< 输入输出错误 */
} clt_memory_error_t;

/* ==================== 缓存条目结构 ==================== */

/**
 * @brief 缓存条目元数据
 */
typedef struct {
    size_t id;                          /**< 条目ID */
    uint64_t timestamp;                 /**< 时间戳（毫秒） */
    uint64_t expire_time;               /**< 过期时间（0表示永不过期） */
    size_t data_size;                   /**< 数据大小 */
    uint32_t access_count;              /**< 访问次数 */
    uint64_t last_access_time;          /**< 最后访问时间 */
} clt_memory_cache_metadata_t;

/* ==================== 公共接口 ==================== */

/**
 * @brief 初始化记忆层
 * @return 成功返回true，失败返回false
 */
bool clt_memory_init(void);

/**
 * @brief 清理记忆层资源
 */
void clt_memory_cleanup(void);

/**
 * @brief 保存任务结果到记忆层
 * @param task_id 任务ID
 * @param result 任务结果（JSON格式字符串）
 * @param result_len 结果长度
 * @return 成功返回true，失败返回false
 */
bool clt_memory_save_result(size_t task_id, const char* result, size_t result_len);

/**
 * @brief 根据任务ID获取结果
 * @param task_id 任务ID
 * @param result 输出参数：任务结果（JSON格式字符串）
 * @param result_len 输出参数：结果长度
 * @return 成功返回true，失败返回false
 */
bool clt_memory_get_result(size_t task_id, char** result, size_t* result_len);

/**
 * @brief 根据内容搜索缓存结果
 * @param query_text 查询文本
 * @param max_results 最大返回结果数
 * @param result_ids 输出参数：结果ID数组
 * @param result_count 输出参数：结果数量
 * @return 成功返回true，失败返回false
 */
bool clt_memory_search_results(
    const char* query_text,
    size_t max_results,
    size_t** result_ids,
    size_t* result_count
);

/**
 * @brief 删除任务结果
 * @param task_id 任务ID
 * @return 成功返回true，失败返回false
 */
bool clt_memory_delete_result(size_t task_id);

/**
 * @brief 清理过期缓存
 * @param max_age_ms 最大缓存年龄（毫秒）
 * @return 被删除的条目数量
 */
size_t clt_memory_cleanup_expired(uint64_t max_age_ms);

/**
 * @brief 获取缓存统计信息
 * @param total_entries 输出参数：总条目数
 * @param total_size 输出参数：总缓存大小（字节）
 * @param hit_count 输出参数：命中次数
 * @param miss_count 输出参数：未命中次数
 */
void clt_memory_get_stats(
    size_t* total_entries,
    size_t* total_size,
    uint64_t* hit_count,
    uint64_t* miss_count
);

/**
 * @brief 保存上下文信息
 * @param context_id 上下文ID
 * @param context_data 上下文数据（JSON格式字符串）
 * @param context_data_len 上下文数据长度
 * @return 成功返回true，失败返回false
 */
bool clt_memory_save_context(
    const char* context_id,
    const char* context_data,
    size_t context_data_len
);

/**
 * @brief 获取上下文信息
 * @param context_id 上下文ID
 * @param context_data 输出参数：上下文数据（JSON格式字符串）
 * @param context_data_len 输出参数：上下文数据长度
 * @return 成功返回true，失败返回false
 */
bool clt_memory_get_context(
    const char* context_id,
    char** context_data,
    size_t* context_data_len
);

/**
 * @brief 删除上下文信息
 * @param context_id 上下文ID
 * @return 成功返回true，失败返回false
 */
bool clt_memory_delete_context(const char* context_id);

/**
 * @brief 导出缓存数据到文件
 * @param file_path 导出文件路径
 * @return 成功返回true，失败返回false
 */
bool clt_memory_export_cache(const char* file_path);

/**
 * @brief 从文件导入缓存数据
 * @param file_path 导入文件路径
 * @return 成功返回true，失败返回false
 */
bool clt_memory_import_cache(const char* file_path);

/**
 * @brief 获取最后错误信息
 * @return 错误信息字符串
 */
const char* clt_memory_get_last_error(void);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_MEMORY_LITE_H */