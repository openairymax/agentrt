/**
 * @file prompt_loader.h
 * @brief P3.2: Prompt 加载器 — 从 YAML 模板加载并渲染 Prompt
 *
 * 功能：
 *   - 从 ecosystem/prompts/templates/ 目录加载 YAML 模板
 *   - 支持 {variable_name} 变量替换
 *   - 支持 output_schema JSON Schema 校验
 *   - 支持模板缓存和热重载
 *
 * 模板结构（YAML 格式）：
 *   name: "template_name"
 *   version: "1.0.0"
 *   description: "模板描述"
 *   model_family: "any"
 *   temperature: 0.7
 *   max_tokens: 4096
 *   system: |
 *     System prompt with {variables}
 *   user_template: |
 *     User message with {variables}
 *   output_schema: {...}
 *   metrics: {...}
 *
 * @owner team-A
 * @see 0.1.1详细任务清单.md P3.2
 */

#ifndef AGENTRT_CORELOOPTHREE_PROMPT_LOADER_H
#define AGENTRT_CORELOOPTHREE_PROMPT_LOADER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * 常量定义
 * ================================================================ */

/** 最大变量数量 */
#define AGENTRT_PROMPT_MAX_VARIABLES      64
/** 变量名最大长度 */
#define AGENTRT_PROMPT_VAR_NAME_MAX       64
/** 变量值最大长度 */
#define AGENTRT_PROMPT_VAR_VALUE_MAX      4096
/** 模板缓存最大条目数 */
#define AGENTRT_PROMPT_CACHE_MAX_ENTRIES  128
/** 模板名称最大长度 */
#define AGENTRT_PROMPT_NAME_MAX           128
/** 模板路径最大长度 */
#define AGENTRT_PROMPT_PATH_MAX           512
/** 默认模板目录 */
#define AGENTRT_PROMPT_DEFAULT_TEMPLATE_DIR "ecosystem/prompts/templates/"

/* ================================================================
 * 类型定义
 * ================================================================ */

/**
 * @brief Prompt 模板变量
 */
typedef struct {
    char name[AGENTRT_PROMPT_VAR_NAME_MAX];   /**< 变量名 */
    char value[AGENTRT_PROMPT_VAR_VALUE_MAX];  /**< 变量值 */
} agentrt_prompt_variable_t;

/**
 * @brief Prompt 模板（从 YAML 加载）
 */
typedef struct {
    char name[AGENTRT_PROMPT_NAME_MAX];       /**< 模板名称 */
    char version[32];                          /**< 模板版本 */
    char description[256];                     /**< 模板描述 */
    char model_family[32];                     /**< 目标模型族 */
    float temperature;                         /**< 推荐温度 */
    uint32_t max_tokens;                       /**< 最大 token 数 */

    char *system_prompt;                       /**< 系统提示词（已分配） */
    size_t system_prompt_len;                  /**< 系统提示词长度 */

    char *user_template;                       /**< 用户模板（已分配） */
    size_t user_template_len;                  /**< 用户模板长度 */

    char *output_schema;                       /**< JSON Schema 字符串（已分配） */
    size_t output_schema_len;                  /**< JSON Schema 长度 */

    /* 指标 */
    float target_precision;
    float target_recall;
    float max_hallucination_rate;

    /* 内部标记 */
    char source_path[AGENTRT_PROMPT_PATH_MAX]; /**< 模板文件路径 */
    bool is_loaded;                            /**< 是否已加载 */
} agentrt_prompt_template_t;

/**
 * @brief 渲染后的 Prompt
 */
typedef struct {
    char *system_prompt;         /**< 渲染后的系统提示词（需调用者释放） */
    size_t system_prompt_len;

    char *user_message;          /**< 渲染后的用户消息（需调用者释放） */
    size_t user_message_len;

    float temperature;           /**< 使用的温度 */
    uint32_t max_tokens;         /**< 最大 token 数 */
    char model_family[32];       /**< 模型族 */
} agentrt_prompt_rendered_t;

/**
 * @brief Prompt 加载器配置
 */
typedef struct {
    char template_dir[AGENTRT_PROMPT_PATH_MAX]; /**< 模板目录 */
    bool enable_cache;                           /**< 是否启用缓存 */
    size_t cache_max_entries;                    /**< 缓存最大条目数 */
    bool enable_schema_validation;               /**< 是否启用 Schema 校验 */
} agentrt_prompt_loader_config_t;

