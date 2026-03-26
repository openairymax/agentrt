/**
 * @file cognition_lite.c
 * @brief AgentOS Lite CoreLoopThree - 轻量化认知层实现
 * 
 * 轻量化认知层实现，提供简化的意图理解和任务规划功能：
 * 1. 基本JSON解析：支持简单JSON格式的任务数据
 * 2. 意图识别：基于关键词匹配识别任务意图
 * 3. 参数提取：从任务数据中提取关键参数
 * 4. 计划生成：生成简化的执行计划
 * 
 * 实现特点：
 * - 不使用外部JSON库，减少依赖和内存占用
 * - 基于关键词的简单意图识别，速度快
 * - 固定大小的内存池，减少内存分配次数
 * - 线程安全设计，支持多线程调用
 */

#include "cognition_lite.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

/* ==================== 内部常量定义 ==================== */

#define MAX_ERROR_LENGTH 256
#define MAX_INTENT_KEYWORDS 10
#define MAX_SKILL_NAME_LENGTH 64
#define MAX_PARAM_KEY_LENGTH 64
#define MAX_PARAM_VALUE_LENGTH 256
#define DEFAULT_CACHE_SIZE 16

/* ==================== 内部数据结构 ==================== */

/**
 * @brief 意图关键词映射
 */
typedef struct {
    const char* keyword;                 /**< 关键词 */
    clt_task_intent_t intent;            /**< 对应的意图 */
} intent_keyword_mapping_t;

/**
 * @brief 技能映射
 */
typedef struct {
    const char* intent_name;             /**< 意图名称 */
    const char* skill_name;              /**< 技能名称 */
} intent_skill_mapping_t;

/**
 * @brief 认知层全局状态
 */
typedef struct {
    bool initialized;                    /**< 初始化标志 */
    char last_error[MAX_ERROR_LENGTH];   /**< 最后错误信息 */
    
    /* 意图关键词映射表 */
    intent_keyword_mapping_t intent_keywords[MAX_INTENT_KEYWORDS];
    size_t intent_keyword_count;
    
    /* 意图-技能映射表 */
    intent_skill_mapping_t intent_skills[MAX_INTENT_KEYWORDS];
    size_t intent_skill_count;
    
    /* 简单缓存：最近处理的任务结果 */
    struct {
        char* task_data_hash;            /**< 任务数据哈希（简化） */
        char* cached_result;             /**< 缓存的结果 */
        size_t cached_result_len;        /**< 缓存结果长度 */
    } cache[DEFAULT_CACHE_SIZE];
    
    size_t cache_head;                   /**< 缓存头部索引（LRU算法） */
} clt_cognition_global_t;

/* ==================== 静态全局变量 ==================== */

static clt_cognition_global_t g_cognition = {0};

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 设置最后错误信息
 * @param format 错误信息格式字符串
 * @param ... 可变参数
 */
static void set_last_error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(g_cognition.last_error, sizeof(g_cognition.last_error), format, args);
    va_end(args);
    g_cognition.last_error[sizeof(g_cognition.last_error) - 1] = '\0';
}

/**
 * @brief 初始化意图关键词映射表
 */
static void init_intent_keywords(void) {
    /* 初始化意图关键词映射 */
    intent_keyword_mapping_t keywords[] = {
        {"query", CLT_INTENT_QUERY},
        {"search", CLT_INTENT_SEARCH},
        {"find", CLT_INTENT_SEARCH},
        {"get", CLT_INTENT_QUERY},
        {"update", CLT_INTENT_UPDATE},
        {"modify", CLT_INTENT_UPDATE},
        {"edit", CLT_INTENT_UPDATE},
        {"delete", CLT_INTENT_DELETE},
        {"remove", CLT_INTENT_DELETE},
        {"create", CLT_INTENT_CREATE},
        {"add", CLT_INTENT_CREATE},
        {"analyze", CLT_INTENT_ANALYZE},
        {"analyze", CLT_INTENT_ANALYZE},
        {"summarize", CLT_INTENT_SUMMARIZE},
        {"summary", CLT_INTENT_SUMMARIZE},
        {"transform", CLT_INTENT_TRANSFORM},
        {"convert", CLT_INTENT_TRANSFORM},
    };
    
    size_t count = sizeof(keywords) / sizeof(keywords[0]);
    if (count > MAX_INTENT_KEYWORDS) {
        count = MAX_INTENT_KEYWORDS;
    }
    
    for (size_t i = 0; i < count; i++) {
        g_cognition.intent_keywords[i] = keywords[i];
    }
    g_cognition.intent_keyword_count = count;
    
    /* 初始化意图-技能映射 */
    intent_skill_mapping_t skills[] = {
        {"query", "query_skill"},
        {"search", "search_skill"},
        {"update", "update_skill"},
        {"delete", "delete_skill"},
        {"create", "create_skill"},
        {"analyze", "analyze_skill"},
        {"summarize", "summarize_skill"},
        {"transform", "transform_skill"},
    };
    
    count = sizeof(skills) / sizeof(skills[0]);
    if (count > MAX_INTENT_KEYWORDS) {
        count = MAX_INTENT_KEYWORDS;
    }
    
    for (size_t i = 0; i < count; i++) {
        g_cognition.intent_skills[i] = skills[i];
    }
    g_cognition.intent_skill_count = count;
}

