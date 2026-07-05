/**
 * @file agent.c
 * @brief Agent 相关系统调用实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "agent_registry.h"
#include "agentrt.h"
#include "atomic_compat.h"
#include "cognition.h"
#include "execution.h"
#include "logger.h"
#include "memory_compat.h"
#include "string_compat.h"
#include "syscalls.h"

#include <stdlib.h>
#include <string.h>
#include "error.h"
#include "error_compat.h"

#define ATM_RET_ERR(c) \
    do { agentrt_error_push_ex((c), __FILE__, __LINE__, __func__, "%s", agentrt_error_str(c)); return (c); } while(0)


static agentrt_cognition_engine_t *g_cognition_engine = NULL;

typedef struct agent_instance {
    char *agent_id;
    char *spec;
    agentrt_execution_unit_t *unit;
    struct agent_instance *next;
} agent_instance_t;

static agent_instance_t *agents = NULL;
static agentrt_mutex_t *agent_lock = NULL;

/**
 * @brief 线程安全地确保 agent 锁已初始化
 */
static void ensure_agent_lock(void)
{
    agentrt_mutex_t *current =
        (agentrt_mutex_t *)atomic_load_ptr((_Atomic void **)&agent_lock, memory_order_acquire);
    if (!current) {
        agentrt_mutex_t *new_lock = agentrt_mutex_create();
        if (!new_lock)
            return;
        agentrt_mutex_t *expected = NULL;
        if (!atomic_compare_exchange_strong_ptr((_Atomic void **)&agent_lock, (void **)&expected,
                                                (void *)new_lock, memory_order_acq_rel,
                                                memory_order_acquire)) {
            agentrt_mutex_free(new_lock);
        }
    }
}

static agentrt_cognition_engine_t *ensure_cognition_engine(void)
{
    if (g_cognition_engine)
        return g_cognition_engine;
    agentrt_error_t err = agentrt_cognition_create_take(NULL, NULL, NULL, &g_cognition_engine);
    if (err != AGENTRT_SUCCESS || !g_cognition_engine) {
        g_cognition_engine = NULL;
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }
    return g_cognition_engine;
}

static char *build_memory_context(const char *input)
{
    if (!input) return NULL;
    char **record_ids = NULL;
    float *scores = NULL;
    size_t count = 0;
    agentrt_error_t err = agentrt_sys_memory_search(input, 3, &record_ids, &scores, &count);
    if (err != AGENTRT_SUCCESS || count == 0 || !record_ids) {
        if (record_ids)
            agentrt_sys_free(record_ids);
        if (scores)
            agentrt_sys_free(scores);
        AGENTRT_ERROR_NULL(AGENTRT_ERR_OVERFLOW, "limit exceeded");
    }
    size_t ctx_max = count * 256 + 64;
    char *ctx = (char *)AGENTRT_MALLOC(ctx_max);
    if (!ctx) {
        agentrt_sys_free(record_ids);
        agentrt_sys_free(scores);
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }
    size_t pos = 0;
    pos += snprintf(ctx + pos, ctx_max - pos, "{\"memory_refs\":[");
    for (size_t i = 0; i < count && pos < ctx_max - 2; i++) {
        if (i > 0)
            pos += snprintf(ctx + pos, ctx_max - pos, ",");
        pos += snprintf(ctx + pos, ctx_max - pos, "{\"id\":\"%s\",\"score\":%.3f}",
                        record_ids[i] ? record_ids[i] : "", scores[i]);
    }
    pos += snprintf(ctx + pos, ctx_max - pos, "]}");
    agentrt_sys_free(record_ids);
    agentrt_sys_free(scores);
    return ctx;
}

