/**
 * @file prompt_loader.c
 * @brief P3.2: Prompt 加载器实现 — YAML 模板加载、变量渲染、Schema 校验
 *
 * 功能：
 *   - P3.2.1: 从 ecosystem/prompts/templates/ 加载 YAML 模板
 *   - P3.2.2: {variable_name} → 实际值 变量替换
 *   - P3.2.3: output_schema JSON Schema 校验
 *
 * @owner team-A
 */

#include "prompt_loader.h"
#include "yaml_loader.h"
#include "memory_compat.h"
#include "string_compat.h"
#include "logging_compat.h"

#include "yaml_minimal.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* ================================================================
 * 内部常量
 * ================================================================ */

#define PROMPT_LOADER_MAX_SCAN_DEPTH     3
#define PROMPT_LOADER_MAX_FILE_SIZE       (1024 * 1024)  /* 1MB */

/* ================================================================
 * 全局状态
 * ================================================================ */

static struct {
    /* 缓存 */
    agentos_prompt_template_t *cache[AGENTOS_PROMPT_CACHE_MAX_ENTRIES];
    size_t cache_count;

    /* 配置 */
    agentos_prompt_loader_config_t config;

    /* 统计 */
    size_t hit_count;
    size_t miss_count;

    /* 线程安全 */
    agentos_mutex_t lock;
    bool initialized;
} g_prompt_loader;

/* ================================================================
 * 辅助函数声明
 * ================================================================ */

static void template_init_defaults(agentos_prompt_template_t *tmpl);
static void template_free_internal(agentos_prompt_template_t *tmpl);
static int parse_template_yaml(const char *yaml_content,
                                agentos_prompt_template_t *tmpl);
static int parse_metrics_yaml(struct yaml_node *metrics_node,
                               agentos_prompt_template_t *tmpl);
static int scan_template_dir(const char *dir_path, int depth);
static char *substitute_variables(const char *template_str,
                                   size_t template_len,
                                   const agentos_prompt_variable_t *variables,
                                   size_t variable_count,
                                   size_t *out_len);
static int validate_required_fields(const agentos_prompt_template_t *tmpl);
static int cache_lookup(const char *name, agentos_prompt_template_t **out);
static void cache_insert(agentos_prompt_template_t *tmpl);

/* ================================================================
 * 默认配置
 * ================================================================ */

static void config_defaults(agentos_prompt_loader_config_t *cfg)
{
    AGENTOS_MEMSET(cfg, 0, sizeof(*cfg));
    AGENTOS_STRNCPY_TERM(cfg->template_dir,
                         AGENTOS_PROMPT_DEFAULT_TEMPLATE_DIR,
                         sizeof(cfg->template_dir));
    cfg->enable_cache = true;
    cfg->cache_max_entries = AGENTOS_PROMPT_CACHE_MAX_ENTRIES;
    cfg->enable_schema_validation = true;
}

static void template_init_defaults(agentos_prompt_template_t *tmpl)
{
    AGENTOS_MEMSET(tmpl, 0, sizeof(*tmpl));
    tmpl->temperature = 0.7f;
    tmpl->max_tokens = 4096;
    AGENTOS_STRNCPY_TERM(tmpl->model_family, "any", sizeof(tmpl->model_family));
    AGENTOS_STRNCPY_TERM(tmpl->version, "1.0.0", sizeof(tmpl->version));
}

/* ================================================================
 * 生命周期
 * ================================================================ */

int agentos_prompt_loader_init(const agentos_prompt_loader_config_t *config)
{
    if (g_prompt_loader.initialized) {
        AGENTOS_LOG_INFO("PromptLoader: already initialized, skipping");
        return 0;
    }

    AGENTOS_MEMSET(&g_prompt_loader, 0, sizeof(g_prompt_loader));

    if (config) {
        AGENTOS_MEMCPY(&g_prompt_loader.config, config, sizeof(*config));
    } else {
        config_defaults(&g_prompt_loader.config);
    }

    AGENTOS_LOG_INFO("PromptLoader: initializing (template_dir=%s, cache=%s, "
                     "schema_validation=%s, max_cache=%zu)",
                     g_prompt_loader.config.template_dir,
                     g_prompt_loader.config.enable_cache ? "on" : "off",
                     g_prompt_loader.config.enable_schema_validation ? "on" : "off",
                     g_prompt_loader.config.cache_max_entries);

    if (agentos_mutex_init(&g_prompt_loader.lock) != 0) {
        AGENTOS_LOG_ERROR("PromptLoader: failed to initialize mutex");
        return -1;
    }

    g_prompt_loader.initialized = true;

    /* 扫描模板目录 */
    int scanned = scan_template_dir(g_prompt_loader.config.template_dir, 0);
    AGENTOS_LOG_INFO("PromptLoader: init complete, scanned %d templates into cache",
                     scanned);

    return 0;
}