/**
 * @brief 简化JSON解析：提取字符串值
 * @param json JSON字符串
 * @param key 要提取的键
 * @param value 输出参数：值字符串
 * @param max_value_len 值缓冲区最大长度
 * @return 成功返回true，失败返回false
 */
static bool parse_simple_json_string(const char* json, const char* key, 
                                     char* value, size_t max_value_len) {
    if (!json || !key || !value || max_value_len == 0) {
        return false;
    }
    
    /* 构建搜索模式："key": "value" */
    char search_pattern[128];
    snprintf(search_pattern, sizeof(search_pattern), "\"%s\":\"", key);
    
    const char* pos = strstr(json, search_pattern);
    if (!pos) {
        /* 尝试不带引号的键 */
        snprintf(search_pattern, sizeof(search_pattern), "\"%s\": \"", key);
        pos = strstr(json, search_pattern);
        if (!pos) {
            return false;
        }
    }
    
    pos += strlen(search_pattern);
    const char* end = strchr(pos, '"');
    if (!end) {
        return false;
    }
    
    size_t len = (size_t)(end - pos);
    if (len >= max_value_len) {
        len = max_value_len - 1;
    }
    
    strncpy(value, pos, len);
    value[len] = '\0';
    return true;
}

/**
 * @brief 简化JSON解析：提取数字值
 * @param json JSON字符串
 * @param key 要提取的键
 * @param value 输出参数：值
 * @return 成功返回true，失败返回false
 */
static bool parse_simple_json_int(const char* json, const char* key, int* value) {
    if (!json || !key || !value) {
        return false;
    }
    
    char str_value[64];
    if (!parse_simple_json_string(json, key, str_value, sizeof(str_value))) {
        return false;
    }
    
    char* endptr;
    long int_val = strtol(str_value, &endptr, 10);
    if (endptr == str_value || *endptr != '\0') {
        return false;
    }
    
    *value = (int)int_val;
    return true;
}

/**
 * @brief 从任务数据中识别意图
 * @param task_data 任务数据（JSON格式字符串）
 * @return 识别到的意图
 */
static clt_task_intent_t detect_intent(const char* task_data) {
    if (!task_data) {
        return CLT_INTENT_UNKNOWN;
    }
    
    /* 首先尝试从JSON中提取明确的意图字段 */
    char intent_str[64];
    if (parse_simple_json_string(task_data, "intent", intent_str, sizeof(intent_str))) {
        /* 将意图字符串转换为枚举值 */
        for (size_t i = 0; i < g_cognition.intent_keyword_count; i++) {
            if (strcasecmp(intent_str, g_cognition.intent_keywords[i].keyword) == 0) {
                return g_cognition.intent_keywords[i].intent;
            }
        }
    }
    
    /* 如果没有明确的意图字段，尝试从任务内容中识别 */
    char task_content[256];
    if (parse_simple_json_string(task_data, "content", task_content, sizeof(task_content))) {
        /* 在内容中搜索关键词 */
        for (size_t i = 0; i < g_cognition.intent_keyword_count; i++) {
            if (strstr(task_content, g_cognition.intent_keywords[i].keyword) != NULL) {
                return g_cognition.intent_keywords[i].intent;
            }
        }
    }
    
    /* 如果都没有找到，尝试直接搜索整个任务数据 */
    for (size_t i = 0; i < g_cognition.intent_keyword_count; i++) {
        if (strstr(task_data, g_cognition.intent_keywords[i].keyword) != NULL) {
            return g_cognition.intent_keywords[i].intent;
        }
    }
    
    return CLT_INTENT_UNKNOWN;
}

/**
 * @brief 提取任务参数
 * @param task_data 任务数据
 * @param params 输出参数：参数数组
 * @param max_params 最大参数数量
 * @return 提取到的参数数量
 */
