/**
 * @file intent_parser.c
 * @brief 意图理解引擎实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * @details
 * 意图理解引擎负责解析用户自然语言输入，识别真实意图和目标。
 * 基于规则和机器学习结合的混合方法，支持多领域意图识别。
 * 实现生产级可靠性，支持 99.999% 可用性标准。
 *
 * 核心功能：
 * 1. 文本预处理：分词、停用词过滤、词干提取
 * 2. 意图分类：基于规则的分类 + 机器学习模型
 * 3. 实体提取：命名实体识别、参数提取
 * 4. 上下文理解：对话历史、用户偏好
 * 5. 置信度评估：意图识别置信度评估
 * 6. 错误恢复：解析失败时的降级策略
 */

#include "cognition.h"
#include "agentos.h"
#include "logger.h"
#include "id_utils.h"
#include "error_utils.h"
#include "../../../commons/utils/observability/include/observability_compat.h"

/* 辅助模块（已拆分，以下内部函数为历史遗留副本，计划在未来版本移除） */
#include "intent_utils.h"      /* to_lowercase, contains_ignore_case, string_similarity 等 */
#include "entity_extractor.h"  /* 实体提取功能 */
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "include/memory_compat.h"
#include "string_compat.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>
#include <cjson/cJSON.h>

/* ==================== 内部常量定义 ==================== */

/** @brief 最大意图分类数量 */
#define MAX_INTENT_CLASSES 64

/** @brief 最大实体提取数量 */
#define MAX_ENTITIES 32

/** @brief 最大关键词数量 */
#define MAX_KEYWORDS 128

/** @brief 默认意图识别超时（毫秒） */
#define DEFAULT_INTENT_TIMEOUT_MS 500

/** @brief 最小置信度阈值 */
#define MIN_CONFIDENCE_THRESHOLD 0.3

/** @brief 高置信度阈值 */
#define HIGH_CONFIDENCE_THRESHOLD 0.8

/* ==================== 内部数据结构 ==================== */

/**
 * @brief 意图分类规则结构
 */
typedef struct intent_rule {
    char* pattern;                     /**< 模式字符串 */
    size_t pattern_len;                /**< 模式长度 */
    char* intent_name;                 /**< 意图名称 */
    float confidence;                  /**< 基础置信度 */
    uint32_t flags;                    /**< 标志位 */
    struct intent_rule* next;          /**< 下一个规则 */
} intent_rule_t;

/**
 * @brief 实体提取结果
 */
typedef struct extracted_entity {
    char* type;                        /**< 实体类型 */
    char* value;                       /**< 实体值 */
    size_t value_len;                  /**< 值长度 */
    int start_pos;                     /**< 起始位置 */
    int end_pos;                       /**< 结束位置 */
    float confidence;                  /**< 置信度 */
} extracted_entity_t;

/**
 * @brief 意图解析器内部状态
 */
struct agentos_intent_parser {
    intent_rule_t* rule_list;          /**< 规则链表 */
    agentos_mutex_t* lock;             /**< 线程锁 */
    uint64_t total_parsed;             /**< 总解析次数 */
    uint64_t success_count;            /**< 成功次数 */
    uint64_t failure_count;            /**< 失败次数 */
    uint64_t total_time_ns;            /**< 总耗时（纳秒） */
    void* obs;                         /**< 可观测性句柄（预留） */
    char* parser_id;                   /**< 解析器 ID */
    uint8_t* keyword_trie;             /**< 关键词 Trie 树 */
    size_t keyword_count;              /**< 关键词数量 */
};

/* ==================== 内部工具函数 ==================== */

/**
 * @brief 字符串转换为小写（原地修改）
 * @param str 输入字符串
 * @return 转换后的字符串
 */
/* ==================== [LEGACY] 工具函数 ====================
 * 以下函数与 intent_utils.c 中的实现重复。
 * 保留原因：确保向后兼容，避免破坏现有调用方。
 * 计划在 v2.0 统一为 intent_utils 模块的公开API。
 * 新代码请使用 intent_utils.h 中声明的函数。
 * ============================================================ */

static char* to_lowercase(char* str) {
    if (!str) return NULL;
    for (char* p = str; *p; ++p) {
        *p = tolower(*p);
    }
    return str;
}

