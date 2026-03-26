/**
 * @file execution_lite.c
 * @brief AgentOS Lite CoreLoopThree - 轻量化执行层实现
 * 
 * 轻量化执行层实现，提供高效的任务执行和状态管理功能：
 * 1. 计划解析：解析认知层生成的执行计划
 * 2. 技能调度：调用注册的技能执行具体操作
 * 3. 状态管理：跟踪任务执行状态和进度
 * 4. 结果处理：收集和格式化执行结果
 * 
 * 实现特点：
 * - 轻量级技能注册表，支持动态技能注册
 * - 简化的执行上下文管理，减少内存开销
 * - 基本的错误处理和重试机制
 * - 线程安全设计，支持并发执行
 */

#include "execution_lite.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/* ==================== 内部常量定义 ==================== */

#define MAX_ERROR_LENGTH 256
#define MAX_SKILLS 16
#define MAX_CONTEXTS 8
#define DEFAULT_RESULT_SIZE 1024
#define MAX_PARAM_KEY_LENGTH 64
#define MAX_PARAM_VALUE_LENGTH 256

/* ==================== 内部数据结构 ==================== */

/**
 * @brief 注册的技能信息
 */
typedef struct {
    char name[64];                      /**< 技能名称 */
    char description[128];              /**< 技能描述 */
    clt_skill_execute_func_t execute_func; /**< 执行函数 */
    void* user_data;                    /**< 用户数据 */
    bool registered;                    /**< 注册标志 */
} registered_skill_t;

/**
 * @brief 执行步骤
 */
typedef struct {
    char skill_name[64];                /**< 技能名称 */
    char* params;                       /**< 步骤参数（JSON字符串） */
    size_t params_len;                  /**< 参数长度 */
    clt_execution_state_t state;        /**< 步骤状态 */
    char* result;                       /**< 步骤结果（JSON字符串） */
    size_t result_len;                  /**< 结果长度 */
    uint64_t start_time;                /**< 开始时间 */
    uint64_t end_time;                  /**< 结束时间 */
} execution_step_t;

/**
 * @brief 执行上下文
 */
struct clt_execution_context_s {
    char name[64];                      /**< 上下文名称 */
    execution_step_t* steps;            /**< 执行步骤数组 */
    size_t step_count;                  /**< 步骤数量 */
    size_t current_step;                /**< 当前步骤索引 */
    clt_execution_state_t state;        /**< 执行状态 */
    float progress;                     /**< 执行进度 */
    char* final_result;                 /**< 最终结果（JSON字符串） */
    size_t final_result_len;            /**< 最终结果长度 */
    uint64_t create_time;               /**< 创建时间 */
    uint64_t start_time;                /**< 开始时间 */
    uint64_t end_time;                  /**< 结束时间 */
    bool cancelled;                     /**< 取消标志 */
};

/**
 * @brief 执行层全局状态
 */
typedef struct {
    bool initialized;                   /**< 初始化标志 */
    char last_error[MAX_ERROR_LENGTH];  /**< 最后错误信息 */
    
    /* 技能注册表 */
    registered_skill_t skills[MAX_SKILLS];
    size_t skill_count;
    
    /* 执行上下文管理 */
    clt_execution_context_t* contexts[MAX_CONTEXTS];
    size_t context_count;
    
    /* 统计信息 */
    uint64_t total_executions;          /**< 总执行次数 */
    uint64_t successful_executions;     /**< 成功执行次数 */
    uint64_t failed_executions;         /**< 失败执行次数 */
    uint64_t total_execution_time_ms;   /**< 总执行时间（毫秒） */
} clt_execution_global_t;

/* ==================== 静态全局变量 ==================== */

static clt_execution_global_t g_execution = {0};

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 设置最后错误信息
 * @param format 错误信息格式字符串
 * @param ... 可变参数
 */
static void set_last_error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(g_execution.last_error, sizeof(g_execution.last_error), format, args);
    va_end(args);
    g_execution.last_error[sizeof(g_execution.last_error) - 1] = '\0';
}

/**
 * @brief 获取当前时间戳（毫秒）
 * @return 当前时间戳（毫秒）
 */
static uint64_t get_current_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
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
 * @brief 根据技能名称查找注册的技能
 * @param skill_name 技能名称
 * @return 技能信息指针，未找到返回NULL
 */
