/**
 * @file intent_classifier.c
 * @brief 意图分类器实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "intent_classifier.h"
#include "intent_utils.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

static int g_classifier_initialized = 0;

/* 查询关键词 */
static const char* g_query_keywords[] = {
    "what", "where", "when", "why", "how", "who", "which",
    "查询", "搜索", "查找", "什么", "哪里", "何时", "为什么",
    NULL
};

/* 命令关键词 */
static const char* g_command_keywords[] = {
    "create", "delete", "update", "modify", "set", "get",
    "创建", "删除", "更新", "修改", "设置", "获取",
    "执行", "运行", "启动", "停止", "暂停", "继续",
    NULL
};

/* 确认关键词 */
static const char* g_confirm_keywords[] = {
    "yes", "ok", "sure", "confirm", "agree", "accept",
    "是", "好的", "确定", "同意", "接受", "确认",
    "对", "正确", "没错",
    NULL
};

/* 否定关键词 */
static const char* g_negate_keywords[] = {
    "no", "not", "cancel", "deny", "reject", "decline",
    "不", "取消", "否认", "拒绝", "否定",
    "错误", "不对", "不行",
    NULL
};

/* 问候关键词 */
static const char* g_greet_keywords[] = {
    "hello", "hi", "hey", "good morning", "good afternoon",
    "你好", "您好", "早上好", "下午好", "晚上好",
    NULL
};

/* 告别关键词 */
static const char* g_farewell_keywords[] = {
    "bye", "goodbye", "see you", "farewell",
    "再见", "拜拜", "回头见",
    NULL
};

int agentos_intent_classifier_init(void) {
    if (g_classifier_initialized) {
        return 0;
    }
    
    g_classifier_initialized = 1;
    return 0;
}

void agentos_intent_classifier_cleanup(void) {
    g_classifier_initialized = 0;
}

const char* agentos_intent_type_name(agentos_intent_type_t type) {
    switch (type) {
        case AGENTOS_INTENT_UNKNOWN:      return "unknown";
        case AGENTOS_INTENT_QUERY:        return "query";
        case AGENTOS_INTENT_COMMAND:      return "command";
        case AGENTOS_INTENT_EXPLANATION:  return "explanation";
        case AGENTOS_INTENT_CREATION:     return "creation";
        case AGENTOS_INTENT_MODIFICATION: return "modification";
        case AGENTOS_INTENT_DELETION:     return "deletion";
        case AGENTOS_INTENT_CONFIRMATION: return "confirmation";
        case AGENTOS_INTENT_NEGATION:     return "negation";
        case AGENTOS_INTENT_GREETING:     return "greeting";
        case AGENTOS_INTENT_FAREWELL:     return "farewell";
        default:                          return "unknown";
    }
}

int agentos_intent_classify(const char* input, size_t input_len,
                            agentos_intent_classification_t* result) {
    if (!input || !result || input_len == 0) {
        return -1;
    }
    
    if (!g_classifier_initialized) {
        agentos_intent_classifier_init();
    }
    
    /* 创建输入副本并转小写 */
    char* lower_input = (char*)AGENTOS_MALLOC(input_len + 1);
    if (!lower_input) {
        return -1;
    }
    
    memcpy(lower_input, input, input_len);
    lower_input[input_len] = '\0';
    intent_to_lowercase(lower_input);
    
    /* 初始化结果 */
    memset(result, 0, sizeof(*result));
    result->type = AGENTOS_INTENT_UNKNOWN;
    result->confidence = 0.0f;
    
    /* 检查各类关键词 */
    float max_score = 0.0f;
    agentos_intent_type_t detected_type = AGENTOS_INTENT_UNKNOWN;
    
    /* 检查问候 */
    for (int i = 0; g_greet_keywords[i]; i++) {
        if (intent_contains_ignore_case(lower_input, g_greet_keywords[i])) {
            detected_type = AGENTOS_INTENT_GREETING;
            max_score = 0.95f;
            break;
        }
    }
    
    /* 检查告别 */
    if (detected_type == AGENTOS_INTENT_UNKNOWN) {
        for (int i = 0; g_farewell_keywords[i]; i++) {
            if (intent_contains_ignore_case(lower_input, g_farewell_keywords[i])) {
                detected_type = AGENTOS_INTENT_FAREWELL;
                max_score = 0.95f;
                break;
            }
        }
    }
    
    /* 检查查询 */
    if (detected_type == AGENTOS_INTENT_UNKNOWN) {
        int query_count = 0;
        for (int i = 0; g_query_keywords[i]; i++) {
            if (intent_contains_ignore_case(lower_input, g_query_keywords[i])) {
                query_count++;
            }
        }
        if (query_count > 0) {
            detected_type = AGENTOS_INTENT_QUERY;
            max_score = 0.8f + (query_count > 2 ? 0.15f : 0.05f);
        }
    }
    
    /* 检查命令 */
    if (detected_type == AGENTOS_INTENT_UNKNOWN) {
        int cmd_count = 0;
        for (int i = 0; g_command_keywords[i]; i++) {
            if (intent_contains_ignore_case(lower_input, g_command_keywords[i])) {
                cmd_count++;
            }
        }
        if (cmd_count > 0) {
            detected_type = AGENTOS_INTENT_COMMAND;
            max_score = 0.85f + (cmd_count > 2 ? 0.10f : 0.05f);
            
            /* 区分创建/修改/删除 */
            if (intent_contains_ignore_case(lower_input, "create") ||
                intent_contains_ignore_case(lower_input, "创建")) {
                detected_type = AGENTOS_INTENT_CREATION;
            } else if (intent_contains_ignore_case(lower_input, "delete") ||
                       intent_contains_ignore_case(lower_input, "删除")) {
                detected_type = AGENTOS_INTENT_DELETION;
            } else if (intent_contains_ignore_case(lower_input, "modify") ||
                       intent_contains_ignore_case(lower_input, "修改")) {
                detected_type = AGENTOS_INTENT_MODIFICATION;
            }
        }
    }
    
    /* 检查确认/否定 */
    if (detected_type == AGENTOS_INTENT_UNKNOWN) {
        for (int i = 0; g_confirm_keywords[i]; i++) {
            if (intent_contains_ignore_case(lower_input, g_confirm_keywords[i])) {
                detected_type = AGENTOS_INTENT_CONFIRMATION;
                max_score = 0.90f;
                break;
            }
        }
        
        if (detected_type == AGENTOS_INTENT_UNKNOWN) {
            for (int i = 0; g_negate_keywords[i]; i++) {
                if (intent_contains_ignore_case(lower_input, g_negate_keywords[i])) {
                    detected_type = AGENTOS_INTENT_NEGATION;
                    max_score = 0.90f;
                    break;
                }
            }
        }
    }
    
    /* 填充结果 */
    result->type = detected_type;
    result->confidence = max_score > 0 ? max_score : 0.3f; /* 默认低置信度 */
    result->type_name = agentos_intent_type_name(detected_type);
    
    AGENTOS_FREE(lower_input);
    return 0;
}