static size_t extract_params(const char* task_data, 
                             clt_task_param_t* params, 
                             size_t max_params) {
    if (!task_data || !params || max_params == 0) {
        return 0;
    }
    
    size_t param_count = 0;
    
    /* 预定义的参数键列表 */
    const char* param_keys[] = {
        "target", "object", "id", "name", "type", 
        "filter", "limit", "offset", "sort", "format"
    };
    size_t key_count = sizeof(param_keys) / sizeof(param_keys[0]);
    
    for (size_t i = 0; i < key_count && param_count < max_params; i++) {
        char value[256];
        if (parse_simple_json_string(task_data, param_keys[i], value, sizeof(value))) {
            params[param_count].key = strdup(param_keys[i]);
            params[param_count].value = strdup(value);
            if (params[param_count].key && params[param_count].value) {
                param_count++;
            } else {
                /* 分配失败，清理 */
                if (params[param_count].key) free(params[param_count].key);
                if (params[param_count].value) free(params[param_count].value);
            }
        }
    }
    
    return param_count;
}

/**
 * @brief 获取意图对应的技能名称
 * @param intent 任务意图
 * @return 技能名称，未找到返回NULL
 */
static const char* get_skill_for_intent(clt_task_intent_t intent) {
    const char* intent_names[] = {
        "unknown", "query", "update", "delete", 
        "create", "search", "analyze", "summarize", "transform"
    };
    
    if (intent < 0 || (size_t)intent >= sizeof(intent_names) / sizeof(intent_names[0])) {
        return NULL;
    }
    
    const char* intent_name = intent_names[intent];
    
    for (size_t i = 0; i < g_cognition.intent_skill_count; i++) {
        if (strcmp(intent_name, g_cognition.intent_skills[i].intent_name) == 0) {
            return g_cognition.intent_skills[i].skill_name;
        }
    }
    
    return NULL;
}

/**
 * @brief 生成执行计划JSON
 * @param intent 任务意图
 * @param skill_name 技能名称
 * @param params 任务参数
 * @param param_count 参数数量
 * @return 执行计划JSON字符串，需要调用free释放
 */
static char* generate_execution_plan(clt_task_intent_t intent,
                                     const char* skill_name,
                                     clt_task_param_t* params,
                                     size_t param_count) {
    /* 计算所需缓冲区大小 */
    size_t buffer_size = 512;  /* 基础大小 */
    
    if (skill_name) {
        buffer_size += strlen(skill_name) + 32;
    }
    
    for (size_t i = 0; i < param_count; i++) {
        if (params[i].key && params[i].value) {
            buffer_size += strlen(params[i].key) + strlen(params[i].value) + 32;
        }
    }
    
    /* 分配缓冲区 */
    char* plan = (char*)malloc(buffer_size);
    if (!plan) {
        set_last_error("Failed to allocate memory for execution plan");
        return NULL;
    }
    
    /* 生成JSON */
    char* ptr = plan;
    size_t remaining = buffer_size;
    
    int written = snprintf(ptr, remaining,
        "{\"intent\":\"%s\",\"skill\":\"%s\",\"params\":{",
        clt_cognition_get_intent_description(intent),
        skill_name ? skill_name : "unknown_skill");
    
    if (written < 0 || (size_t)written >= remaining) {
        free(plan);
        return NULL;
    }
    
    ptr += written;
    remaining -= (size_t)written;
    
    /* 添加参数 */
    for (size_t i = 0; i < param_count; i++) {
        if (params[i].key && params[i].value) {
            if (i > 0) {
                written = snprintf(ptr, remaining, ",");
                if (written < 0 || (size_t)written >= remaining) {
                    free(plan);
                    return NULL;
                }
                ptr += written;
                remaining -= (size_t)written;
            }
            
            written = snprintf(ptr, remaining, "\"%s\":\"%s\"",
                             params[i].key, params[i].value);
            if (written < 0 || (size_t)written >= remaining) {
                free(plan);
                return NULL;
            }
            ptr += written;
            remaining -= (size_t)written;
        }
    }
    
    /* 闭合JSON */
    written = snprintf(ptr, remaining, "},\"timestamp\":%llu}",
                      (unsigned long long)time(NULL));
    
    if (written < 0 || (size_t)written >= remaining) {
        free(plan);
        return NULL;
    }
    
    return plan;
}

/**
 * @brief 检查缓存中是否有相同任务的结果
 * @param task_data 任务数据
 * @param task_data_len 任务数据长度
 * @param cached_result 输出参数：缓存结果
 * @param cached_result_len 输出参数：缓存结果长度
 * @return 找到缓存返回true，否则返回false
 */