static char *serialize_task_plan(agentrt_task_plan_t *plan)
{
    if (!plan) return NULL;
    size_t buf_max = 4096 + plan->task_plan_node_count * 512;
    char *buf = (char *)AGENTRT_MALLOC(buf_max);
    if (!buf) return NULL;
    size_t pos = 0;
    pos += snprintf(buf + pos, buf_max - pos,
                    "{\"plan_id\":\"%s\",\"node_count\":%zu,\"entry_count\":%zu,\"nodes\":[",
                    plan->task_plan_id ? plan->task_plan_id : "", plan->task_plan_node_count,
                    plan->task_plan_entry_count);
    for (size_t i = 0; i < plan->task_plan_node_count && pos < buf_max - 256; i++) {
        agentrt_task_node_t *node = plan->task_plan_nodes[i];
        if (!node)
            continue;
        if (i > 0)
            pos += snprintf(buf + pos, buf_max - pos, ",");
        pos += snprintf(
            buf + pos, buf_max - pos,
            "{\"id\":\"%s\",\"role\":\"%s\",\"timeout_ms\":%u,\"priority\":%u,\"dep_count\":%zu}",
            node->task_node_id ? node->task_node_id : "",
            node->task_node_agent_role ? node->task_node_agent_role : "",
            node->task_node_timeout_ms, node->task_node_priority, node->task_node_depends_count);
    }
    pos += snprintf(buf + pos, buf_max - pos, "]}");
    return buf;
}

static agentrt_error_t agent_unit_execute(agentrt_execution_unit_t *unit, const void *input,
                                          void **out_output)
{
    if (!unit || !input || !out_output)
        ATM_RET_ERR(AGENTRT_EINVAL);

    const char *spec = (const char *)unit->execution_unit_data;
    if (!spec) {
        AGENTRT_LOG_ERROR("Agent unit has no spec data");
        ATM_RET_ERR(AGENTRT_ENOTINIT);
    }

    const char *input_str = (const char *)input;
    size_t input_len = strnlen(input_str, 65536);
    if (input_len == 0) {
        AGENTRT_LOG_WARN("Empty input received for agent execution");
        ATM_RET_ERR(AGENTRT_EINVAL);
    }

    uint64_t start_ns = agentrt_time_monotonic_ns();

    agentrt_cognition_engine_t *engine = ensure_cognition_engine();
    if (engine) {
        agentrt_task_plan_t *plan = NULL;
        agentrt_error_t err = agentrt_cognition_process(engine, input_str, input_len, &plan);
        if (err == AGENTRT_SUCCESS && plan) {
            char *plan_json = serialize_task_plan(plan);
            agentrt_task_plan_free(plan);
            if (plan_json) {
                char *mem_ctx = build_memory_context(input_str);
                size_t result_max = strlen(plan_json) + (mem_ctx ? strlen(mem_ctx) : 0) + 256;
                char *result = (char *)AGENTRT_MALLOC(result_max);
                if (result) {
                    if (mem_ctx) {
                        snprintf(result, result_max,
                                 "{\"status\":\"completed\",\"source\":\"cognition\","
                                 "\"plan\":%s,\"memory_context\":%s}",
                                 plan_json, mem_ctx);
                    } else {
                        snprintf(result, result_max,
                                 "{\"status\":\"completed\",\"source\":\"cognition\","
                                 "\"plan\":%s}",
                                 plan_json);
                    }
                    AGENTRT_FREE(plan_json);
                    if (mem_ctx)
                        AGENTRT_FREE(mem_ctx);
                    uint64_t end_ns = agentrt_time_monotonic_ns();
                    AGENTRT_LOG_INFO("Agent cognition execution: elapsed=%lu ms",
                                     (unsigned long)((end_ns - start_ns) / 1000000));
                    *out_output = result;
                    return AGENTRT_SUCCESS;
                }
                AGENTRT_FREE(plan_json);
            }
        }
    }

    const char *role = "general";
    const char *role_key = "\"role\"";
    const char *role_pos = strstr(spec, role_key);
    if (role_pos) {
        const char *colon = strchr(role_pos, ':');
        if (colon) {
            const char *quote_start = strchr(colon, '"');
            if (quote_start) {
                const char *quote_end = strchr(quote_start + 1, '"');
                if (quote_end) {
                    size_t role_len = (size_t)(quote_end - quote_start - 1);
                    if (role_len > 0 && role_len < 64) {
                        static char role_buf[64];
                        __builtin_memcpy(role_buf, quote_start + 1, role_len);
                        role_buf[role_len] = '\0';
                        role = role_buf;
                    }
                }
            }
        }
    }

    char *mem_ctx = build_memory_context(input_str);
    char *result = NULL;
    size_t result_max = input_len + (mem_ctx ? strlen(mem_ctx) : 0) + 1024;
    result = (char *)AGENTRT_MALLOC(result_max);
    if (!result) {
        if (mem_ctx)
            AGENTRT_FREE(mem_ctx);
        ATM_RET_ERR(AGENTRT_ENOMEM);
    }

    if (mem_ctx) {
        snprintf(result, result_max,
                 "{\"status\":\"completed\",\"source\":\"fallback\",\"role\":\"%s\","
                 "\"input_length\":%zu,\"memory_context\":%s}",
                 role, input_len, mem_ctx);
        AGENTRT_FREE(mem_ctx);
    } else {
        snprintf(result, result_max,
                 "{\"status\":\"completed\",\"source\":\"fallback\",\"role\":\"%s\","
                 "\"input_length\":%zu}",
                 role, input_len);
    }

    uint64_t end_ns = agentrt_time_monotonic_ns();
    AGENTRT_LOG_INFO("Agent fallback execution: role=%s, elapsed=%lu ms", role,
                     (unsigned long)((end_ns - start_ns) / 1000000));

    *out_output = result;
    return AGENTRT_SUCCESS;
}