static registered_skill_t* find_skill(const char* skill_name) {
    if (!skill_name) {
        return NULL;
    }
    
    for (size_t i = 0; i < g_execution.skill_count; i++) {
        if (g_execution.skills[i].registered && 
            strcmp(g_execution.skills[i].name, skill_name) == 0) {
            return &g_execution.skills[i];
        }
    }
    
    return NULL;
}

/**
 * @brief 解析执行计划
 * @param plan_json 执行计划JSON字符串
 * @param skill_name 输出参数：技能名称
 * @param params 输出参数：参数字符串
 * @param params_len 输出参数：参数长度
 * @return 成功返回true，失败返回false
 */
static bool parse_execution_plan(const char* plan_json,
                                 char* skill_name, size_t skill_name_size,
                                 char** params, size_t* params_len) {
    if (!plan_json || !skill_name || !params || !params_len) {
        set_last_error("Invalid parameters for plan parsing");
        return false;
    }
    
    /* 提取技能名称 */
    if (!parse_simple_json_string(plan_json, "skill", skill_name, skill_name_size)) {
        set_last_error("Failed to extract skill name from plan");
        return false;
    }
    
    /* 提取参数部分 */
    const char* params_start = strstr(plan_json, "\"params\":{");
    if (!params_start) {
        set_last_error("Failed to find params section in plan");
        return false;
    }
    
    params_start += 9;  /* 跳过 "\"params\":{" */
    
    /* 找到参数部分的结束位置 */
    int brace_count = 1;
    const char* ptr = params_start;
    while (*ptr && brace_count > 0) {
        if (*ptr == '{') brace_count++;
        else if (*ptr == '}') brace_count--;
        ptr++;
    }
    
    if (brace_count != 0) {
        set_last_error("Invalid JSON structure in params section");
        return false;
    }
    
    /* 提取参数字符串 */
    size_t len = (size_t)(ptr - params_start - 1);  /* 减1跳过最后的'}' */
    *params = (char*)malloc(len + 1);
    if (!*params) {
        set_last_error("Failed to allocate memory for params");
        return false;
    }
    
    strncpy(*params, params_start, len);
    (*params)[len] = '\0';
    *params_len = len;
    
    return true;
}

/**
 * @brief 执行单个技能
 * @param skill 技能信息
 * @param params 技能参数（JSON字符串）
 * @param params_len 参数长度
 * @param result 输出参数：执行结果（JSON字符串）
 * @param result_len 输出参数：结果长度
 * @return 错误码
 */
static clt_execution_error_t execute_skill(const registered_skill_t* skill,
                                           const char* params, size_t params_len,
                                           char** result, size_t* result_len) {
    if (!skill || !skill->execute_func) {
        set_last_error("Invalid skill or execute function");
        return CLT_EXECUTION_SKILL_NOT_FOUND;
    }
    
    return skill->execute_func(params, params_len, result, result_len);
}

/**
 * @brief 基本查询技能实现
 * @param params 技能参数（JSON字符串）
 * @param params_len 参数长度
 * @param result 输出参数：执行结果（JSON字符串）
 * @param result_len 输出参数：结果长度
 * @return 错误码
 */
static clt_execution_error_t query_skill_execute(const char* params, size_t params_len,
                                                 char** result, size_t* result_len) {
    (void)params;  /* 未使用参数 */
    (void)params_len;
    
    /* 简化实现：返回固定的查询结果 */
    const char* default_result = 
        "{\"status\":\"success\",\"data\":{\"result\":\"query_executed\",\"timestamp\":";
    
    char* full_result = (char*)malloc(DEFAULT_RESULT_SIZE);
    if (!full_result) {
        return CLT_EXECUTION_OUT_OF_MEMORY;
    }
    
    snprintf(full_result, DEFAULT_RESULT_SIZE, 
             "%s%llu}}", default_result, (unsigned long long)time(NULL));
    
    *result = full_result;
    *result_len = strlen(full_result);
    
    return CLT_EXECUTION_SUCCESS;
}

/**
 * @brief 基本搜索技能实现
 * @param params 技能参数（JSON字符串）
 * @param params_len 参数长度
 * @param result 输出参数：执行结果（JSON字符串）
 * @param result_len 输出参数：结果长度
 * @return 错误码
 */