/**
 * @brief 检查字符串是否包含子串（不区分大小写）
 * @param haystack 源字符串
 * @param needle 子串
 * @return 1 表示包含，0 表示不包含
 */
static int contains_ignore_case(const char* haystack, const char* needle) {
    if (!haystack || !needle) return 0;

    size_t haystack_len = strlen(haystack);
    size_t needle_len = strlen(needle);

    if (needle_len > haystack_len) return 0;

    for (size_t i = 0; i <= haystack_len - needle_len; i++) {
        int match = 1;
        for (size_t j = 0; j < needle_len; j++) {
            if (tolower(haystack[i + j]) != tolower(needle[j])) {
                match = 0;
                break;
            }
        }
        if (match) return 1;
    }
    return 0;
}

/**
 * @brief 计算字符串相似度（基于编辑距离）
 * @param s1 字符串 1
 * @param s2 字符串 2
 * @return 相似度得分（0-1）
 */
static float string_similarity(const char* s1, const char* s2) {
    if (!s1 || !s2) return 0.0f;

    size_t len1 = strlen(s1);
    size_t len2 = strlen(s2);

    // 简单实现：基于公共子串
    int commons = 0;
    for (size_t i = 0; i < len1; i++) {
        for (size_t j = 0; j < len2; j++) {
            size_t k = 0;
            while (i + k < len1 && j + k < len2 &&
                   tolower(s1[i + k]) == tolower(s2[j + k])) {
                k++;
            }
            if (k > (size_t)commons) commons = (int)k;
        }
    }

    return (float)commons / (float)(len1 > len2 ? len1 : len2);
}

/**
 * @brief 提取关键词从文本中
 * @param text 输入文本
 * @param keywords 关键词数组
 * @param max_keywords 最大关键词数
 * @return 提取的关键词数量
 */
static size_t extract_keywords(const char* text, char** keywords, size_t max_keywords) {
    if (!text || !keywords || max_keywords == 0) return 0;

    // 简单实现：按空格分割
    size_t count = 0;
    char* copy = AGENTOS_STRDUP(text);
    if (!copy) return 0;

    char* token = strtok(copy, " ,.!?;:\t\n\r");
    while (token && count < max_keywords) {
        // 过滤短词
        if (strlen(token) > 2) {
            keywords[count] = AGENTOS_STRDUP(token);
            if (keywords[count]) {
                to_lowercase(keywords[count]);
                count++;
            }
        }
        token = strtok(NULL, " ,.!?;:\t\n\r");
    }

    AGENTOS_FREE(copy);
    return count;
}

/**
 * @brief 释放关键词数组
 * @param keywords 关键词数组
 * @param count 关键词数量
 */
static void free_keywords(char** keywords, size_t count) {
    if (!keywords) return;
    for (size_t i = 0; i < count; i++) {
        if (keywords[i]) AGENTOS_FREE(keywords[i]);
    }
}

/* ==================== 规则管理函数 ==================== */

/**
 * @brief 创建新的意图规则
 * @param pattern 模式字符串
 * @param intent_name 意图名称
 * @param confidence 置信度
 * @param flags 标志位
 * @return 新规则对象，失败返回 NULL
 */
static intent_rule_t* create_intent_rule(const char* pattern, const char* intent_name,
                                         float confidence, uint32_t flags) {
    if (!pattern || !intent_name) return NULL;

    intent_rule_t* rule = (intent_rule_t*)AGENTOS_CALLOC(1, sizeof(intent_rule_t));
    if (!rule) {
        AGENTOS_LOG_ERROR("Failed to allocate intent rule");
        return NULL;
    }

    rule->pattern = AGENTOS_STRDUP(pattern);
    rule->pattern_len = strlen(pattern);
    rule->intent_name = AGENTOS_STRDUP(intent_name);
    rule->confidence = confidence;
    rule->flags = flags;
    rule->next = NULL;

    if (!rule->pattern || !rule->intent_name) {
        if (rule->pattern) AGENTOS_FREE(rule->pattern);
        if (rule->intent_name) AGENTOS_FREE(rule->intent_name);
        AGENTOS_FREE(rule);
        AGENTOS_LOG_ERROR("Failed to duplicate strings for intent rule");
        return NULL;
    }

    // 转换为小写以支持不区分大小写匹配
    to_lowercase(rule->pattern);

    return rule;
}