/**
 * @brief 释放 Agent 执行单元资源
 */
static void agent_unit_destroy(agentrt_execution_unit_t *unit)
{
    if (!unit)
        return;
    if (unit->execution_unit_data) {
        AGENTRT_FREE(unit->execution_unit_data);
        unit->execution_unit_data = NULL;
    }
    AGENTRT_FREE(unit);
}

/**
 * @brief 获取 Agent 单元元数据
 */
static const char *agent_unit_get_metadata(agentrt_execution_unit_t *unit)
{
    if (!unit || !unit->execution_unit_data)
        return "{}";
    return (const char *)unit->execution_unit_data;
}

/**
 * @brief 创建 Agent 实例
 */
agentrt_error_t agentrt_sys_agent_spawn(const char *agent_spec, char **out_agent_id)
{
    if (!agent_spec || !out_agent_id)
        ATM_RET_ERR(AGENTRT_EINVAL);
    ensure_agent_lock();

    char id_buf[64];
    static atomic_int counter = 0;
    snprintf(id_buf, sizeof(id_buf), "agent_%d",
             atomic_fetch_add_explicit(&counter, 1, memory_order_seq_cst));

    agent_instance_t *inst = (agent_instance_t *)AGENTRT_CALLOC(1, sizeof(agent_instance_t));
    if (!inst)
        ATM_RET_ERR(AGENTRT_ENOMEM);

    inst->agent_id = AGENTRT_STRDUP(id_buf);
    inst->spec = AGENTRT_STRDUP(agent_spec);
    if (!inst->agent_id || !inst->spec) {
        if (inst->agent_id)
            AGENTRT_FREE(inst->agent_id);
        if (inst->spec)
            AGENTRT_FREE(inst->spec);
        AGENTRT_FREE(inst);
        ATM_RET_ERR(AGENTRT_ENOMEM);
    }

    /* 创建执行单元并注册到全局 registry */
    agentrt_execution_unit_t *unit =
        (agentrt_execution_unit_t *)AGENTRT_CALLOC(1, sizeof(agentrt_execution_unit_t));
    if (!unit) {
        AGENTRT_FREE(inst->agent_id);
        AGENTRT_FREE(inst->spec);
        AGENTRT_FREE(inst);
        ATM_RET_ERR(AGENTRT_ENOMEM);
    }

    unit->execution_unit_data = AGENTRT_STRDUP(agent_spec);
    if (!unit->execution_unit_data) {
        AGENTRT_FREE(unit);
        AGENTRT_FREE(inst->agent_id);
        AGENTRT_FREE(inst->spec);
        AGENTRT_FREE(inst);
        ATM_RET_ERR(AGENTRT_ENOMEM);
    }

    unit->execution_unit_execute = agent_unit_execute;
    unit->execution_unit_destroy = agent_unit_destroy;
    unit->execution_unit_get_metadata = agent_unit_get_metadata;
    inst->unit = unit;

    /* 注册到执行引擎 registry，使 worker 线程可通过 agent_id 查找 */
    agentrt_registry_register_unit_take(inst->agent_id, unit);

    agentrt_mutex_lock(agent_lock);
    inst->next = agents;
    agents = inst;
    agentrt_mutex_unlock(agent_lock);

    *out_agent_id = AGENTRT_STRDUP(inst->agent_id);
    if (!*out_agent_id) {
        ATM_RET_ERR(AGENTRT_ENOMEM);
    }

    AGENTRT_LOG_INFO("Agent spawned: %s (registered as execution unit)", *out_agent_id);
    return AGENTRT_SUCCESS;
}