void agentos_prompt_loader_shutdown(void)
{
    if (!g_prompt_loader.initialized) {
        AGENTOS_LOG_DEBUG("PromptLoader: not initialized, skip shutdown");
        return;
    }

    AGENTOS_LOG_INFO("PromptLoader: shutting down (cache_entries=%zu, "
                     "hits=%zu, misses=%zu)",
                     g_prompt_loader.cache_count,
                     g_prompt_loader.hit_count,
                     g_prompt_loader.miss_count);

    agentos_mutex_lock(&g_prompt_loader.lock);

    for (size_t i = 0; i < g_prompt_loader.cache_count; i++) {
        template_free_internal(g_prompt_loader.cache[i]);
        AGENTOS_FREE(g_prompt_loader.cache[i]);
        g_prompt_loader.cache[i] = NULL;
    }
    g_prompt_loader.cache_count = 0;

    agentos_mutex_unlock(&g_prompt_loader.lock);
    agentos_mutex_destroy(&g_prompt_loader.lock);

    AGENTOS_MEMSET(&g_prompt_loader, 0, sizeof(g_prompt_loader));
    AGENTOS_LOG_INFO("PromptLoader: shutdown complete");
}

/* ================================================================
 * 模板加载
 * ================================================================ */

int agentos_prompt_template_load(const char *template_path,
                                  agentos_prompt_template_t **out_template)
{
    if (!template_path || !out_template) {
        AGENTOS_LOG_ERROR("PromptLoader: template_load called with NULL params");
        return -1;
    }

    AGENTOS_LOG_DEBUG("PromptLoader: loading template from %s", template_path);

    /* 读取文件 */
    FILE *fp = fopen(template_path, "rb");
    if (!fp) {
        AGENTOS_LOG_WARN("PromptLoader: cannot open template file %s", template_path);
        return -1;
    }

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (size <= 0 || size > PROMPT_LOADER_MAX_FILE_SIZE) {
        AGENTOS_LOG_WARN("PromptLoader: template file %s size=%ld out of range "
                         "(max=%d)",
                         template_path, size, PROMPT_LOADER_MAX_FILE_SIZE);
        fclose(fp);
        return -1;
    }

    AGENTOS_LOG_DEBUG("PromptLoader: reading template file %s (%ld bytes)",
                      template_path, size);

    char *content = (char *)AGENTOS_MALLOC((size_t)size + 1);
    if (!content) {
        AGENTOS_LOG_ERROR("PromptLoader: OOM allocating %ld bytes for %s",
                          size, template_path);
        fclose(fp);
        return -1;
    }

    size_t read = fread(content, 1, (size_t)size, fp);
    fclose(fp);

    if (read != (size_t)size) {
        AGENTOS_LOG_ERROR("PromptLoader: partial read %zu/%ld for %s",
                          read, size, template_path);
        AGENTOS_FREE(content);
        return -1;
    }
    content[read] = '\0';

    /* 分配并解析模板 */
    agentos_prompt_template_t *tmpl =
        (agentos_prompt_template_t *)AGENTOS_CALLOC(1, sizeof(*tmpl));
    if (!tmpl) {
        AGENTOS_LOG_ERROR("PromptLoader: OOM allocating template struct");
        AGENTOS_FREE(content);
        return -1;
    }

    template_init_defaults(tmpl);
    AGENTOS_STRNCPY_TERM(tmpl->source_path, template_path,
                         sizeof(tmpl->source_path));

    int ret = parse_template_yaml(content, tmpl);
    AGENTOS_FREE(content);

    if (ret != 0) {
        AGENTOS_LOG_WARN("PromptLoader: YAML parse failed for %s", template_path);
        template_free_internal(tmpl);
        AGENTOS_FREE(tmpl);
        return -1;
    }

    /* 验证必填字段 */
    if (validate_required_fields(tmpl) != 0) {
        AGENTOS_LOG_WARN("PromptLoader: required fields missing in %s "
                         "(name='%s')",
                         template_path, tmpl->name);
        template_free_internal(tmpl);
        AGENTOS_FREE(tmpl);
        return -1;
    }

    tmpl->is_loaded = true;
    *out_template = tmpl;

    AGENTOS_LOG_INFO("PromptLoader: loaded template '%s' v%s (system=%zub, "
                     "user=%zub, schema=%zub, model=%s, temp=%.2f, max_tokens=%u)",
                     tmpl->name, tmpl->version,
                     tmpl->system_prompt_len, tmpl->user_template_len,
                     tmpl->output_schema_len,
                     tmpl->model_family, tmpl->temperature, tmpl->max_tokens);

    return 0;
}

