/**
 * @file entity_extractor.h
 * @brief 实体提取器接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTRT_ENTITY_EXTRACTOR_H
#define AGENTRT_ENTITY_EXTRACTOR_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 实体类型枚举
 */
typedef enum agentrt_entity_type {
    AGENTRT_ENTITY_UNKNOWN = 0,
    AGENTRT_ENTITY_PERSON = 1,       /* 人名 */
    AGENTRT_ENTITY_ORGANIZATION = 2, /* 组织 */
    AGENTRT_ENTITY_LOCATION = 3,     /* 地点 */
    AGENTRT_ENTITY_TIME = 4,         /* 时间 */
    AGENTRT_ENTITY_DATE = 5,         /* 日期 */
    AGENTRT_ENTITY_NUMBER = 6,       /* 数字 */
    AGENTRT_ENTITY_URL = 7,          /* URL */
    AGENTRT_ENTITY_EMAIL = 8,        /* 邮箱 */
    AGENTRT_ENTITY_FILEPATH = 9,     /* 文件路径 */
    AGENTRT_ENTITY_COMMAND = 10,     /* 命令 */
    AGENTRT_ENTITY_PARAMETER = 11,   /* 参数 */
    AGENTRT_ENTITY_MAX
} agentrt_entity_type_t;

/**
 * @brief 实体结构
 */
typedef struct agentrt_entity {
    agentrt_entity_type_t type;
    const char *type_name;
    char *value;
    size_t value_len;
    int start_pos;
    int end_pos;
    float confidence;
} agentrt_entity_t;

/**
 * @brief 实体提取结果
 */
typedef struct agentrt_extraction_result {
    agentrt_entity_t *entities;
    size_t entity_count;
    size_t capacity;
} agentrt_extraction_result_t;

/**
 * @brief 初始化实体提取器
 * @return 成功返回 0，失败返回错误码
 */
int agentrt_entity_extractor_init(void);

/**
 * @brief 清理实体提取器
 */
void agentrt_entity_extractor_cleanup(void);

/**
 * @brief 从文本中提取实体
 * @param input 输入文本
 * @param input_len 输入长度
 * @param result 输出结果
 * @return 成功返回 0，失败返回错误码
 */
int agentrt_entity_extract(const char *input, size_t input_len,
                           agentrt_extraction_result_t *result);

/**
 * @brief 创建空的提取结果
 * @param initial_capacity 初始容量
 * @return 结果指针，失败返回 NULL
 */
agentrt_extraction_result_t *agentrt_extraction_result_create(size_t initial_capacity);

/**
 * @brief 销毁提取结果
 * @param result 结果指针
 */
void agentrt_extraction_result_destroy(agentrt_extraction_result_t *result);

/**
 * @brief 添加实体到结果
 * @param result 结果指针
 * @param entity 实体数据
 * @return 成功返回 0，失败返回错误码
 */
int agentrt_extraction_result_add(agentrt_extraction_result_t *result,
                                  const agentrt_entity_t *entity);

/**
 * @brief 获取实体类型名称
 * @param type 实体类型
 * @return 类型名称字符串
 */
const char *agentrt_entity_type_name(agentrt_entity_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* AGENTRT_ENTITY_EXTRACTOR_H */