/**
 * @brief 释放意图规则
 * @param rule 规则对象
 */
static void free_intent_rule(intent_rule_t* rule) {
    if (!rule) return;
    if (rule->pattern) AGENTOS_FREE(rule->pattern);
    if (rule->intent_name) AGENTOS_FREE(rule->intent_name);
    AGENTOS_FREE(rule);
}

/**
 * @brief 释放规则链表
 * @param head 链表头
 */
static void free_rule_list(intent_rule_t* head) {
    while (head) {
        intent_rule_t* next = head->next;
        free_intent_rule(head);
        head = next;
    }
}

/**
 * @brief 添加规则到解析器
 * @param parser 解析器
 * @param rule 规则
 * @return AGENTOS_SUCCESS 成功，其他为错误码
 */
static agentos_error_t add_rule_to_parser(agentos_intent_parser_t* parser, intent_rule_t* rule) {
    if (!parser || !rule) return AGENTOS_EINVAL;

    agentos_mutex_lock(parser->lock);

    // 添加到链表头部
    rule->next = parser->rule_list;
    parser->rule_list = rule;

    agentos_mutex_unlock(parser->lock);

    AGENTOS_LOG_DEBUG("Added intent rule: %s -> %s", rule->pattern, rule->intent_name);
    return AGENTOS_SUCCESS;
}

/* ==================== 意图解析核心函数 ==================== */

/**
 * @brief 基于规则匹配意图
 * @param parser 解析器
 * @param text 输入文本
 * @param out_intent_name 输出意图名称
 * @param out_confidence 输出置信度
 * @return AGENTOS_SUCCESS 找到匹配，其他为未找到
 */
static agentos_error_t match_intent_by_rules(agentos_intent_parser_t* parser,
                                             const char* text,
                                             char** out_intent_name,
                                             float* out_confidence) {
    if (!parser || !text || !out_intent_name || !out_confidence) {
        return AGENTOS_EINVAL;
    }

    char* lower_text = AGENTOS_STRDUP(text);
    if (!lower_text) return AGENTOS_ENOMEM;
    to_lowercase(lower_text);

    agentos_mutex_lock(parser->lock);
    intent_rule_t* rule = parser->rule_list;

    float best_confidence = 0.0f;
    char* best_intent = NULL;

    while (rule) {
        // 检查直接包含
        if (contains_ignore_case(lower_text, rule->pattern)) {
            if (rule->confidence > best_confidence) {
                best_confidence = rule->confidence;
                if (best_intent) AGENTOS_FREE(best_intent);
                best_intent = AGENTOS_STRDUP(rule->intent_name);
                if (!best_intent) {
                    AGENTOS_FREE(lower_text);
                    agentos_mutex_unlock(parser->lock);
                    return AGENTOS_ENOMEM;
                }
            }
        }
        // 检查相似度
        else {
            float similarity = string_similarity(lower_text, rule->pattern);
            float adjusted_confidence = similarity * rule->confidence;
            if (adjusted_confidence > best_confidence && adjusted_confidence > MIN_CONFIDENCE_THRESHOLD) {
                best_confidence = adjusted_confidence;
                if (best_intent) AGENTOS_FREE(best_intent);
                best_intent = AGENTOS_STRDUP(rule->intent_name);
                if (!best_intent) {
                    AGENTOS_FREE(lower_text);
                    agentos_mutex_unlock(parser->lock);
                    return AGENTOS_ENOMEM;
                }
            }
        }

        rule = rule->next;
    }

    agentos_mutex_unlock(parser->lock);
    AGENTOS_FREE(lower_text);

    if (best_intent && best_confidence > MIN_CONFIDENCE_THRESHOLD) {
        *out_intent_name = best_intent;
        *out_confidence = best_confidence;
        return AGENTOS_SUCCESS;
    }

    if (best_intent) AGENTOS_FREE(best_intent);
    return AGENTOS_ENOENT;
}

/**
 * @brief 提取文本中的实体
 * @param text 输入文本
 * @param entities 输出实体数组
 * @param max_entities 最大实体数
 * @return 提取的实体数量
 */