int agentos_prompt_template_get(const char *template_name,
                                 const char *category,
                                 agentos_prompt_template_t **out_template)
{
    if (!template_name || !out_template) {
        AGENTOS_LOG_ERROR("PromptLoader: template_get called with NULL params");
        return -1;
    }
    if (!g_prompt_loader.initialized) {
        AGENTOS_LOG_ERROR("PromptLoader: template_get called before init");
        return -1;
    }

    AGENTOS_LOG_DEBUG("PromptLoader: looking up template '%s' (category=%s)",
                      template_name, category ? category : "any");

    /* 先从缓存查找 */
    if (g_prompt_loader.config.enable_cache) {
        agentos_prompt_template_t *cached = NULL;
        if (cache_lookup(template_name, &cached) == 0) {
            *out_template = cached;
            AGENTOS_LOG_DEBUG("PromptLoader: cache hit for '%s'", template_name);
            return 0;
        }
        AGENTOS_LOG_DEBUG("PromptLoader: cache miss for '%s'", template_name);
    }

    /* 扫描目录查找 */
    char search_path[AGENTOS_PROMPT_PATH_MAX];
    const char *base_dir = g_prompt_loader.config.template_dir;

    if (category && category[0]) {
        snprintf(search_path, sizeof(search_path), "%s%s/%s.yaml",
                 base_dir, category, template_name);
    } else {
        /* 搜索所有类别 */
        static const char *categories[] = {
            "system", "cognition", "memory", "execution", "tools", NULL
        };
        for (int i = 0; categories[i]; i++) {
            snprintf(search_path, sizeof(search_path), "%s%s/%s.yaml",
                     base_dir, categories[i], template_name);
            struct stat st;
            if (stat(search_path, &st) == 0) {
                AGENTOS_LOG_DEBUG("PromptLoader: found '%s' in category '%s'",
                                  template_name, categories[i]);
                break;
            }
            search_path[0] = '\0';
        }
    }

    if (search_path[0] == '\0') {
        AGENTOS_LOG_WARN("PromptLoader: template '%s' not found in any category",
                         template_name);
        return -1;
    }

    /* 加载并缓存 */
    agentos_prompt_template_t *tmpl = NULL;
    if (agentos_prompt_template_load(search_path, &tmpl) != 0) {
        return -1;
    }

    if (g_prompt_loader.config.enable_cache) {
        cache_insert(tmpl);
    }

    *out_template = tmpl;
    return 0;
}

void agentos_prompt_template_free(agentos_prompt_template_t *template)
{
    if (!template) return;
    template_free_internal(template);
    AGENTOS_FREE(template);
}

/* ================================================================
 * 内部：模板清理
 * ================================================================ */

static void template_free_internal(agentos_prompt_template_t *tmpl)
{
    if (!tmpl) return;

    if (tmpl->system_prompt) {
        AGENTOS_FREE(tmpl->system_prompt);
        tmpl->system_prompt = NULL;
    }
    if (tmpl->user_template) {
        AGENTOS_FREE(tmpl->user_template);
        tmpl->user_template = NULL;
    }
    if (tmpl->output_schema) {
        AGENTOS_FREE(tmpl->output_schema);
        tmpl->output_schema = NULL;
    }

    tmpl->system_prompt_len = 0;
    tmpl->user_template_len = 0;
    tmpl->output_schema_len = 0;
    tmpl->is_loaded = false;
}

static int validate_required_fields(const agentos_prompt_template_t *tmpl)
{
    if (!tmpl) return -1;
    if (tmpl->name[0] == '\0') return -1;
    /* system_prompt 或 user_template 至少有一个 */
    if (!tmpl->system_prompt && !tmpl->user_template) return -1;
    return 0;
}

/* ================================================================
 * YAML 解析
 * ================================================================ */