static bool check_cache(const char* task_data, size_t task_data_len,
                       char** cached_result, size_t* cached_result_len) {
    /* 简化缓存：使用任务数据的前64字节作为哈希 */
    char hash[65] = {0};
    size_t hash_len = task_data_len < 64 ? task_data_len : 64;
    strncpy(hash, task_data, hash_len);
    hash[hash_len] = '\0';
    
    for (size_t i = 0; i < DEFAULT_CACHE_SIZE; i++) {
        if (g_cognition.cache[i].task_data_hash &&
            strcmp(g_cognition.cache[i].task_data_hash, hash) == 0) {
            if (g_cognition.cache[i].cached_result) {
                *cached_result = strdup(g_cognition.cache[i].cached_result);
                *cached_result_len = g_cognition.cache[i].cached_result_len;
                return true;
            }
        }
    }
    
    return false;
}

/**
 * @brief 添加结果到缓存
 * @param task_data 任务数据
 * @param task_data_len 任务数据长度
 * @param result 处理结果
 * @param result_len 结果长度
 */
static void add_to_cache(const char* task_data, size_t task_data_len,
                        const char* result, size_t result_len) {
    /* 简化缓存：使用任务数据的前64字节作为哈希 */
    char hash[65] = {0};
    size_t hash_len = task_data_len < 64 ? task_data_len : 64;
    strncpy(hash, task_data, hash_len);
    hash[hash_len] = '\0';
    
    /* 使用LRU策略：覆盖最旧的项目 */
    size_t index = g_cognition.cache_head;
    g_cognition.cache_head = (g_cognition.cache_head + 1) % DEFAULT_CACHE_SIZE;
    
    /* 清理旧项目 */
    if (g_cognition.cache[index].task_data_hash) {
        free(g_cognition.cache[index].task_data_hash);
    }
    if (g_cognition.cache[index].cached_result) {
        free(g_cognition.cache[index].cached_result);
    }
    
    /* 添加新项目 */
    g_cognition.cache[index].task_data_hash = strdup(hash);
    g_cognition.cache[index].cached_result = strdup(result);
    g_cognition.cache[index].cached_result_len = result_len;
}

/* ==================== 公共接口实现 ==================== */

bool clt_cognition_init(void) {
    if (g_cognition.initialized) {
        return true;
    }
    
    /* 初始化全局状态 */
    memset(&g_cognition, 0, sizeof(g_cognition));
    
    /* 初始化意图关键词映射 */
    init_intent_keywords();
    
    /* 初始化缓存 */
    for (size_t i = 0; i < DEFAULT_CACHE_SIZE; i++) {
        g_cognition.cache[i].task_data_hash = NULL;
        g_cognition.cache[i].cached_result = NULL;
        g_cognition.cache[i].cached_result_len = 0;
    }
    g_cognition.cache_head = 0;
    
    g_cognition.initialized = true;
    return true;
}

void clt_cognition_cleanup(void) {
    if (!g_cognition.initialized) {
        return;
    }
    
    /* 清理缓存 */
    for (size_t i = 0; i < DEFAULT_CACHE_SIZE; i++) {
        if (g_cognition.cache[i].task_data_hash) {
            free(g_cognition.cache[i].task_data_hash);
        }
        if (g_cognition.cache[i].cached_result) {
            free(g_cognition.cache[i].cached_result);
        }
    }
    
    /* 重置全局状态 */
    memset(&g_cognition, 0, sizeof(g_cognition));
}

char* clt_cognition_process(const char* task_data, size_t task_data_len) {
    if (!g_cognition.initialized) {
        set_last_error("Cognition layer not initialized");
        return NULL;
    }
    
    if (!task_data || task_data_len == 0) {
        set_last_error("Invalid task data");
        return NULL;
    }
    
    /* 检查缓存 */
    char* cached_result = NULL;
    size_t cached_result_len = 0;
    if (check_cache(task_data, task_data_len, &cached_result, &cached_result_len)) {
        return cached_result;
    }
    
    /* 检测意图 */
    clt_task_intent_t intent = detect_intent(task_data);
    
    /* 获取技能名称 */
    const char* skill_name = get_skill_for_intent(intent);
    if (!skill_name) {
        skill_name = "default_skill";
    }
    
    /* 提取参数 */
    clt_task_param_t params[10];
    size_t param_count = extract_params(task_data, params, 10);
    
    /* 生成执行计划 */
    char* plan = generate_execution_plan(intent, skill_name, params, param_count);
    
    /* 清理参数内存 */
    for (size_t i = 0; i < param_count; i++) {
        if (params[i].key) free(params[i].key);
        if (params[i].value) free(params[i].value);
    }
    
    if (!plan) {
        set_last_error("Failed to generate execution plan");
        return NULL;
    }
    
    /* 添加到缓存 */
    add_to_cache(task_data, task_data_len, plan, strlen(plan));
    
    return plan;
}