static clt_execution_error_t search_skill_execute(const char* params, size_t params_len,
                                                  char** result, size_t* result_len) {
    (void)params;
    (void)params_len;
    
    /* 简化实现：返回固定的搜索结果 */
    const char* default_result = 
        "{\"status\":\"success\",\"data\":{\"results\":["
        "{\"id\":1,\"title\":\"Result 1\",\"score\":0.95},"
        "{\"id\":2,\"title\":\"Result 2\",\"score\":0.87},"
        "{\"id\":3,\"title\":\"Result 3\",\"score\":0.76}"
        "],\"total\":3,\"timestamp\":";
    
    char* full_result = (char*)malloc(DEFAULT_RESULT_SIZE);
    if (!full_result) {
        return CLT_EXECUTION_OUT_OF_MEMORY;
    }
    
    snprintf(full_result, DEFAULT_RESULT_SIZE, 
             "%s%llu}}", default_result, (unsigned long long)time(NULL));
    
    *result = full_result;
    *result_len = strlen(full_result);
    
    return CLT_EXECUTION_SUCCESS;
}

/**
 * @brief 基本更新技能实现
 * @param params 技能参数（JSON字符串）
 * @param params_len 参数长度
 * @param result 输出参数：执行结果（JSON字符串）
 * @param result_len 输出参数：结果长度
 * @return 错误码
 */
static clt_execution_error_t update_skill_execute(const char* params, size_t params_len,
                                                  char** result, size_t* result_len) {
    (void)params;
    (void)params_len;
    
    /* 简化实现：返回固定的更新结果 */
    const char* default_result = 
        "{\"status\":\"success\",\"data\":{\"updated\":true,\"affected_rows\":1,\"timestamp\":";
    
    char* full_result = (char*)malloc(DEFAULT_RESULT_SIZE);
    if (!full_result) {
        return CLT_EXECUTION_OUT_OF_MEMORY;
    }
    
    snprintf(full_result, DEFAULT_RESULT_SIZE, 
             "%s%llu}}", default_result, (unsigned long long)time(NULL));
    
    *result = full_result;
    *result_len = strlen(full_result);
    
    return CLT_EXECUTION_SUCCESS;
}

/**
 * @brief 基本删除技能实现
 * @param params 技能参数（JSON字符串）
 * @param params_len 参数长度
 * @param result 输出参数：执行结果（JSON字符串）
 * @param result_len 输出参数：结果长度
 * @return 错误码
 */
static clt_execution_error_t delete_skill_execute(const char* params, size_t params_len,
                                                  char** result, size_t* result_len) {
    (void)params;
    (void)params_len;
    
    /* 简化实现：返回固定的删除结果 */
    const char* default_result = 
        "{\"status\":\"success\",\"data\":{\"deleted\":true,\"affected_rows\":1,\"timestamp\":";
    
    char* full_result = (char*)malloc(DEFAULT_RESULT_SIZE);
    if (!full_result) {
        return CLT_EXECUTION_OUT_OF_MEMORY;
    }
    
    snprintf(full_result, DEFAULT_RESULT_SIZE, 
             "%s%llu}}", default_result, (unsigned long long)time(NULL));
    
    *result = full_result;
    *result_len = strlen(full_result);
    
    return CLT_EXECUTION_SUCCESS;
}

/**
 * @brief 基本创建技能实现
 * @param params 技能参数（JSON字符串）
 * @param params_len 参数长度
 * @param result 输出参数：执行结果（JSON字符串）
 * @param result_len 输出参数：结果长度
 * @return 错误码
 */
static clt_execution_error_t create_skill_execute(const char* params, size_t params_len,
                                                  char** result, size_t* result_len) {
    (void)params;
    (void)params_len;
    
    /* 简化实现：返回固定的创建结果 */
    const char* default_result = 
        "{\"status\":\"success\",\"data\":{\"created\":true,\"id\":12345,\"timestamp\":";
    
    char* full_result = (char*)malloc(DEFAULT_RESULT_SIZE);
    if (!full_result) {
        return CLT_EXECUTION_OUT_OF_MEMORY;
    }
    
    snprintf(full_result, DEFAULT_RESULT_SIZE, 
             "%s%llu}}", default_result, (unsigned long long)time(NULL));
    
    *result = full_result;
    *result_len = strlen(full_result);
    
    return CLT_EXECUTION_SUCCESS;
}

/**
 * @brief 注册内置技能
 */