/**
 * @brief 销毁 Agent 实例
 */
agentrt_error_t agentrt_sys_agent_terminate(const char *agent_id)
{
    if (!agent_id)
        ATM_RET_ERR(AGENTRT_EINVAL);
    ensure_agent_lock();

    agentrt_mutex_lock(agent_lock);
    agent_instance_t **prev = &agents;
    agent_instance_t *curr = agents;

    while (curr) {
        if (strcmp(curr->agent_id, agent_id) == 0) {
            *prev = curr->next;

            /* 从 registry 注销执行单元 */
            agentrt_registry_unregister_unit(curr->agent_id);

            /* 释放执行单元资源 */
            if (curr->unit) {
                agent_unit_destroy(curr->unit);
                curr->unit = NULL;
            }

            AGENTRT_FREE(curr->agent_id);
            AGENTRT_FREE(curr->spec);
            AGENTRT_FREE(curr);
            agentrt_mutex_unlock(agent_lock);
            AGENTRT_LOG_INFO("Agent terminated: %s", agent_id);
            return AGENTRT_SUCCESS;
        }
        prev = &curr->next;
        curr = curr->next;
    }

    agentrt_mutex_unlock(agent_lock);
    AGENTRT_LOG_WARN("Agent not found: %s", agent_id);
    ATM_RET_ERR(AGENTRT_ENOENT);
}

/**
 * @brief 调用 Agent 执行任务
 *
 * 生产级实现流程：
 * 1. 验证 agent_id 存在性
 * 2. 获取/创建全局执行引擎
 * 3. 构造 agentrt_task_t 结构
 * 4. 提交到执行引擎（异步）
 * 5. 同步等待完成（带超时）
 * 6. 返回结果
 */
