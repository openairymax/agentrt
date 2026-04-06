/**
 * @file intent_classifier.h
 * @brief 意图分类器接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_INTENT_CLASSIFIER_H
#define AGENTOS_INTENT_CLASSIFIER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 意图类型枚举
 */
typedef enum agentos_intent_type {
    AGENTOS_INTENT_UNKNOWN = 0,
    AGENTOS_INTENT_QUERY = 1,
    AGENTOS_INTENT_COMMAND = 2,
    AGENTOS_INTENT_EXPLANATION = 3,
    AGENTOS_INTENT_CREATION = 4,
    AGENTOS_INTENT_MODIFICATION = 5,
    AGENTOS_INTENT_DELETION = 6,
    AGENTOS_INTENT_CONFIRMATION = 7,
    AGENTOS_INTENT_NEGATION = 8,
    AGENTOS_INTENT_GREETING = 9,
    AGENTOS_INTENT_FAREWELL = 10,
    AGENTOS_INTENT_MAX
} agentos_intent_type_t;

/**
 * @brief 意图分类结果
 */
typedef struct agentos_intent_classification {
    agentos_intent_type_t type;
    float confidence;
    const char* type_name;
} agentos_intent_classification_t;

/**
 * @brief 初始化意图分类器
 * @return 成功返回 0，失败返回错误码
 */
int agentos_intent_classifier_init(void);

/**
 * @brief 清理意图分类器
 */
void agentos_intent_classifier_cleanup(void);

/**
 * @brief 分类用户意图
 * @param input 用户输入文本
 * @param input_len 输入长度
 * @param result 输出分类结果
 * @return 成功返回 0，失败返回错误码
 */
int agentos_intent_classify(const char* input, size_t input_len, 
                            agentos_intent_classification_t* result);

/**
 * @brief 获取意图类型名称
 * @param type 意图类型
 * @return 类型名称字符串
 */
const char* agentos_intent_type_name(agentos_intent_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_INTENT_CLASSIFIER_H */