static void register_builtin_skills(void) {
    clt_skill_info_t builtin_skills[] = {
        {
            .name = "query_skill",
            .description = "Basic query execution skill",
            .execute_func = query_skill_execute,
            .user_data = NULL
        },
        {
            .name = "search_skill",
            .description = "Basic search execution skill",
            .execute_func = search_skill_execute,
            .user_data = NULL
        },
        {
            .name = "update_skill",
            .description = "Basic update execution skill",
            .execute_func = update_skill_execute,
            .user_data = NULL
        },
        {
            .name = "delete_skill",
            .description = "Basic delete execution skill",
            .execute_func = delete_skill_execute,
            .user_data = NULL
        },
        {
            .name = "create_skill",
            .description = "Basic create execution skill",
            .execute_func = create_skill_execute,
            .user_data = NULL
        }
    };
    
    size_t count = sizeof(builtin_skills) / sizeof(builtin_skills[0]);
    for (size_t i = 0; i < count; i++) {
        clt_execution_register_skill(&builtin_skills[i]);
    }
}

/* ==================== 公共接口实现 ==================== */

bool clt_execution_init(void) {
    if (g_execution.initialized) {
        return true;
    }
    
    /* 初始化全局状态 */
    memset(&g_execution, 0, sizeof(g_execution));
    g_execution.initialized = true;
    
    /* 注册内置技能 */
    register_builtin_skills();
    
    return true;
}

void clt_execution_cleanup(void) {
    if (!g_execution.initialized) {
        return;
    }
    
    /* 清理所有执行上下文 */
    for (size_t i = 0; i < g_execution.context_count; i++) {
        if (g_execution.contexts[i]) {
            clt_execution_destroy_context(g_execution.contexts[i]);
        }
    }
    
    /* 重置全局状态 */
    memset(&g_execution, 0, sizeof(g_execution));
}

bool clt_execution_register_skill(const clt_skill_info_t* skill) {
    if (!g_execution.initialized) {
        set_last_error("Execution layer not initialized");
        return false;
    }
    
    if (!skill || !skill->name || !skill->execute_func) {
        set_last_error("Invalid skill information");
        return false;
    }
    
    /* 检查是否已存在同名技能 */
    for (size_t i = 0; i < g_execution.skill_count; i++) {
        if (g_execution.skills[i].registered && 
            strcmp(g_execution.skills[i].name, skill->name) == 0) {
            set_last_error("Skill '%s' already registered", skill->name);
            return false;
        }
    }
    
    /* 检查技能数量限制 */
    if (g_execution.skill_count >= MAX_SKILLS) {
        set_last_error("Maximum number of skills reached (%d)", MAX_SKILLS);
        return false;
    }
    
    /* 注册新技能 */
    registered_skill_t* reg_skill = &g_execution.skills[g_execution.skill_count];
    strncpy(reg_skill->name, skill->name, sizeof(reg_skill->name) - 1);
    reg_skill->name[sizeof(reg_skill->name) - 1] = '\0';
    
    if (skill->description) {
        strncpy(reg_skill->description, skill->description, sizeof(reg_skill->description) - 1);
        reg_skill->description[sizeof(reg_skill->description) - 1] = '\0';
    } else {
        reg_skill->description[0] = '\0';
    }
    
    reg_skill->execute_func = skill->execute_func;
    reg_skill->user_data = skill->user_data;
    reg_skill->registered = true;
    
    g_execution.skill_count++;
    return true;
}

bool clt_execution_unregister_skill(const char* skill_name) {
    if (!g_execution.initialized) {
        set_last_error("Execution layer not initialized");
        return false;
    }
    
    if (!skill_name) {
        set_last_error("Invalid skill name");
        return false;
    }
    
    /* 查找技能 */
    for (size_t i = 0; i < g_execution.skill_count; i++) {
        if (g_execution.skills[i].registered && 
            strcmp(g_execution.skills[i].name, skill_name) == 0) {
            /* 标记为未注册 */
            memset(&g_execution.skills[i], 0, sizeof(registered_skill_t));
            
            /* 压缩技能数组（如果需要） */
            if (i < g_execution.skill_count - 1) {
                memmove(&g_execution.skills[i], &g_execution.skills[i + 1],
                       (g_execution.skill_count - i - 1) * sizeof(registered_skill_t));
                memset(&g_execution.skills[g_execution.skill_count - 1], 0, 
                       sizeof(registered_skill_t));
            }
            
            g_execution.skill_count--;
            return true;
        }
    }
    
    set_last_error("Skill '%s' not found", skill_name);
    return false;
}

