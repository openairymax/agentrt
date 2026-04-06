/**
 * @file entity_extractor.h
 * @brief 实体提取器接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_ENTITY_EXTRACTOR_H
#define AGENTOS_ENTITY_EXTRACTOR_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 实体类型枚举
 */
typedef enum agentos_entity_type {
    AGENTOS_ENTITY_UNKNOWN = 0,
    AGENTOS_ENTITY_PERSON = 1,      /* 人名 */
    AGENTOS_ENTITY_ORGANIZATION = 2, /* 组织 */
    AGENTOS_ENTITY_LOCATION = 3,    /* 地点 */
    AGENTOS_ENTITY_TIME = 4,        /* 时间 */
    AGENTOS_ENTITY_DATE = 5,        /* 日期 */
    AGENTOS_ENTITY_NUMBER = 6,      /* 数字 */
    AGENTOS_ENTITY_URL = 7,         /* URL */
    AGENTOS_ENTITY_EMAIL = 8,       /* 邮箱 */
    AGENTOS_ENTITY_FILEPATH = 9,    /* 文件路径 */
    AGENTOS_ENTITY_COMMAND = 10,    /* 命令 */
    AGENTOS_ENTITY_PARAMETER = 11,  /* 参数 */
    AGENTOS_ENTITY_MAX
} agentos_entity_type_t;

/**
 * @brief 实体结构
 */
typedef struct agentos_entity {
    agentos_entity_type_t type;
    const char* type_name;
    char* value;
    size_t value_len;
    int start_pos;
    int end_pos;
    float confidence;
} agentos_entity_t;

/**
 * @brief 实体提取结果
 */
typedef struct agentos_extraction_result {
    agentos_entity_t* entities;
    size_t entity_count;
    size_t capacity;
} agentos_extraction_result_t;

/**
 * @brief 初始化实体提取器
 * @return 成功返回 0，失败返回错误码
 */
int agentos_entity_extractor_init(void);

/**
 * @brief 清理实体提取器
 */
void agentos_entity_extractor_cleanup(void);

/**
 * @brief 从文本中提取实体
 * @param input 输入文本
 * @param input_len 输入长度
 * @param result 输出结果
 * @return 成功返回 0，失败返回错误码
 */
int agentos_entity_extract(const char* input, size_t input_len,
                          agentos_extraction_result_t* result);

/**
 * @brief 创建空的提取结果
 * @param initial_capacity 初始容量
 * @return 结果指针，失败返回 NULL
 */
agentos_extraction_result_t* agentos_extraction_result_create(size_t initial_capacity);

/**
 * @brief 销毁提取结果
 * @param result 结果指针
 */
void agentos_extraction_result_destroy(agentos_extraction_result_t* result);

/**
 * @brief 添加实体到结果
 * @param result 结果指针
 * @param entity 实体数据
 * @return 成功返回 0，失败返回错误码
 */
int agentos_extraction_result_add(agentos_extraction_result_t* result,
                                  const agentos_entity_t* entity);

/**
 * @brief 获取实体类型名称
 * @param type 实体类型
 * @return 类型名称字符串
 */
const char* agentos_entity_type_name(agentos_entity_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_ENTITY_EXTRACTOR_H */