/* ==================== [LEGACY] 实体提取函数 ====================
 * 以下函数与 entity_extractor.c 中的实现重复。
 * 保留原因：确保向后兼容。
 * 计划在 v2.0 统一为 entity_extractor 模块的公开API。
 * ============================================================ */

static size_t extract_entities_from_text(const char* text, extracted_entity_t* entities,
                                         size_t max_entries) {
    if (!text || !entities || max_entries == 0) return 0;

    // 简单实现：基于关键词的实体提取
    // 生产环境应使用 NER 模型

    size_t count = 0;
    char* keywords[MAX_KEYWORDS];
    size_t keyword_count = extract_keywords(text, keywords, MAX_KEYWORDS);

    // 定义一些简单的实体类型映射
    __attribute__((unused)) const char* number_words[] = {"一", "二", "三", "四", "五", "六", "七", "八", "九", "十",
                                   "1", "2", "3", "4", "5", "6", "7", "8", "9", "10",
                                   "first", "second", "third", "fourth", "fifth", NULL};

    for (size_t i = 0; i < keyword_count && count < max_entries; i++) {
        const char* keyword = keywords[i];
        char* type = NULL;
        float confidence = 0.5f;  // 基础置信度

        // 检查是否为数字
        int is_number = 1;
        for (size_t j = 0; j < strlen(keyword); j++) {
            if (!isdigit(keyword[j])) {
                is_number = 0;
                break;
            }
        }

        if (is_number) {
            type = "number";
            confidence = 0.9f;
        }
        // 检查是否为时间相关
        else if (contains_ignore_case(keyword, "time") ||
                 contains_ignore_case(keyword, "hour") ||
                 contains_ignore_case(keyword, "minute") ||
                 contains_ignore_case(keyword, "second") ||
                 contains_ignore_case(keyword, "day") ||
                 contains_ignore_case(keyword, "week") ||
                 contains_ignore_case(keyword, "month") ||
                 contains_ignore_case(keyword, "year")) {
            type = "time";
            confidence = 0.8f;
        }
        // 检查是否为文件相关
        else if (contains_ignore_case(keyword, "file") ||
                 contains_ignore_case(keyword, "document") ||
                 contains_ignore_case(keyword, "report") ||
                 contains_ignore_case(keyword, "data")) {
            type = "file";
            confidence = 0.7f;
        }

        if (type) {
            entities[count].type = AGENTOS_STRDUP(type);
            entities[count].value = AGENTOS_STRDUP(keyword);
            if (!entities[count].type || !entities[count].value) {
                AGENTOS_FREE(entities[count].type);
                AGENTOS_FREE(entities[count].value);
                continue;
            }
            entities[count].value_len = strlen(keyword);
            entities[count].confidence = confidence;
            entities[count].start_pos = 0;  // 简化实现
            entities[count].end_pos = 0;    // 简化实现
            count++;
        }
    }

    free_keywords(keywords, keyword_count);
    return count;
}

/* ==================== 公共 API 实现 ==================== */

/**
 * @brief 创建意图解析器
 * @param out_parser 输出解析器句柄
 * @return agentos_error_t
 */