clt_cognition_result_t* clt_cognition_parse_result(const char* result_json) {
    if (!result_json) {
        set_last_error("Invalid result JSON");
        return NULL;
    }
    
    clt_cognition_result_t* result = 
        (clt_cognition_result_t*)calloc(1, sizeof(clt_cognition_result_t));
    if (!result) {
        set_last_error("Failed to allocate memory for result structure");
        return NULL;
    }
    
    /* 简化解析：只提取关键字段 */
    char intent_str[64];
    if (parse_simple_json_string(result_json, "intent", intent_str, sizeof(intent_str))) {
        /* 将意图字符串转换为枚举值 */
        for (size_t i = 0; i < g_cognition.intent_keyword_count; i++) {
            if (strcasecmp(intent_str, g_cognition.intent_keywords[i].keyword) == 0) {
                result->intent = g_cognition.intent_keywords[i].intent;
                break;
            }
        }
    }
    
    /* 提取技能名称 */
    char skill_name[64];
    if (parse_simple_json_string(result_json, "skill", skill_name, sizeof(skill_name))) {
        result->skill_count = 1;
        result->required_skills = (char**)malloc(sizeof(char*));
        if (result->required_skills) {
            result->required_skills[0] = strdup(skill_name);
        }
    }
    
    /* 保存原始计划 */
    result->raw_plan = strdup(result_json);
    
    return result;
}

const char* clt_cognition_get_intent_description(clt_task_intent_t intent) {
    switch (intent) {
        case CLT_INTENT_UNKNOWN:     return "unknown";
        case CLT_INTENT_QUERY:       return "query";
        case CLT_INTENT_UPDATE:      return "update";
        case CLT_INTENT_DELETE:      return "delete";
        case CLT_INTENT_CREATE:      return "create";
        case CLT_INTENT_SEARCH:      return "search";
        case CLT_INTENT_ANALYZE:     return "analyze";
        case CLT_INTENT_SUMMARIZE:   return "summarize";
        case CLT_INTENT_TRANSFORM:   return "transform";
        default:                     return "unknown";
    }
}

float clt_cognition_evaluate_complexity(const clt_cognition_result_t* result) {
    if (!result) {
        return 0.0f;
    }
    
    /* 简化复杂度评估 */
    float complexity = 0.0f;
    
    /* 基于意图的复杂度 */
    switch (result->intent) {
        case CLT_INTENT_QUERY:
        case CLT_INTENT_SEARCH:
            complexity = 0.3f;
            break;
        case CLT_INTENT_UPDATE:
        case CLT_INTENT_DELETE:
            complexity = 0.5f;
            break;
        case CLT_INTENT_CREATE:
            complexity = 0.6f;
            break;
        case CLT_INTENT_ANALYZE:
        case CLT_INTENT_SUMMARIZE:
        case CLT_INTENT_TRANSFORM:
            complexity = 0.8f;
            break;
        default:
            complexity = 0.2f;
            break;
    }
    
    /* 基于技能数量的复杂度 */
    if (result->skill_count > 0) {
        complexity += result->skill_count * 0.1f;
    }
    
    /* 基于参数数量的复杂度 */
    if (result->param_count > 0) {
        complexity += result->param_count * 0.05f;
    }
    
    /* 限制在0.0-1.0范围内 */
    if (complexity < 0.0f) complexity = 0.0f;
    if (complexity > 1.0f) complexity = 1.0f;
    
    return complexity;
}

void clt_cognition_free_result(char* result) {
    if (result) {
        free(result);
    }
}

void clt_cognition_free_parsed_result(clt_cognition_result_t* result) {
    if (!result) {
        return;
    }
    
    if (result->required_skills) {
        for (size_t i = 0; i < result->skill_count; i++) {
            if (result->required_skills[i]) {
                free(result->required_skills[i]);
            }
        }
        free(result->required_skills);
    }
    
    if (result->params) {
        for (size_t i = 0; i < result->param_count; i++) {
            if (result->params[i].key) free(result->params[i].key);
            if (result->params[i].value) free(result->params[i].value);
        }
        free(result->params);
    }
    
    if (result->raw_plan) {
        free(result->raw_plan);
    }
    
    free(result);
}

const char* clt_cognition_get_last_error(void) {
    return g_cognition.last_error;
}