static int parse_template_yaml(const char *yaml_content,
                                agentos_prompt_template_t *tmpl)
{
    if (!yaml_content || !tmpl) return -1;

    yaml_document_t *doc = yaml_create();
    if (!doc) return -1;

    if (yaml_parse_string(doc, yaml_content, strlen(yaml_content)) != 0) {
        yaml_destroy(doc);
        return -1;
    }

    struct yaml_node *root = yaml_root(doc);
    if (!root || root->type != YAML_NODE_MAPPING) {
        yaml_destroy(doc);
        return -1;
    }

    /* 解析基本字段 */
    const char *name = yaml_as_string(yaml_get(root, "name"), "");
    AGENTOS_STRNCPY_TERM(tmpl->name, name, sizeof(tmpl->name));

    const char *version = yaml_as_string(yaml_get(root, "version"), "1.0.0");
    AGENTOS_STRNCPY_TERM(tmpl->version, version, sizeof(tmpl->version));

    const char *desc = yaml_as_string(yaml_get(root, "description"), "");
    AGENTOS_STRNCPY_TERM(tmpl->description, desc, sizeof(tmpl->description));

    const char *model = yaml_as_string(yaml_get(root, "model_family"), "any");
    AGENTOS_STRNCPY_TERM(tmpl->model_family, model, sizeof(tmpl->model_family));

    tmpl->temperature = (float)yaml_as_double(
        yaml_get(root, "temperature"), 0.7);
    tmpl->max_tokens = (uint32_t)yaml_as_int64(
        yaml_get(root, "max_tokens"), 4096);

    /* 解析 system prompt */
    struct yaml_node *system_node = yaml_get(root, "system");
    if (system_node) {
        const char *system_text = yaml_as_string(system_node, NULL);
        if (system_text) {
            size_t slen = strlen(system_text);
            tmpl->system_prompt = (char *)AGENTOS_MALLOC(slen + 1);
            if (tmpl->system_prompt) {
                AGENTOS_MEMCPY(tmpl->system_prompt, system_text, slen + 1);
                tmpl->system_prompt_len = slen;
            }
        }
    }

    /* 解析 user_template */
    struct yaml_node *user_node = yaml_get(root, "user_template");
    if (user_node) {
        const char *user_text = yaml_as_string(user_node, NULL);
        if (user_text) {
            size_t ulen = strlen(user_text);
            tmpl->user_template = (char *)AGENTOS_MALLOC(ulen + 1);
            if (tmpl->user_template) {
                AGENTOS_MEMCPY(tmpl->user_template, user_text, ulen + 1);
                tmpl->user_template_len = ulen;
            }
        }
    }

    /* 解析 output_schema */
    struct yaml_node *schema_node = yaml_get(root, "output_schema");
    if (schema_node) {
        /* 将 output_schema YAML 子树序列化为紧凑 JSON */
        char schema_buf[8192];
        yaml_dump(schema_node, schema_buf, sizeof(schema_buf), 0);
        size_t slen = strlen(schema_buf);
        if (slen > 0) {
            tmpl->output_schema = (char *)AGENTOS_MALLOC(slen + 1);
            if (tmpl->output_schema) {
                AGENTOS_MEMCPY(tmpl->output_schema, schema_buf, slen + 1);
                tmpl->output_schema_len = slen;
            }
        }
    }

    /* 解析 metrics */
    struct yaml_node *metrics_node = yaml_get(root, "metrics");
    if (metrics_node) {
        parse_metrics_yaml(metrics_node, tmpl);
    }

    yaml_destroy(doc);
    return 0;
}

static int parse_metrics_yaml(struct yaml_node *metrics_node,
                               agentos_prompt_template_t *tmpl)
{
    if (!metrics_node || !tmpl) return -1;

    tmpl->target_precision = (float)yaml_as_double(
        yaml_get(metrics_node, "target_precision"), 0.85);
    tmpl->target_recall = (float)yaml_as_double(
        yaml_get(metrics_node, "target_recall"), 0.80);
    tmpl->max_hallucination_rate = (float)yaml_as_double(
        yaml_get(metrics_node, "max_hallucination_rate"), 0.05);

    return 0;
}

/* ================================================================
 * 目录扫描
 * ================================================================ */