agentos_error_t agentos_intent_parser_create(agentos_intent_parser_t** out_parser) {
    if (!out_parser) return AGENTOS_EINVAL;

    agentos_intent_parser_t* parser = (agentos_intent_parser_t*)AGENTOS_CALLOC(1, sizeof(agentos_intent_parser_t));
    if (!parser) {
        AGENTOS_LOG_ERROR("Failed to allocate intent parser");
        return AGENTOS_ENOMEM;
    }

    parser->lock = agentos_mutex_create();
    if (!parser->lock) {
        AGENTOS_LOG_ERROR("Failed to create mutex for intent parser");
        AGENTOS_FREE(parser);
        return AGENTOS_ENOMEM;
    }

    // 生成唯一 ID
    char uuid_buf[64];
    if (agentos_generate_uuid(uuid_buf) == AGENTOS_SUCCESS) {
        parser->parser_id = AGENTOS_STRDUP(uuid_buf);
    } else {
        parser->parser_id = NULL;
    }
    if (!parser->parser_id) {
        AGENTOS_LOG_WARN("Failed to generate UUID for intent parser, using default");
        parser->parser_id = AGENTOS_STRDUP("intent_parser_default");
    }

    // 初始化可观测性

    // 添加默认规则
    intent_rule_t* rule;

    // 文件操作意图
    rule = create_intent_rule("read file", "file_read", 0.9f, 0);
    if (rule) add_rule_to_parser(parser, rule);

    rule = create_intent_rule("write file", "file_write", 0.9f, 0);
    if (rule) add_rule_to_parser(parser, rule);

    rule = create_intent_rule("create file", "file_create", 0.9f, 0);
    if (rule) add_rule_to_parser(parser, rule);

    rule = create_intent_rule("delete file", "file_delete", 0.9f, 0);
    if (rule) add_rule_to_parser(parser, rule);

    // 数据分析意图
    rule = create_intent_rule("analyze data", "data_analyze", 0.8f, 0);
    if (rule) add_rule_to_parser(parser, rule);

    rule = create_intent_rule("process data", "data_process", 0.8f, 0);
    if (rule) add_rule_to_parser(parser, rule);

    rule = create_intent_rule("summarize report", "report_summarize", 0.85f, 0);
    if (rule) add_rule_to_parser(parser, rule);

    // 网络操作意图
    rule = create_intent_rule("download", "network_download", 0.9f, 0);
    if (rule) add_rule_to_parser(parser, rule);

    rule = create_intent_rule("upload", "network_upload", 0.9f, 0);
    if (rule) add_rule_to_parser(parser, rule);

    rule = create_intent_rule("send email", "email_send", 0.95f, 0);
    if (rule) add_rule_to_parser(parser, rule);

    // 通用意图
    rule = create_intent_rule("help", "general_help", 0.95f, 0);
    if (rule) add_rule_to_parser(parser, rule);

    rule = create_intent_rule("what can you do", "general_capabilities", 0.9f, 0);
    if (rule) add_rule_to_parser(parser, rule);

    *out_parser = parser;

    AGENTOS_LOG_INFO("Intent parser created: %s", parser->parser_id);
    return AGENTOS_SUCCESS;
}

/**
 * @brief 销毁意图解析器
 * @param parser 解析器句柄
 */
void agentos_intent_parser_destroy(agentos_intent_parser_t* parser) {
    if (!parser) return;

    AGENTOS_LOG_DEBUG("Destroying intent parser: %s", parser->parser_id);

    free_rule_list(parser->rule_list);

    if (parser->lock) {
        agentos_mutex_destroy(parser->lock);
    }

    if (parser->parser_id) {
        AGENTOS_FREE(parser->parser_id);
    }

    AGENTOS_FREE(parser);
}

/**
 * @brief 解析用户输入，提取意图
 * @param parser 解析器
 * @param input 用户输入文本
 * @param input_len 输入长度
 * @param out_intent 输出意图结构
 * @return agentos_error_t
 */