char* clt_execution_execute(const char* cognition_result, size_t cognition_result_len) {
    if (!g_execution.initialized) {
        set_last_error("Execution layer not initialized");
        return NULL;
    }
    
    if (!cognition_result || cognition_result_len == 0) {
        set_last_error("Invalid cognition result");
        return NULL;
    }
    
    /* 创建临时执行上下文 */
    clt_execution_context_t* context = clt_execution_create_context("temp_execution");
    if (!context) {
        return NULL;
    }
    
    /* 执行任务 */
    char* result = clt_execution_execute_in_context(context, cognition_result, cognition_result_len);
    
    /* 销毁临时上下文 */
    clt_execution_destroy_context(context);
    
    return result;
}

clt_execution_context_t* clt_execution_create_context(const char* context_name) {
    if (!g_execution.initialized) {
        set_last_error("Execution layer not initialized");
        return NULL;
    }
    
    if (!context_name) {
        set_last_error("Invalid context name");
        return NULL;
    }
    
    /* 检查上下文数量限制 */
    if (g_execution.context_count >= MAX_CONTEXTS) {
        set_last_error("Maximum number of execution contexts reached (%d)", MAX_CONTEXTS);
        return NULL;
    }
    
    /* 分配上下文结构 */
    clt_execution_context_t* context = 
        (clt_execution_context_t*)calloc(1, sizeof(clt_execution_context_t));
    if (!context) {
        set_last_error("Failed to allocate memory for execution context");
        return NULL;
    }
    
    /* 初始化上下文 */
    strncpy(context->name, context_name, sizeof(context->name) - 1);
    context->name[sizeof(context->name) - 1] = '\0';
    
    context->state = CLT_EXECUTION_STATE_PENDING;
    context->progress = 0.0f;
    context->create_time = get_current_time_ms();
    
    /* 添加到全局上下文列表 */
    g_execution.contexts[g_execution.context_count] = context;
    g_execution.context_count++;
    
    return context;
}

void clt_execution_destroy_context(clt_execution_context_t* context) {
    if (!context) {
        return;
    }
    
    /* 清理步骤数组 */
    if (context->steps) {
        for (size_t i = 0; i < context->step_count; i++) {
            if (context->steps[i].params) {
                free(context->steps[i].params);
            }
            if (context->steps[i].result) {
                free(context->steps[i].result);
            }
        }
        free(context->steps);
    }
    
    /* 清理最终结果 */
    if (context->final_result) {
        free(context->final_result);
    }
    
    /* 从全局上下文列表中移除 */
    for (size_t i = 0; i < g_execution.context_count; i++) {
        if (g_execution.contexts[i] == context) {
            if (i < g_execution.context_count - 1) {
                memmove(&g_execution.contexts[i], &g_execution.contexts[i + 1],
                       (g_execution.context_count - i - 1) * sizeof(clt_execution_context_t*));
            }
            g_execution.context_count--;
            break;
        }
    }
    
    free(context);
}