static int scan_template_dir(const char *dir_path, int depth)
{
    if (!dir_path || depth > PROMPT_LOADER_MAX_SCAN_DEPTH) return 0;

    DIR *dir = opendir(dir_path);
    if (!dir) {
        /* 目录不存在时静默返回 */
        AGENTOS_LOG_DEBUG("PromptLoader: template dir not found: %s", dir_path);
        return 0;
    }

    AGENTOS_LOG_DEBUG("PromptLoader: scanning template dir: %s (depth=%d)",
                      dir_path, depth);

    int loaded_count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        const char *name = entry->d_name;

        /* 跳过隐藏文件 */
        if (name[0] == '.') continue;

        char full_path[AGENTOS_PROMPT_PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, name);

        struct stat st;
        if (stat(full_path, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            /* 递归扫描子目录 */
            loaded_count += scan_template_dir(full_path, depth + 1);
        } else if (S_ISREG(st.st_mode)) {
            /* 只加载 .yaml 和 .yml 文件 */
            size_t nlen = strlen(name);
            if (nlen > 5 &&
                (strcmp(name + nlen - 5, ".yaml") == 0 ||
                 strcmp(name + nlen - 4, ".yml") == 0)) {

                agentos_prompt_template_t *tmpl = NULL;
                if (agentos_prompt_template_load(full_path, &tmpl) == 0) {
                    if (g_prompt_loader.config.enable_cache) {
                        cache_insert(tmpl);
                    } else {
                        /* 未启用缓存，直接释放 */
                        agentos_prompt_template_free(tmpl);
                    }
                    loaded_count++;
                }
            }
        }
    }

    closedir(dir);

    AGENTOS_LOG_DEBUG("PromptLoader: scanned %s: %d templates loaded",
                      dir_path, loaded_count);

    return loaded_count;
}

/* ================================================================
 * 变量渲染
 * ================================================================ */

int agentos_prompt_render(const agentos_prompt_template_t *template,
                           const agentos_prompt_variable_t *variables,
                           size_t variable_count,
                           agentos_prompt_rendered_t **out_rendered)
{
    if (!template || !out_rendered) {
        AGENTOS_LOG_ERROR("PromptLoader: render called with NULL params");
        return -1;
    }
    if (!template->is_loaded) {
        AGENTOS_LOG_ERROR("PromptLoader: render called on unloaded template");
        return -1;
    }

    AGENTOS_LOG_DEBUG("PromptLoader: rendering template '%s' with %zu variables",
                      template->name, variable_count);

    agentos_prompt_rendered_t *rendered =
        (agentos_prompt_rendered_t *)AGENTOS_CALLOC(1, sizeof(*rendered));
    if (!rendered) {
        AGENTOS_LOG_ERROR("PromptLoader: OOM allocating rendered struct");
        return -1;
    }

    /* 渲染 system prompt */
    if (template->system_prompt && template->system_prompt_len > 0) {
        rendered->system_prompt = substitute_variables(
            template->system_prompt, template->system_prompt_len,
            variables, variable_count,
            &rendered->system_prompt_len);
        if (rendered->system_prompt) {
            AGENTOS_LOG_DEBUG("PromptLoader: rendered system prompt (%zu -> %zu chars)",
                              template->system_prompt_len,
                              rendered->system_prompt_len);
        }
    }

    /* 渲染 user_template */
    if (template->user_template && template->user_template_len > 0) {
        rendered->user_message = substitute_variables(
            template->user_template, template->user_template_len,
            variables, variable_count,
            &rendered->user_message_len);
        if (rendered->user_message) {
            AGENTOS_LOG_DEBUG("PromptLoader: rendered user message (%zu -> %zu chars)",
                              template->user_template_len,
                              rendered->user_message_len);
        }
    }

    rendered->temperature = template->temperature;
    rendered->max_tokens = template->max_tokens;
    AGENTOS_STRNCPY_TERM(rendered->model_family, template->model_family,
                         sizeof(rendered->model_family));

    AGENTOS_LOG_INFO("PromptLoader: render complete for '%s' (system=%zub, "
                     "user=%zub, temp=%.2f, max_tokens=%u)",
                     template->name,
                     rendered->system_prompt_len,
                     rendered->user_message_len,
                     rendered->temperature, rendered->max_tokens);

    *out_rendered = rendered;
    return 0;
}

void agentos_prompt_rendered_free(agentos_prompt_rendered_t *rendered)
{
    if (!rendered) return;

    if (rendered->system_prompt) {
        AGENTOS_FREE(rendered->system_prompt);
        rendered->system_prompt = NULL;
    }
    if (rendered->user_message) {
        AGENTOS_FREE(rendered->user_message);
        rendered->user_message = NULL;
    }

    AGENTOS_FREE(rendered);
}

/**
 * @brief 在模板字符串中替换 {variable_name}
 *
 * 扫描模板字符串，找到 {var_name} 格式的变量占位符，
 * 查表替换为实际值。未找到的变量保留原样。
 *
 * @param template_str 模板字符串
 * @param template_len 模板长度
 * @param variables 变量表
 * @param variable_count 变量数量
 * @param out_len 输出长度
 * @return 替换后的字符串（需调用者释放），失败返回 NULL
 */