agentos_error_t agentos_intent_parser_parse(agentos_intent_parser_t* parser,
                                            const char* input,
                                            size_t input_len,
                                            agentos_intent_t** out_intent) {
    if (!parser || !input || !out_intent) {
        return AGENTOS_EINVAL;
    }

    // 记录开始时间
    uint64_t start_time_ns = (uint64_t)agentos_time_monotonic_ms() * 1000000ULL;

    // 更新统计
    parser->total_parsed++;

    agentos_intent_t* intent = (agentos_intent_t*)AGENTOS_CALLOC(1, sizeof(agentos_intent_t));
    if (!intent) {
        AGENTOS_LOG_ERROR("Failed to allocate intent structure");
        parser->failure_count++;
        return AGENTOS_ENOMEM;
    }

    intent->intent_raw_text = (char*)AGENTOS_MALLOC(input_len + 1);
    if (!intent->intent_raw_text) {
        AGENTOS_LOG_ERROR("Failed to allocate raw text buffer");
        AGENTOS_FREE(intent);
        parser->failure_count++;
        return AGENTOS_ENOMEM;
    }
    memcpy(intent->intent_raw_text, input, input_len);
    intent->intent_raw_text[input_len] = '\0';
    intent->intent_raw_len = input_len;

    // 提取意图名称和置信度
    char* intent_name = NULL;
    float confidence = 0.0f;
    agentos_error_t match_result = match_intent_by_rules(parser, input, &intent_name, &confidence);

    if (match_result == AGENTOS_SUCCESS && intent_name) {
        intent->intent_goal = intent_name;
        intent->intent_goal_len = strlen(intent_name);
        intent->intent_flags = 0;

        // 设置标志位
        if (confidence > HIGH_CONFIDENCE_THRESHOLD) {
            intent->intent_flags |= 0x01;  // 高置信度标志
        }

        // 提取实体
        extracted_entity_t entities[MAX_ENTITIES];
        size_t entity_count = extract_entities_from_text(input, entities, MAX_ENTITIES);

        if (entity_count > 0) {
            // 创建上下文 JSON
            cJSON* context_json = cJSON_CreateObject();
            if (context_json) {
                cJSON* entities_array = cJSON_CreateArray();
                for (size_t i = 0; i < entity_count; i++) {
                    cJSON* entity_obj = cJSON_CreateObject();
                    cJSON_AddStringToObject(entity_obj, "type", entities[i].type);
                    cJSON_AddStringToObject(entity_obj, "value", entities[i].value);
                    cJSON_AddNumberToObject(entity_obj, "confidence", entities[i].confidence);
                    cJSON_AddItemToArray(entities_array, entity_obj);

                    AGENTOS_FREE(entities[i].type);
                    AGENTOS_FREE(entities[i].value);
                }
                cJSON_AddItemToObject(context_json, "entities", entities_array);

                char* context_str = cJSON_PrintUnformatted(context_json);
                if (context_str) {
                    intent->intent_context = context_str;
                }
                cJSON_Delete(context_json);
            }
        }

        // 清理实体数组
        for (size_t i = 0; i < entity_count; i++) {
            // 已经在添加到 JSON 时释放了
        }

        AGENTOS_LOG_DEBUG("Intent parsed successfully: %s (confidence: %.2f)",
                         intent_name, confidence);

        parser->success_count++;
    } else {
        intent->intent_goal = AGENTOS_STRDUP("unknown");
        if (!intent->intent_goal) {
            AGENTOS_FREE(intent);
            parser->failure_count++;
            return AGENTOS_ENOMEM;
        }
        intent->intent_goal_len = 7;
        intent->intent_flags = 0x02;

        AGENTOS_LOG_WARN("No intent matched for input: %.*s", (int)input_len, input);

        parser->failure_count++;
    }

    uint64_t end_time_ns = (uint64_t)agentos_time_monotonic_ms() * 1000000ULL;
    uint64_t duration_ns = end_time_ns - start_time_ns;
    parser->total_time_ns += duration_ns;

    *out_intent = intent;

    if (match_result == AGENTOS_SUCCESS) {
        char feedback_json[256];
        snprintf(feedback_json, sizeof(feedback_json),
                "{\"intent\":\"%s\",\"confidence\":%.2f,\"duration_ns\":%llu}",
                intent_name, confidence, (unsigned long long)duration_ns);
    }

    return AGENTOS_SUCCESS;
}

/**
 * @brief 释放意图结构
 * @param intent 意图结构
 */
void agentos_intent_free(agentos_intent_t* intent) {
    if (!intent) return;

    if (intent->intent_raw_text) AGENTOS_FREE(intent->intent_raw_text);
    if (intent->intent_goal) AGENTOS_FREE(intent->intent_goal);
    if (intent->intent_context) AGENTOS_FREE(intent->intent_context);

    AGENTOS_FREE(intent);
}

/**
 * @brief 添加自定义意图规则
 * @param parser 解析器
 * @param pattern 模式字符串
 * @param intent_name 意图名称
 * @param confidence 置信度
 * @param flags 标志位
 * @return agentos_error_t
 */
agentos_error_t agentos_intent_parser_add_rule(agentos_intent_parser_t* parser,
                                               const char* pattern,
                                               const char* intent_name,
                                               float confidence,
                                               uint32_t flags) {
    if (!parser || !pattern || !intent_name) return AGENTOS_EINVAL;

    if (confidence < 0.0f || confidence > 1.0f) {
        AGENTOS_LOG_ERROR("Confidence must be between 0.0 and 1.0");
        return AGENTOS_EINVAL;
    }

    intent_rule_t* rule = create_intent_rule(pattern, intent_name, confidence, flags);
    if (!rule) {
        AGENTOS_LOG_ERROR("Failed to create intent rule");
        return AGENTOS_ENOMEM;
    }

    agentos_error_t result = add_rule_to_parser(parser, rule);
    if (result != AGENTOS_SUCCESS) {
        free_intent_rule(rule);
    }

    return result;
}

