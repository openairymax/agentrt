/**
 * @file cognition_lite.h
 * @brief AgentOS Lite CoreLoopThree - 轻量化认知层接口
 * @version 1.0.0
 * @date 2026-03-26
 * 
 * 轻量化认知层提供简化的意图理解和任务规划功能：
 * 1. 任务解析：解析JSON格式的任务数据
 * 2. 意图识别：识别任务意图和关键参数
 * 3. 规划生成：生成简化的执行计划
 * 4. 优先级评估：评估任务优先级和执行策略
 * 
 * 设计目标：
 * - 处理延迟：< 0.1ms
 * - 内存占用：< 100KB
 * - 支持基本JSON解析和任务分类
 */

#ifndef AGENTOS_COGNITION_LITE_H
#define AGENTOS_COGNITION_LITE_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 错误码定义 ==================== */

typedef enum {
    CLT_COGNITION_SUCCESS = 0,           /**< 操作成功 */
    CLT_COGNITION_ERROR = -1,            /**< 通用错误 */
    CLT_COGNITION_INVALID_JSON = -2,     /**< 无效的JSON数据 */
    CLT_COGNITION_UNKNOWN_INTENT = -3,   /**< 未知的任务意图 */
    CLT_COGNITION_OUT_OF_MEMORY = -4,    /**< 内存不足 */
} clt_cognition_error_t;

/* ==================== 任务意图定义 ==================== */

typedef enum {
    CLT_INTENT_UNKNOWN = 0,              /**< 未知意图 */
    CLT_INTENT_QUERY = 1,                /**< 查询意图 */
    CLT_INTENT_UPDATE = 2,               /**< 更新意图 */
    CLT_INTENT_DELETE = 3,               /**< 删除意图 */
    CLT_INTENT_CREATE = 4,               /**< 创建意图 */
    CLT_INTENT_SEARCH = 5,               /**< 搜索意图 */
    CLT_INTENT_ANALYZE = 6,              /**< 分析意图 */
    CLT_INTENT_SUMMARIZE = 7,            /**< 总结意图 */
    CLT_INTENT_TRANSFORM = 8,            /**< 转换意图 */
} clt_task_intent_t;

/* ==================== 任务参数结构 ==================== */

/**
 * @brief 任务参数键值对
 */
typedef struct {
    char* key;                           /**< 参数键 */
    char* value;                         /**< 参数值 */
} clt_task_param_t;

/**
 * @brief 任务解析结果
 */
typedef struct {
    clt_task_intent_t intent;            /**< 任务意图 */
    char** required_skills;              /**< 需要的技能列表 */
    size_t skill_count;                  /**< 技能数量 */
    clt_task_param_t* params;            /**< 任务参数列表 */
    size_t param_count;                  /**< 参数数量 */
    char* raw_plan;                      /**< 原始执行计划（JSON字符串） */
} clt_cognition_result_t;

/* ==================== 公共接口 ==================== */

/**
 * @brief 初始化认知层
 * @return 成功返回true，失败返回false
 */
bool clt_cognition_init(void);

/**
 * @brief 清理认知层资源
 */
void clt_cognition_cleanup(void);

/**
 * @brief 处理任务数据，生成认知结果
 * @param task_data 任务数据（JSON格式字符串）
 * @param task_data_len 任务数据长度
 * @return 认知结果字符串（JSON格式），需要调用clt_cognition_free_result释放
 */
char* clt_cognition_process(const char* task_data, size_t task_data_len);

/**
 * @brief 解析认知结果字符串为结构化结果
 * @param result_json 认知结果JSON字符串
 * @return 结构化结果指针，需要调用clt_cognition_free_parsed_result释放
 */
clt_cognition_result_t* clt_cognition_parse_result(const char* result_json);

/**
 * @brief 获取任务意图描述
 * @param intent 任务意图枚举值
 * @return 意图描述字符串
 */
const char* clt_cognition_get_intent_description(clt_task_intent_t intent);

/**
 * @brief 评估任务复杂度
 * @param result 认知结果
 * @return 复杂度分数（0.0-1.0）
 */
float clt_cognition_evaluate_complexity(const clt_cognition_result_t* result);

/**
 * @brief 释放认知结果字符串
 * @param result 认知结果字符串
 */
void clt_cognition_free_result(char* result);

/**
 * @brief 释放结构化认知结果
 * @param result 结构化认知结果指针
 */
void clt_cognition_free_parsed_result(clt_cognition_result_t* result);

/**
 * @brief 获取最后错误信息
 * @return 错误信息字符串
 */
const char* clt_cognition_get_last_error(void);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_COGNITION_LITE_H */