/* ================================================================
 * 生命周期 API
 * ================================================================ */

/**
 * @brief 初始化 Prompt 加载器
 *
 * 扫描模板目录，预加载常用模板。
 *
 * @param config 加载器配置（NULL 使用默认值）
 * @return 0 成功，非0失败
 */
int agentrt_prompt_loader_init(const agentrt_prompt_loader_config_t *config);

/**
 * @brief 关闭 Prompt 加载器，释放所有资源
 */
void agentrt_prompt_loader_shutdown(void);

/* ================================================================
 * 模板加载 API
 * ================================================================ */

/**
 * @brief 从文件加载单个 Prompt 模板
 *
 * @param template_path YAML 模板文件路径
 * @param out_template 输出模板（需调用者通过 agentrt_prompt_template_free 释放）
 * @return 0 成功，非0失败
 *
 * @ownership out_template: OWNER（调用者负责释放）
 */
int agentrt_prompt_template_load(const char *template_path,
                                  agentrt_prompt_template_t **out_template);

/**
 * @brief 按名称加载模板（从缓存或扫描目录）
 *
 * @param template_name 模板名称（不含路径和扩展名）
 * @param category 模板类别（如 "system", "cognition", "memory"），NULL 搜索所有类别
 * @param out_template 输出模板（BORROW，由加载器管理，不可释放）
 * @return 0 成功，非0失败
 */
int agentrt_prompt_template_get(const char *template_name,
                                 const char *category,
                                 agentrt_prompt_template_t **out_template);

/**
 * @brief 释放模板
 *
 * @param template 模板（NULL 无操作）
 */
void agentrt_prompt_template_free(agentrt_prompt_template_t *template);

/* ================================================================
 * 变量渲染 API
 * ================================================================ */

/**
 * @brief 渲染 Prompt 模板
 *
 * 将模板中的 {variable_name} 替换为实际值。
 *
 * @param template 模板（BORROW）
 * @param variables 变量数组
 * @param variable_count 变量数量
 * @param out_rendered 输出渲染结果（需调用者通过 agentrt_prompt_rendered_free 释放）
 * @return 0 成功，非0失败
 *
 * @ownership out_rendered: OWNER（调用者负责释放）
 */
int agentrt_prompt_render(const agentrt_prompt_template_t *template,
                           const agentrt_prompt_variable_t *variables,
                           size_t variable_count,
                           agentrt_prompt_rendered_t **out_rendered);

/**
 * @brief 释放渲染结果
 *
 * @param rendered 渲染结果（NULL 无操作）
 */
void agentrt_prompt_rendered_free(agentrt_prompt_rendered_t *rendered);

/* ================================================================
 * Schema 校验 API
 * ================================================================ */

/**
 * @brief 根据 output_schema 校验 LLM 输出
 *
 * 使用 JSON Schema 校验输出是否符合模板定义的格式。
 * 支持基本校验：type、required、properties、enum、items。
 *
 * @param template 模板（BORROW，包含 output_schema）
 * @param output_json 待校验的 JSON 输出
 * @param output_len 输出长度
 * @param out_error 校验失败时的错误描述（需调用者释放），成功时为 NULL
 * @return 0 校验通过，-1 校验失败，-2 无 schema 定义
 *
 * @ownership out_error: OWNER（调用者负责释放），仅当返回 -1 时分配
 */
int agentrt_prompt_validate_output(const agentrt_prompt_template_t *template,
                                    const char *output_json,
                                    size_t output_len,
                                    char **out_error);

/* ================================================================
 * 缓存管理 API
 * ================================================================ */

/**
 * @brief 清除模板缓存
 */
void agentrt_prompt_cache_clear(void);

/**
 * @brief 重载所有模板（扫描目录重新加载）
 *
 * @return 0 成功，非0失败
 */
int agentrt_prompt_reload_all(void);

/**
 * @brief 获取缓存统计
 *
 * @param out_entry_count 输出缓存条目数
 * @param out_hit_count 输出命中次数
 * @param out_miss_count 输出未命中次数
 */
void agentrt_prompt_cache_stats(size_t *out_entry_count,
                                 size_t *out_hit_count,
                                 size_t *out_miss_count);

/**
 * @brief 设置默认模板目录
 *
 * @param template_dir 模板目录路径
 */
void agentrt_prompt_set_template_dir(const char *template_dir);

#ifdef __cplusplus
}
#endif

#endif /* AGENTRT_CORELOOPTHREE_PROMPT_LOADER_H */