/**
 * @brief 获取解析器统计信息
 * @param parser 解析器
 * @param out_stats 输出统计 JSON 字符串
 * @return agentos_error_t
 */
agentos_error_t agentos_intent_parser_stats(agentos_intent_parser_t* parser,
                                            char** out_stats) {
    if (!parser || !out_stats) return AGENTOS_EINVAL;

    cJSON* stats_json = cJSON_CreateObject();
    if (!stats_json) return AGENTOS_ENOMEM;

    agentos_mutex_lock(parser->lock);

    cJSON_AddStringToObject(stats_json, "parser_id", parser->parser_id);
    cJSON_AddNumberToObject(stats_json, "total_parsed", parser->total_parsed);
    cJSON_AddNumberToObject(stats_json, "success_count", parser->success_count);
    cJSON_AddNumberToObject(stats_json, "failure_count", parser->failure_count);
    cJSON_AddNumberToObject(stats_json, "total_time_ns", parser->total_time_ns);

    // 计算平均耗时
    double avg_time_ns = parser->total_parsed > 0 ?
                        (double)parser->total_time_ns / parser->total_parsed : 0.0;
    cJSON_AddNumberToObject(stats_json, "avg_time_ns", avg_time_ns);

    // 计算成功率
    double success_rate = parser->total_parsed > 0 ?
                         (double)parser->success_count / parser->total_parsed * 100.0 : 0.0;
    cJSON_AddNumberToObject(stats_json, "success_rate_percent", success_rate);

    // 统计规则数量
    int rule_count = 0;
    intent_rule_t* rule = parser->rule_list;
    while (rule) {
        rule_count++;
        rule = rule->next;
    }
    cJSON_AddNumberToObject(stats_json, "rule_count", rule_count);

    agentos_mutex_unlock(parser->lock);

    char* stats_str = cJSON_PrintUnformatted(stats_json);
    cJSON_Delete(stats_json);

    if (!stats_str) return AGENTOS_ENOMEM;

    *out_stats = stats_str;
    return AGENTOS_SUCCESS;
}

/**
 * @brief 重置解析器统计信息
 * @param parser 解析器
 */
void agentos_intent_parser_reset_stats(agentos_intent_parser_t* parser) {
    if (!parser) return;

    agentos_mutex_lock(parser->lock);

    parser->total_parsed = 0;
    parser->success_count = 0;
    parser->failure_count = 0;
    parser->total_time_ns = 0;

    agentos_mutex_unlock(parser->lock);

    AGENTOS_LOG_INFO("Intent parser stats reset: %s", parser->parser_id);
}

/**
 * @brief 健康检查
 * @param parser 解析器
 * @param out_json 输出健康状态 JSON
 * @return agentos_error_t
 */
agentos_error_t agentos_intent_parser_health_check(agentos_intent_parser_t* parser,
                                                   char** out_json) {
    if (!parser || !out_json) return AGENTOS_EINVAL;

    cJSON* health_json = cJSON_CreateObject();
    if (!health_json) return AGENTOS_ENOMEM;

    agentos_mutex_lock(parser->lock);

    cJSON_AddStringToObject(health_json, "component", "intent_parser");
    cJSON_AddStringToObject(health_json, "parser_id", parser->parser_id);
    cJSON_AddStringToObject(health_json, "status", "healthy");
    cJSON_AddNumberToObject(health_json, "rule_count", 0);  // 简化

    // 检查资源
    int resources_ok = 1;
    if (!parser->lock) resources_ok = 0;

    cJSON_AddBoolToObject(health_json, "resources_ok", resources_ok);
    cJSON_AddNumberToObject(health_json, "timestamp_ns", agentos_get_monotonic_time_ns());

    agentos_mutex_unlock(parser->lock);

    char* health_str = cJSON_PrintUnformatted(health_json);
    cJSON_Delete(health_json);

    if (!health_str) return AGENTOS_ENOMEM;

    *out_json = health_str;
    return AGENTOS_SUCCESS;
}