static char *substitute_variables(const char *template_str,
                                   size_t template_len,
                                   const agentos_prompt_variable_t *variables,
                                   size_t variable_count,
                                   size_t *out_len)
{
    if (!template_str || !out_len) return NULL;

    /* 第一遍：计算输出长度 */
    size_t estimated_len = template_len;
    const char *p = template_str;
    const char *end = template_str + template_len;

    while (p < end) {
        if (*p == '{') {
            const char *close = (const char *)memchr(p + 1, '}', (size_t)(end - p - 1));
            if (close && close > p + 1) {
                size_t var_len = (size_t)(close - p - 1);
                /* 查找变量 */
                for (size_t i = 0; i < variable_count; i++) {
                    if (strlen(variables[i].name) == var_len &&
                        strncmp(p + 1, variables[i].name, var_len) == 0) {
                        estimated_len += strlen(variables[i].value);
                        estimated_len -= (var_len + 2); /* 减去 {var_name} 长度 */
                        break;
                    }
                }
                p = close + 1;
                continue;
            }
        }
        p++;
    }

    /* 分配输出缓冲区 */
    char *result = (char *)AGENTOS_MALLOC(estimated_len + 1);
    if (!result) return NULL;

    /* 第二遍：执行替换 */
    char *dst = result;
    p = template_str;

    while (p < end) {
        if (*p == '{') {
            const char *close = (const char *)memchr(p + 1, '}', (size_t)(end - p - 1));
            if (close && close > p + 1) {
                size_t var_len = (size_t)(close - p - 1);
                bool found = false;

                for (size_t i = 0; i < variable_count; i++) {
                    if (strlen(variables[i].name) == var_len &&
                        strncmp(p + 1, variables[i].name, var_len) == 0) {
                        const char *val = variables[i].value;
                        size_t val_len = strlen(val);
                        AGENTOS_MEMCPY(dst, val, val_len);
                        dst += val_len;
                        found = true;
                        break;
                    }
                }

                if (!found) {
                    /* 未找到变量，保留原样 */
                    size_t placeholder_len = (size_t)(close - p + 1);
                    AGENTOS_MEMCPY(dst, p, placeholder_len);
                    dst += placeholder_len;
                }
                p = close + 1;
                continue;
            }
        }
        *dst++ = *p++;
    }

    *dst = '\0';
    *out_len = (size_t)(dst - result);
    return result;
}

/* ================================================================
 * Schema 校验
 * ================================================================ */