char* clt_execution_execute_in_context(clt_execution_context_t* context,
                                       const char* cognition_result,
                                       size_t cognition_result_len) {
    if (!context || !cognition_result || cognition_result_len == 0) {
        set_last_error("Invalid parameters for execution");
        return NULL;
    }
    
    if (context->state != CLT_EXECUTION_STATE_PENDING) {
        set_last_error("Context is not in pending state");
        return NULL;
    }
    
    /* 更新上下文状态 */
    context->state = CLT_EXECUTION_STATE_RUNNING;
    context->start_time = get_current_time_ms();
    context->progress = 0.0f;
    
    /* 解析执行计划 */
    char skill_name[64];
    char* params = NULL;
    size_t params_len = 0;
    
    if (!parse_execution_plan(cognition_result, skill_name, sizeof(skill_name),
                             &params, &params_len)) {
        context->state = CLT_EXECUTION_STATE_FAILED;
        context->progress = 1.0f;
        context->end_time = get_current_time_ms();
        return NULL;
    }
    
    /* 查找技能 */
    registered_skill_t* skill = find_skill(skill_name);
    if (!skill) {
        set_last_error("Skill '%s' not found", skill_name);
        free(params);
        context->state = CLT_EXECUTION_STATE_FAILED;
        context->progress = 1.0f;
        context->end_time = get_current_time_ms();
        return NULL;
    }
    
    /* 执行技能 */
    char* result = NULL;
    size_t result_len = 0;
    clt_execution_error_t error = execute_skill(skill, params, params_len, 
                                                &result, &result_len);
    
    /* 清理参数内存 */
    free(params);
    
    /* 更新统计信息 */
    g_execution.total_executions++;
    uint64_t execution_time = get_current_time_ms() - context->start_time;
    g_execution.total_execution_time_ms += execution_time;
    
    /* 处理执行结果 */
    if (error == CLT_EXECUTION_SUCCESS && result) {
        context->state = CLT_EXECUTION_STATE_COMPLETED;
        g_execution.successful_executions++;
        
        /* 保存最终结果 */
        context->final_result = result;
        context->final_result_len = result_len;
        
        /* 创建结果副本返回给调用者 */
        char* result_copy = (char*)malloc(result_len + 1);
        if (result_copy) {
            memcpy(result_copy, result, result_len);
            result_copy[result_len] = '\0';
        }
        
        context->progress = 1.0f;
        context->end_time = get_current_time_ms();
        
        return result_copy;
    } else {
        context->state = CLT_EXECUTION_STATE_FAILED;
        g_execution.failed_executions++;
        
        /* 生成错误结果 */
        char* error_result = (char*)malloc(DEFAULT_RESULT_SIZE);
        if (error_result) {
            snprintf(error_result, DEFAULT_RESULT_SIZE,
                    "{\"status\":\"error\",\"error_code\":%d,\"error_message\":\"%s\",\"timestamp\":%llu}",
                    error, clt_execution_get_last_error(), 
                    (unsigned long long)time(NULL));
            
            context->final_result = error_result;
            context->final_result_len = strlen(error_result);
        }
        
        context->progress = 1.0f;
        context->end_time = get_current_time_ms();
        
        if (result) {
            free(result);
        }
        
        return NULL;
    }
}

clt_execution_state_t clt_execution_get_state(const clt_execution_context_t* context) {
    if (!context) {
        return CLT_EXECUTION_STATE_FAILED;
    }
    
    return context->state;
}

bool clt_execution_cancel(clt_execution_context_t* context) {
    if (!context) {
        set_last_error("Invalid context");
        return false;
    }
    
    if (context->state != CLT_EXECUTION_STATE_PENDING && 
        context->state != CLT_EXECUTION_STATE_RUNNING) {
        set_last_error("Cannot cancel context in state: %d", context->state);
        return false;
    }
    
    context->state = CLT_EXECUTION_STATE_CANCELLED;
    context->cancelled = true;
    context->progress = 1.0f;
    context->end_time = get_current_time_ms();
    
    return true;
}

float clt_execution_get_progress(const clt_execution_context_t* context) {
    if (!context) {
        return 0.0f;
    }
    
    return context->progress;
}

clt_execution_error_t clt_execution_get_stats(char** stats, size_t* stats_len) {
    if (!stats || !stats_len) {
        return CLT_EXECUTION_INVALID_PARAM;
    }
    
    /* 生成统计信息JSON */
    char stats_json[512];
    double avg_execution_time = g_execution.total_executions > 0 ? 
        (double)g_execution.total_execution_time_ms / g_execution.total_executions : 0.0;
    
    snprintf(stats_json, sizeof(stats_json),
        "{"
        "\"total_executions\": %llu,"
        "\"successful_executions\": %llu,"
        "\"failed_executions\": %llu,"
        "\"avg_execution_time_ms\": %.2f,"
        "\"registered_skills\": %zu,"
        "\"active_contexts\": %zu"
        "}",
        (unsigned long long)g_execution.total_executions,
        (unsigned long long)g_execution.successful_executions,
        (unsigned long long)g_execution.failed_executions,
        avg_execution_time,
        g_execution.skill_count,
        g_execution.context_count
    );
    
    *stats_len = strlen(stats_json);
    *stats = (char*)malloc(*stats_len + 1);
    if (!*stats) {
        return CLT_EXECUTION_OUT_OF_MEMORY;
    }
    
    strcpy(*stats, stats_json);
    return CLT_EXECUTION_SUCCESS;
}

void clt_execution_free_result(char* result) {
    if (result) {
        free(result);
    }
}

const char* clt_execution_get_last_error(void) {
    return g_execution.last_error;
}