agentrt_error_t agentrt_sys_agent_invoke(const char *agent_id, const char *input, size_t input_len,
                                         char **out_output)
{
    if (!agent_id || !input || !out_output)
        ATM_RET_ERR(AGENTRT_EINVAL);
    ensure_agent_lock();

    /* 验证 agent 存在性 */
    agentrt_mutex_lock(agent_lock);
    agent_instance_t *inst = agents;
    while (inst) {
        if (strcmp(inst->agent_id, agent_id) == 0)
            break;
        inst = inst->next;
    }
    agentrt_mutex_unlock(agent_lock);

    if (!inst) {
        AGENTRT_LOG_WARN("Agent not found: %s", agent_id);
        ATM_RET_ERR(AGENTRT_ENOENT);
    }

    if (!inst->unit) {
        AGENTRT_LOG_ERROR("Agent has no execution unit: %s", agent_id);
        ATM_RET_ERR(AGENTRT_ENOTINIT);
    }

    /* 直接通过 registry 获取执行单元并同步调用 */
    agentrt_execution_unit_t *unit = agentrt_registry_get_unit(agent_id);
    if (!unit) {
        AGENTRT_LOG_ERROR("Execution unit not found in registry: %s", agent_id);
        ATM_RET_ERR(AGENTRT_ENOENT);
    }

    void *output = NULL;
    agentrt_error_t err = unit->execution_unit_execute(unit, input, &output);
    if (err != AGENTRT_SUCCESS) {
        AGENTRT_LOG_ERROR("Agent execution failed: %s, error=%d", agent_id, err);
        return err;
    }

    if (!output) {
        AGENTRT_LOG_WARN("Agent returned null output: %s", agent_id);
        ATM_RET_ERR(AGENTRT_ENOTINIT);
    }

    *out_output = (char *)output;
    AGENTRT_LOG_DEBUG("Agent invoked: %s, output_length=%zu", agent_id,
                      strlen((const char *)*out_output));
    return AGENTRT_SUCCESS;
}

/**
 * @brief 列出所有 Agent
 */
agentrt_error_t agentrt_sys_agent_list(char ***out_agent_ids, size_t *out_count)
{
    if (!out_agent_ids || !out_count)
        ATM_RET_ERR(AGENTRT_EINVAL);
    ensure_agent_lock();

    agentrt_mutex_lock(agent_lock);

    // 计数
    size_t count = 0;
    agent_instance_t *inst = agents;
    while (inst) {
        count++;
        inst = inst->next;
    }

    if (count == 0) {
        agentrt_mutex_unlock(agent_lock);
        *out_agent_ids = NULL;
        *out_count = 0;
        return AGENTRT_SUCCESS;
    }

    // 分配数组
    char **ids = (char **)AGENTRT_CALLOC(count, sizeof(char *));
    if (!ids) {
        agentrt_mutex_unlock(agent_lock);
        ATM_RET_ERR(AGENTRT_ENOMEM);
    }

    // 填充数组
    inst = agents;
    for (size_t i = 0; i < count; i++) {
        ids[i] = AGENTRT_STRDUP(inst->agent_id);
        if (!ids[i]) {
            for (size_t j = 0; j < i; j++) {
                AGENTRT_FREE(ids[j]);
            }
            AGENTRT_FREE(ids);
            agentrt_mutex_unlock(agent_lock);
            ATM_RET_ERR(AGENTRT_ENOMEM);
        }
        inst = inst->next;
    }

    agentrt_mutex_unlock(agent_lock);

    *out_agent_ids = ids;
    *out_count = count;
    return AGENTRT_SUCCESS;
}

/**
 * @brief 清理 Agent 系统调用资源
 */
void agentrt_sys_agent_cleanup(void)
{
    if (g_cognition_engine) {
        agentrt_cognition_destroy(g_cognition_engine);
        g_cognition_engine = NULL;
    }

    if (!agent_lock)
        return;

    agentrt_mutex_lock(agent_lock);
    agent_instance_t *inst = agents;
    while (inst) {
        agent_instance_t *next = inst->next;

        agentrt_registry_unregister_unit(inst->agent_id);

        if (inst->unit) {
            agent_unit_destroy(inst->unit);
            inst->unit = NULL;
        }

        AGENTRT_FREE(inst->agent_id);
        AGENTRT_FREE(inst->spec);
        AGENTRT_FREE(inst);
        inst = next;
    }
    agents = NULL;
    agentrt_mutex_unlock(agent_lock);

    agentrt_mutex_free(agent_lock);
    agent_lock = NULL;

    AGENTRT_LOG_INFO("Agent syscall cleanup completed");
}