int agentos_prompt_validate_output(const agentos_prompt_template_t *template,
                                    const char *output_json,
                                    size_t output_len,
                                    char **out_error)
{
    if (!template || !output_json) {
        if (out_error) *out_error = NULL;
        AGENTOS_LOG_ERROR("PromptLoader: validate_output called with NULL params");
        return -1;
    }

    /* 无 schema 定义时跳过校验 */
    if (!template->output_schema || template->output_schema_len == 0) {
        if (out_error) *out_error = NULL;
        AGENTOS_LOG_DEBUG("PromptLoader: no output_schema defined for '%s', "
                          "skipping validation", template->name);
        return -2;
    }

    if (!g_prompt_loader.config.enable_schema_validation) {
        if (out_error) *out_error = NULL;
        AGENTOS_LOG_DEBUG("PromptLoader: schema validation disabled, skipping");
        return 0;
    }

    AGENTOS_LOG_DEBUG("PromptLoader: validating output against schema for '%s' "
                      "(schema=%zub, output=%zub)",
                      template->name, template->output_schema_len, output_len);

    /* 解析 output_schema 为 YAML 节点 */
    yaml_document_t *schema_doc = yaml_create();
    if (!schema_doc) {
        if (out_error) *out_error = NULL;
        AGENTOS_LOG_ERROR("PromptLoader: failed to create schema YAML doc");
        return -1;
    }

    int ret = yaml_parse_string(schema_doc, template->output_schema,
                                 template->output_schema_len);
    if (ret != 0) {
        yaml_destroy(schema_doc);
        if (out_error) *out_error = NULL;
        AGENTOS_LOG_WARN("PromptLoader: failed to parse output_schema for '%s'",
                         template->name);
        return -1;
    }

    struct yaml_node *schema_root = yaml_root(schema_doc);
    if (!schema_root) {
        yaml_destroy(schema_doc);
        if (out_error) *out_error = NULL;
        AGENTOS_LOG_ERROR("PromptLoader: empty schema root for '%s'", template->name);
        return -1;
    }

    /* 解析待校验的 JSON 输出 */
    yaml_document_t *output_doc = yaml_create();
    if (!output_doc) {
        yaml_destroy(schema_doc);
        if (out_error) *out_error = NULL;
        AGENTOS_LOG_ERROR("PromptLoader: failed to create output YAML doc");
        return -1;
    }

    ret = yaml_parse_string(output_doc, output_json, output_len);
    if (ret != 0) {
        yaml_destroy(schema_doc);
        yaml_destroy(output_doc);
        if (out_error) {
            const char *err_msg = yaml_get_error(output_doc);
            if (err_msg) {
                *out_error = AGENTOS_STRDUP(err_msg);
            } else {
                *out_error = AGENTOS_STRDUP("JSON parse error");
            }
        }
        AGENTOS_LOG_WARN("PromptLoader: output JSON parse failed for '%s': %s",
                         template->name,
                         out_error && *out_error ? *out_error : "unknown error");
        return -1;
    }

    struct yaml_node *output_root = yaml_root(output_doc);
    int validation_result = 0;

    if (!output_root) {
        validation_result = -1;
        if (out_error) {
            *out_error = AGENTOS_STRDUP("Empty output");
        }
        AGENTOS_LOG_WARN("PromptLoader: empty output for '%s'", template->name);
    } else {
        /* 基本 Schema 校验 */
        /* 1. 检查 type */
        const char *expected_type = yaml_as_string(
            yaml_get(schema_root, "type"), "");
        if (expected_type[0]) {
            if (strcmp(expected_type, "object") == 0 &&
                output_root->type != YAML_NODE_MAPPING) {
                validation_result = -1;
                if (out_error) {
                    *out_error = AGENTOS_STRDUP(
                        "Expected type 'object' but got different type");
                }
                AGENTOS_LOG_WARN("PromptLoader: type mismatch for '%s' "
                                 "(expected=object, got=%d)",
                                 template->name, output_root->type);
            } else if (strcmp(expected_type, "array") == 0 &&
                       output_root->type != YAML_NODE_SEQUENCE) {
                validation_result = -1;
                if (out_error) {
                    *out_error = AGENTOS_STRDUP(
                        "Expected type 'array' but got different type");
                }
                AGENTOS_LOG_WARN("PromptLoader: type mismatch for '%s' "
                                 "(expected=array, got=%d)",
                                 template->name, output_root->type);
            }
        }

        /* 2. 检查 required 字段 */
        if (validation_result == 0) {
            struct yaml_node *required = yaml_get(schema_root, "required");
            if (required && required->type == YAML_NODE_SEQUENCE) {
                size_t req_count = yaml_size(required);
                for (size_t i = 0; i < req_count; i++) {
                    struct yaml_node *req_item = yaml_get_index(required, i);
                    const char *req_name = yaml_as_string(req_item, "");
                    if (req_name[0] && !yaml_has_key(output_root, req_name)) {
                        validation_result = -1;
                        if (out_error) {
                            char err_buf[256];
                            snprintf(err_buf, sizeof(err_buf),
                                     "Missing required field: '%s'", req_name);
                            *out_error = AGENTOS_STRDUP(err_buf);
                        }
                        AGENTOS_LOG_WARN("PromptLoader: missing required field "
                                         "'%s' in output for '%s'",
                                         req_name, template->name);
                        break;
                    }
                }
            }
        }
    }

    yaml_destroy(schema_doc);
    yaml_destroy(output_doc);

    if (validation_result == 0) {
        AGENTOS_LOG_INFO("PromptLoader: output validation passed for '%s'",
                         template->name);
    }

    return validation_result;
}

/* ================================================================
 * 缓存管理
 * ================================================================ */

static int cache_lookup(const char *name, agentos_prompt_template_t **out)
{
    if (!name || !out) return -1;

    agentos_mutex_lock(&g_prompt_loader.lock);

    for (size_t i = 0; i < g_prompt_loader.cache_count; i++) {
        if (g_prompt_loader.cache[i] &&
            strcmp(g_prompt_loader.cache[i]->name, name) == 0) {
            g_prompt_loader.hit_count++;
            *out = g_prompt_loader.cache[i];
            agentos_mutex_unlock(&g_prompt_loader.lock);
            return 0;
        }
    }

    g_prompt_loader.miss_count++;
    agentos_mutex_unlock(&g_prompt_loader.lock);
    return -1;
}

static void cache_insert(agentos_prompt_template_t *tmpl)
{
    if (!tmpl) return;

    agentos_mutex_lock(&g_prompt_loader.lock);

    /* 检查是否已存在，存在则更新 */
    for (size_t i = 0; i < g_prompt_loader.cache_count; i++) {
        if (g_prompt_loader.cache[i] &&
            strcmp(g_prompt_loader.cache[i]->name, tmpl->name) == 0) {
            AGENTOS_LOG_DEBUG("PromptLoader: cache update '%s' (slot=%zu)",
                              tmpl->name, i);
            template_free_internal(g_prompt_loader.cache[i]);
            AGENTOS_FREE(g_prompt_loader.cache[i]);
            g_prompt_loader.cache[i] = tmpl;
            agentos_mutex_unlock(&g_prompt_loader.lock);
            return;
        }
    }

    /* 缓存已满，移除最旧的条目 */
    if (g_prompt_loader.cache_count >= g_prompt_loader.config.cache_max_entries) {
        AGENTOS_LOG_DEBUG("PromptLoader: cache full (%zu/%zu), evicting oldest '%s'",
                          g_prompt_loader.cache_count,
                          g_prompt_loader.config.cache_max_entries,
                          g_prompt_loader.cache[0] ?
                          g_prompt_loader.cache[0]->name : "null");
        template_free_internal(g_prompt_loader.cache[0]);
        AGENTOS_FREE(g_prompt_loader.cache[0]);
        /* 移动所有条目 */
        AGENTOS_MEMMOVE(&g_prompt_loader.cache[0], &g_prompt_loader.cache[1],
                (g_prompt_loader.cache_count - 1) * sizeof(g_prompt_loader.cache[0]));
        g_prompt_loader.cache_count--;
    }

    g_prompt_loader.cache[g_prompt_loader.cache_count++] = tmpl;

    AGENTOS_LOG_DEBUG("PromptLoader: cache insert '%s' (slot=%zu, total=%zu)",
                      tmpl->name, g_prompt_loader.cache_count - 1,
                      g_prompt_loader.cache_count);

    agentos_mutex_unlock(&g_prompt_loader.lock);
}

void agentos_prompt_cache_clear(void)
{
    AGENTOS_LOG_INFO("PromptLoader: clearing cache (%zu entries)", 
                     g_prompt_loader.cache_count);

    agentos_mutex_lock(&g_prompt_loader.lock);

    for (size_t i = 0; i < g_prompt_loader.cache_count; i++) {
        template_free_internal(g_prompt_loader.cache[i]);
        AGENTOS_FREE(g_prompt_loader.cache[i]);
        g_prompt_loader.cache[i] = NULL;
    }
    g_prompt_loader.cache_count = 0;
    g_prompt_loader.hit_count = 0;
    g_prompt_loader.miss_count = 0;

    agentos_mutex_unlock(&g_prompt_loader.lock);

    AGENTOS_LOG_INFO("PromptLoader: cache cleared");
}

int agentos_prompt_reload_all(void)
{
    if (!g_prompt_loader.initialized) {
        AGENTOS_LOG_ERROR("PromptLoader: reload_all called before init");
        return -1;
    }

    AGENTOS_LOG_INFO("PromptLoader: reloading all templates from %s",
                     g_prompt_loader.config.template_dir);

    agentos_prompt_cache_clear();
    int loaded = scan_template_dir(g_prompt_loader.config.template_dir, 0);

    AGENTOS_LOG_INFO("PromptLoader: reload complete, %d templates loaded", loaded);
    return 0;
}

void agentos_prompt_cache_stats(size_t *out_entry_count,
                                 size_t *out_hit_count,
                                 size_t *out_miss_count)
{
    agentos_mutex_lock(&g_prompt_loader.lock);

    if (out_entry_count) *out_entry_count = g_prompt_loader.cache_count;
    if (out_hit_count)   *out_hit_count   = g_prompt_loader.hit_count;
    if (out_miss_count)  *out_miss_count  = g_prompt_loader.miss_count;

    agentos_mutex_unlock(&g_prompt_loader.lock);
}

void agentos_prompt_set_template_dir(const char *template_dir)
{
    if (!template_dir) return;

    AGENTOS_LOG_INFO("PromptLoader: changing template dir '%s' -> '%s'",
                     g_prompt_loader.config.template_dir, template_dir);

    agentos_mutex_lock(&g_prompt_loader.lock);
    AGENTOS_STRNCPY_TERM(g_prompt_loader.config.template_dir,
                         template_dir,
                         sizeof(g_prompt_loader.config.template_dir));
    agentos_mutex_unlock(&g_prompt_loader.lock);
}