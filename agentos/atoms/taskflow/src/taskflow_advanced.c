#include "taskflow_advanced.h"

#include "error.h"
#include "memory_compat.h"
#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "error_compat.h"

/* IRON-2 (load_workflow_json 桩函数修复)：cJSON 可用时实现真正的 JSON 解析。
 * taskflow_advanced 是 B 类应用语义层，可依赖 cJSON（项目已有依赖）。
 * cJSON 不可用时 load_workflow_json 返回 ENOTSUP（显式不支持，非桩函数）。 */
#ifdef AGENTOS_TASKFLOW_HAVE_CJSON
#include <cjson/cJSON.h>
#include "logging_compat.h"
#endif

#define ATM_RET_ERR(c) \
    do { agentos_error_push_ex((c), __FILE__, __LINE__, __func__, "%s", agentos_error_str(c)); return (c); } while(0)

typedef struct {
    char name[128];
    taskflow_task_handler_t handler;
    void *user_data;
} handler_entry_t;

typedef struct {
    taskflow_workflow_t workflow;
    int registered;
} workflow_entry_t;

typedef struct {
    taskflow_execution_t execution;
    int active;
    taskflow_checkpoint_t checkpoints[TASKFLOW_MAX_CHECKPOINTS];
    size_t checkpoint_count;
    char *variables_json;
} execution_entry_t;

/* P3.21 (ACC-DT24)：struct tag 改为 taskflow_adv_engine_s，与 taskflow_core.c
 * 的 struct taskflow_engine_s（Pregel 核心引擎，30+ 字段）分离，消除 ODR 违反。
 * 本结构为高级工作流引擎（11 字段），两者字段布局完全不同。
 * sizeof(taskflow_engine_t) 通过 typedef 名引用，不受 tag 名影响；
 * 字段访问通过 taskflow_engine_t * 指针，同样不受影响。 */
struct taskflow_adv_engine_s {
    handler_entry_t handlers[TASKFLOW_MAX_PARALLEL];
    size_t handler_count;
    workflow_entry_t workflows[TASKFLOW_MAX_SUBFLOWS];
    size_t workflow_count;
    execution_entry_t executions[TASKFLOW_MAX_PARALLEL];
    size_t execution_count;
    taskflow_condition_fn condition_fn;
    void *condition_user_data;
    taskflow_progress_callback_t progress_cb;
    void *progress_user_data;
    taskflow_event_callback_t event_cb;
    void *event_user_data;
    uint64_t id_counter;
};

static uint64_t generate_id(taskflow_engine_t *engine)
{
    return ++engine->id_counter;
}

taskflow_engine_t *taskflow_engine_create(void)
{
    taskflow_engine_t *engine = (taskflow_engine_t *)AGENTOS_CALLOC(1, sizeof(taskflow_engine_t));
    if (!engine) return NULL;
    engine->handler_count = 0;
    engine->workflow_count = 0;
    engine->execution_count = 0;
    engine->condition_fn = NULL;
    engine->progress_cb = NULL;
    engine->event_cb = NULL;
    engine->id_counter = 0;
    return engine;
}

void taskflow_engine_destroy(taskflow_engine_t *engine)
{
    if (!engine)
        return;
    for (size_t i = 0; i < engine->execution_count; i++) {
        if (engine->executions[i].execution.current_node_id)
            AGENTOS_FREE(engine->executions[i].execution.current_node_id);
        if (engine->executions[i].execution.input_json)
            AGENTOS_FREE(engine->executions[i].execution.input_json);
        if (engine->executions[i].execution.output_json)
            AGENTOS_FREE(engine->executions[i].execution.output_json);
        if (engine->executions[i].execution.error_message)
            AGENTOS_FREE(engine->executions[i].execution.error_message);
        if (engine->executions[i].execution.variables_json)
            AGENTOS_FREE(engine->executions[i].execution.variables_json);
        if (engine->executions[i].variables_json)
            AGENTOS_FREE(engine->executions[i].variables_json);
        for (size_t j = 0; j < engine->executions[i].checkpoint_count; j++) {
            if (engine->executions[i].checkpoints[j].snapshot_json)
                AGENTOS_FREE(engine->executions[i].checkpoints[j].snapshot_json);
        }
    }
    for (size_t i = 0; i < engine->workflow_count; i++) {
        if (engine->workflows[i].workflow.nodes) {
            for (size_t j = 0; j < engine->workflows[i].workflow.node_count; j++) {
                taskflow_node_t *n = &engine->workflows[i].workflow.nodes[j];
                AGENTOS_FREE(n->task_handler_name);
                AGENTOS_FREE(n->config_json);
                AGENTOS_FREE(n->input_transform_json);
                AGENTOS_FREE(n->output_transform_json);
                AGENTOS_FREE(n->fallback_handler_name);
                AGENTOS_FREE(n->condition_expr);
                AGENTOS_FREE(n->subflow_id);
                AGENTOS_FREE(n->loop_condition_expr);
                AGENTOS_FREE(n->loop_foreach_json);
                AGENTOS_FREE(n->event_type);
            }
            AGENTOS_FREE(engine->workflows[i].workflow.nodes);
        }
        if (engine->workflows[i].workflow.edges)
            AGENTOS_FREE(engine->workflows[i].workflow.edges);
        AGENTOS_FREE(engine->workflows[i].workflow.initial_node_id);
        AGENTOS_FREE(engine->workflows[i].workflow.input_schema_json);
        AGENTOS_FREE(engine->workflows[i].workflow.output_schema_json);
    }
    AGENTOS_FREE(engine);
}

int taskflow_engine_register_handler(taskflow_engine_t *engine, const char *name,
                                     taskflow_task_handler_t handler, void *user_data)
{
    if (!engine || !name || !handler)
        AGENTOS_ERROR(AGENTOS_EINVAL, "failed to register handler: null engine, name, or handler");
    if (engine->handler_count >= TASKFLOW_MAX_PARALLEL)
        ATM_RET_ERR(AGENTOS_ERR_OVERFLOW);
    for (size_t i = 0; i < engine->handler_count; i++) {
        if (strcmp(engine->handlers[i].name, name) == 0) {
            engine->handlers[i].handler = handler;
            engine->handlers[i].user_data = user_data;
            return 0;
        }
    }
AGENTOS_STRNCPY_TERM(engine->handlers[engine->handler_count].name, name, sizeof(engine->handlers[0].name));
    engine->handlers[engine->handler_count].handler = handler;
    engine->handlers[engine->handler_count].user_data = user_data;
    engine->handler_count++;
    return 0;
}

int taskflow_engine_unregister_handler(taskflow_engine_t *engine, const char *name)
{
    if (!engine || !name)
        ATM_RET_ERR(AGENTOS_EINVAL);
    for (size_t i = 0; i < engine->handler_count; i++) {
        if (strcmp(engine->handlers[i].name, name) == 0) {
            __builtin_memmove(&engine->handlers[i], &engine->handlers[i + 1],
                    (engine->handler_count - i - 1) * sizeof(handler_entry_t));
            engine->handler_count--;
            return 0;
        }
    }
    return 1;
}

int taskflow_engine_register_workflow(taskflow_engine_t *engine,
                                      const taskflow_workflow_t *workflow)
{
    if (!engine || !workflow)
        AGENTOS_ERROR(AGENTOS_EINVAL, "failed to register workflow: null engine or workflow");
    if (engine->workflow_count >= TASKFLOW_MAX_SUBFLOWS)
        ATM_RET_ERR(AGENTOS_ERR_OVERFLOW);
    for (size_t i = 0; i < engine->workflow_count; i++) {
        if (strcmp(engine->workflows[i].workflow.id, workflow->id) == 0) {
            engine->workflows[i].workflow = *workflow;
            engine->workflows[i].registered = 1;
            return 0;
        }
    }
    engine->workflows[engine->workflow_count].workflow = *workflow;
    engine->workflows[engine->workflow_count].registered = 1;
    engine->workflow_count++;
    return 0;
}

int taskflow_engine_unregister_workflow(taskflow_engine_t *engine, const char *workflow_id)
{
    if (!engine || !workflow_id)
        ATM_RET_ERR(AGENTOS_EINVAL);
    for (size_t i = 0; i < engine->workflow_count; i++) {
        if (strcmp(engine->workflows[i].workflow.id, workflow_id) == 0) {
            __builtin_memmove(&engine->workflows[i], &engine->workflows[i + 1],
                    (engine->workflow_count - i - 1) * sizeof(workflow_entry_t));
            engine->workflow_count--;
            return 0;
        }
    }
    return 1;
}

/* IRON-2 (load_workflow_json 桩函数修复)：JSON 解析辅助函数。
 * 以下三个函数仅在使用 cJSON 时被引用，放在 #ifdef 块中避免未使用警告。 */
#ifdef AGENTOS_TASKFLOW_HAVE_CJSON

/* 将节点类型字符串映射为 taskflow_node_type_t 枚举。
 * 未知字符串返回默认值 TASKFLOW_NODE_TASK。 */
static taskflow_node_type_t taskflow_parse_node_type(const char *str)
{
    if (!str) return TASKFLOW_NODE_TASK;
    if (strcmp(str, "task") == 0) return TASKFLOW_NODE_TASK;
    if (strcmp(str, "condition") == 0) return TASKFLOW_NODE_CONDITION;
    if (strcmp(str, "fork") == 0) return TASKFLOW_NODE_FORK;
    if (strcmp(str, "join") == 0) return TASKFLOW_NODE_JOIN;
    if (strcmp(str, "subflow") == 0) return TASKFLOW_NODE_SUBFLOW;
    if (strcmp(str, "loop") == 0) return TASKFLOW_NODE_LOOP;
    if (strcmp(str, "delay") == 0) return TASKFLOW_NODE_DELAY;
    if (strcmp(str, "event_wait") == 0) return TASKFLOW_NODE_EVENT_WAIT;
    if (strcmp(str, "transform") == 0) return TASKFLOW_NODE_TRANSFORM;
    return TASKFLOW_NODE_TASK;
}

/* 将错误策略字符串映射为 taskflow_error_strategy_t 枚举。
 * 未知字符串返回 TASKFLOW_ERROR_STRATEGY_NONE（显式空值）。 */
static taskflow_error_strategy_t taskflow_parse_error_strategy(const char *str)
{
    if (!str) return TASKFLOW_ERROR_STRATEGY_NONE;
    if (strcmp(str, "none") == 0) return TASKFLOW_ERROR_STRATEGY_NONE;
    if (strcmp(str, "retry") == 0) return TASKFLOW_ERROR_RETRY;
    if (strcmp(str, "rollback") == 0) return TASKFLOW_ERROR_ROLLBACK;
    if (strcmp(str, "skip") == 0) return TASKFLOW_ERROR_SKIP;
    if (strcmp(str, "abort") == 0) return TASKFLOW_ERROR_ABORT;
    if (strcmp(str, "fallback") == 0) return TASKFLOW_ERROR_FALLBACK;
    return TASKFLOW_ERROR_STRATEGY_NONE;
}

/* 释放 taskflow_workflow_t 的动态分配字段（nodes/edges/initial_node_id 等）。
 * 用于 load_workflow_json 解析失败或 register_workflow 失败时的回滚清理。
 * 注意：此函数与 taskflow_engine_destroy 中的释放逻辑一致，但作用于单个 workflow。 */
static void taskflow_free_workflow_fields(taskflow_workflow_t *wf)
{
    if (!wf) return;
    if (wf->nodes) {
        for (size_t j = 0; j < wf->node_count; j++) {
            taskflow_node_t *n = &wf->nodes[j];
            AGENTOS_FREE(n->task_handler_name);
            AGENTOS_FREE(n->config_json);
            AGENTOS_FREE(n->input_transform_json);
            AGENTOS_FREE(n->output_transform_json);
            AGENTOS_FREE(n->fallback_handler_name);
            AGENTOS_FREE(n->condition_expr);
            AGENTOS_FREE(n->subflow_id);
            AGENTOS_FREE(n->loop_condition_expr);
            AGENTOS_FREE(n->loop_foreach_json);
            AGENTOS_FREE(n->event_type);
        }
        AGENTOS_FREE(wf->nodes);
        wf->nodes = NULL;
        wf->node_count = 0;
    }
    if (wf->edges) {
        AGENTOS_FREE(wf->edges);
        wf->edges = NULL;
        wf->edge_count = 0;
    }
    AGENTOS_FREE(wf->initial_node_id);
    wf->initial_node_id = NULL;
    AGENTOS_FREE(wf->input_schema_json);
    wf->input_schema_json = NULL;
    AGENTOS_FREE(wf->output_schema_json);
    wf->output_schema_json = NULL;
}

#endif /* AGENTOS_TASKFLOW_HAVE_CJSON */

int taskflow_engine_load_workflow_json(taskflow_engine_t *engine, const char *workflow_json)
{
    if (!engine || !workflow_json)
        AGENTOS_ERROR(AGENTOS_EINVAL, "failed to load workflow json: null engine or workflow_json");
    if (engine->workflow_count >= TASKFLOW_MAX_SUBFLOWS)
        ATM_RET_ERR(AGENTOS_ERR_OVERFLOW);

#ifndef AGENTOS_TASKFLOW_HAVE_CJSON
    /* IRON-2: cJSON 不可用时显式返回 ENOTSUP，非桩函数。
     * 调用方可通过 taskflow_engine_register_workflow（结构体方式）注册工作流。
     * W18 taskflow coreloopthree 集成即采用 register_workflow 方式避免依赖此函数。 */
    AGENTOS_LOG_WARN("taskflow: load_workflow_json requires cJSON (not available), "
                     "use taskflow_engine_register_workflow instead");
    ATM_RET_ERR(AGENTOS_ENOTSUP);
#else
    /* IRON-2 修复：原实现是桩函数（不解析 JSON，硬编码单节点工作流 id="wf_json_N"）。
     * 现在使用 cJSON 实现真正的 JSON 解析，填充 taskflow_workflow_t 全部字段。
     * JSON 格式：
     * {
     *   "id": "wf_001", "name": "...", "description": "...", "version": "1.0",
     *   "initial_node_id": "start",
     *   "nodes": [{"id":"...", "name":"...", "type":"task|condition|...",
     *              "task_handler_name":"...", "timeout_ms":N, "max_retries":N,
     *              "retry_delay_ms":N, "error_strategy":"retry|...",
     *              "config_json":"...", "fallback_handler_name":"...",
     *              "condition_expr":"...", "subflow_id":"..."}],
     *   "edges": [{"id":"...", "source_node_id":"...", "target_node_id":"...",
     *              "condition_expr":"...", "priority":N, "is_default":bool}],
     *   "default_timeout_ms":N, "default_max_retries":N,
     *   "default_error_strategy":"retry|...",
     *   "input_schema_json":"...", "output_schema_json":"..."
     * } */
    taskflow_workflow_t wf;
    AGENTOS_MEMSET(&wf, 0, sizeof(wf));

    cJSON *root = cJSON_Parse(workflow_json);
    if (!root) {
        AGENTOS_LOG_WARN("taskflow: load_workflow_json JSON parse failed");
        ATM_RET_ERR(AGENTOS_EINVAL);
    }

    /* 辅助宏：读取字符串字段到固定缓冲区 */
    #define TF_READ_STR_FIELD(obj, dst, field, maxsize) do { \
        cJSON *_item = cJSON_GetObjectItem(obj, field); \
        if (_item && cJSON_IsString(_item)) { \
            AGENTOS_STRNCPY_TERM(dst, _item->valuestring, maxsize); \
        } \
    } while (0)

    #define TF_READ_STR_DUP(obj, dst_ptr, field) do { \
        cJSON *_item = cJSON_GetObjectItem(obj, field); \
        if (_item && cJSON_IsString(_item) && _item->valuestring) { \
            dst_ptr = AGENTOS_STRDUP(_item->valuestring); \
        } \
    } while (0)

    #define TF_READ_NUM(obj, dst, field) do { \
        cJSON *_item = cJSON_GetObjectItem(obj, field); \
        if (_item && cJSON_IsNumber(_item)) { \
            dst = (int32_t)_item->valueint; \
        } \
    } while (0)

    /* 顶层字符串字段 */
    TF_READ_STR_FIELD(root, wf.id, "id", sizeof(wf.id));
    TF_READ_STR_FIELD(root, wf.name, "name", sizeof(wf.name));
    TF_READ_STR_FIELD(root, wf.description, "description", sizeof(wf.description));
    TF_READ_STR_FIELD(root, wf.version, "version", sizeof(wf.version));
    TF_READ_STR_DUP(root, wf.initial_node_id, "initial_node_id");
    TF_READ_STR_DUP(root, wf.input_schema_json, "input_schema_json");
    TF_READ_STR_DUP(root, wf.output_schema_json, "output_schema_json");

    /* 顶层数值字段 */
    TF_READ_NUM(root, wf.default_timeout_ms, "default_timeout_ms");
    TF_READ_NUM(root, wf.default_max_retries, "default_max_retries");
    {
        cJSON *es_item = cJSON_GetObjectItem(root, "default_error_strategy");
        if (es_item && cJSON_IsString(es_item) && es_item->valuestring) {
            wf.default_error_strategy = taskflow_parse_error_strategy(es_item->valuestring);
        }
    }

    /* nodes 数组 */
    {
        cJSON *nodes_arr = cJSON_GetObjectItem(root, "nodes");
        if (nodes_arr && cJSON_IsArray(nodes_arr)) {
            int n_count = cJSON_GetArraySize(nodes_arr);
            if (n_count < 0 || (size_t)n_count > TASKFLOW_MAX_NODES) {
                cJSON_Delete(root);
                taskflow_free_workflow_fields(&wf);
                AGENTOS_LOG_WARN("taskflow: load_workflow_json nodes count overflow (%d)", n_count);
                ATM_RET_ERR(AGENTOS_ERR_OVERFLOW);
            }
            if (n_count > 0) {
                wf.nodes = (taskflow_node_t *)AGENTOS_CALLOC((size_t)n_count, sizeof(taskflow_node_t));
                if (!wf.nodes) {
                    cJSON_Delete(root);
                    taskflow_free_workflow_fields(&wf);
                    ATM_RET_ERR(AGENTOS_ERR_OUT_OF_MEMORY);
                }
                wf.node_count = (size_t)n_count;
                for (int i = 0; i < n_count; i++) {
                    cJSON *node_obj = cJSON_GetArrayItem(nodes_arr, i);
                    taskflow_node_t *n = &wf.nodes[i];
                    n->state = TASKFLOW_STATE_PENDING;  /* 默认状态：等待中 */
                    n->type = TASKFLOW_NODE_TASK;       /* 默认类型 */

                    TF_READ_STR_FIELD(node_obj, n->id, "id", sizeof(n->id));
                    TF_READ_STR_FIELD(node_obj, n->name, "name", sizeof(n->name));
                    TF_READ_STR_DUP(node_obj, n->task_handler_name, "task_handler_name");
                    TF_READ_STR_DUP(node_obj, n->config_json, "config_json");
                    TF_READ_STR_DUP(node_obj, n->input_transform_json, "input_transform_json");
                    TF_READ_STR_DUP(node_obj, n->output_transform_json, "output_transform_json");
                    TF_READ_STR_DUP(node_obj, n->fallback_handler_name, "fallback_handler_name");
                    TF_READ_STR_DUP(node_obj, n->condition_expr, "condition_expr");
                    TF_READ_STR_DUP(node_obj, n->subflow_id, "subflow_id");
                    TF_READ_NUM(node_obj, n->timeout_ms, "timeout_ms");
                    TF_READ_NUM(node_obj, n->max_retries, "max_retries");
                    TF_READ_NUM(node_obj, n->retry_delay_ms, "retry_delay_ms");

                    cJSON *ntype = cJSON_GetObjectItem(node_obj, "type");
                    if (ntype && cJSON_IsString(ntype) && ntype->valuestring) {
                        n->type = taskflow_parse_node_type(ntype->valuestring);
                    }
                    cJSON *nerr = cJSON_GetObjectItem(node_obj, "error_strategy");
                    if (nerr && cJSON_IsString(nerr) && nerr->valuestring) {
                        n->error_strategy = taskflow_parse_error_strategy(nerr->valuestring);
                    }
                }
            }
        }
    }

    /* edges 数组 */
    {
        cJSON *edges_arr = cJSON_GetObjectItem(root, "edges");
        if (edges_arr && cJSON_IsArray(edges_arr)) {
            int e_count = cJSON_GetArraySize(edges_arr);
            if (e_count < 0 || (size_t)e_count > TASKFLOW_MAX_EDGES) {
                cJSON_Delete(root);
                taskflow_free_workflow_fields(&wf);
                AGENTOS_LOG_WARN("taskflow: load_workflow_json edges count overflow (%d)", e_count);
                ATM_RET_ERR(AGENTOS_ERR_OVERFLOW);
            }
            if (e_count > 0) {
                wf.edges = (taskflow_edge_t *)AGENTOS_CALLOC((size_t)e_count, sizeof(taskflow_edge_t));
                if (!wf.edges) {
                    cJSON_Delete(root);
                    taskflow_free_workflow_fields(&wf);
                    ATM_RET_ERR(AGENTOS_ERR_OUT_OF_MEMORY);
                }
                wf.edge_count = (size_t)e_count;
                for (int i = 0; i < e_count; i++) {
                    cJSON *edge_obj = cJSON_GetArrayItem(edges_arr, i);
                    taskflow_edge_t *e = &wf.edges[i];
                    e->priority = 0;
                    e->is_default = false;

                    TF_READ_STR_FIELD(edge_obj, e->id, "id", sizeof(e->id));
                    TF_READ_STR_FIELD(edge_obj, e->source_node_id, "source_node_id", sizeof(e->source_node_id));
                    TF_READ_STR_FIELD(edge_obj, e->target_node_id, "target_node_id", sizeof(e->target_node_id));
                    TF_READ_STR_FIELD(edge_obj, e->condition_expr, "condition_expr", sizeof(e->condition_expr));
                    TF_READ_NUM(edge_obj, e->priority, "priority");
                    {
                        cJSON *def_item = cJSON_GetObjectItem(edge_obj, "is_default");
                        if (def_item && cJSON_IsBool(def_item)) {
                            e->is_default = cJSON_IsTrue(def_item);
                        }
                    }
                }
            }
        }
    }

    #undef TF_READ_STR_FIELD
    #undef TF_READ_STR_DUP
    #undef TF_READ_NUM

    cJSON_Delete(root);

    /* 验证必需字段：id 不能为空 */
    if (wf.id[0] == '\0') {
        taskflow_free_workflow_fields(&wf);
        AGENTOS_LOG_WARN("taskflow: load_workflow_json missing required field 'id'");
        ATM_RET_ERR(AGENTOS_EINVAL);
    }

    /* 通过 register_workflow 注册（浅拷贝，所有权转移给 engine）。
     * register_workflow 失败时需释放 wf 资源避免泄漏。 */
    int rc = taskflow_engine_register_workflow(engine, &wf);
    if (rc != 0) {
        taskflow_free_workflow_fields(&wf);
        AGENTOS_LOG_WARN("taskflow: load_workflow_json register_workflow failed (rc=%d)", rc);
        return rc;
    }

    AGENTOS_LOG_INFO("taskflow: workflow '%s' loaded from JSON (nodes=%zu edges=%zu)",
                     wf.id, wf.node_count, wf.edge_count);
    return 0;
#endif /* AGENTOS_TASKFLOW_HAVE_CJSON */
}

static taskflow_workflow_t *find_workflow(taskflow_engine_t *engine, const char *workflow_id)
{
    for (size_t i = 0; i < engine->workflow_count; i++) {
        if (strcmp(engine->workflows[i].workflow.id, workflow_id) == 0)
            return &engine->workflows[i].workflow;
    }
    AGENTOS_ERROR_NULL(AGENTOS_ERR_OVERFLOW, "limit exceeded");
}

static handler_entry_t *find_handler(taskflow_engine_t *engine, const char *name)
{
    if (!name) return NULL;
    for (size_t i = 0; i < engine->handler_count; i++) {
        if (strcmp(engine->handlers[i].name, name) == 0)
            return &engine->handlers[i];
    }
    AGENTOS_ERROR_NULL(AGENTOS_ERR_OVERFLOW, "limit exceeded");
}

int taskflow_engine_start(taskflow_engine_t *engine, const char *workflow_id,
                          const char *input_json, char **execution_id)
{
    /* P3.21/W13.1 (ACC-DT25)：记录 handler 返回码，避免返回码恒 SUCCESS。
     * 原代码无论 handler 成功失败都返回 0，调用方无法通过返回码感知执行失败，
     * 违反 BAN-334（禁止 taskflow_advanced 调用方未检查返回码）。
     * 修复：handler 失败时返回其错误码，调用方可通过返回码感知执行失败。 */
    int handler_ret = 0;
    if (!engine || !workflow_id || !execution_id)
        AGENTOS_ERROR(AGENTOS_EINVAL, "failed to start execution: null engine, workflow_id, or execution_id");
    taskflow_workflow_t *wf = find_workflow(engine, workflow_id);
    if (!wf)
        ATM_RET_ERR(AGENTOS_ERR_INVALID_PARAM);
    if (engine->execution_count >= TASKFLOW_MAX_PARALLEL)
        ATM_RET_ERR(AGENTOS_ERR_INVALID_PARAM);

    execution_entry_t *ee = &engine->executions[engine->execution_count];
    __builtin_memset(ee, 0, sizeof(execution_entry_t));
    uint64_t eid = generate_id(engine);
    snprintf(ee->execution.execution_id, sizeof(ee->execution.execution_id), "exec_%lu",
             (unsigned long)eid);
AGENTOS_STRNCPY_TERM(ee->execution.workflow_id, workflow_id, sizeof(ee->execution.workflow_id));
    ee->execution.state = TASKFLOW_STATE_RUNNING;
    ee->execution.input_json = input_json ? AGENTOS_STRDUP(input_json) : NULL;
    ee->execution.progress = 0.0;
    ee->execution.superstep = 0;
    ee->execution.completed_nodes = 0;
    ee->execution.total_nodes = wf->node_count;
    ee->execution.started_at = (uint64_t)(time_t)(agentos_time_ms() / 1000ULL);
    ee->active = 1;
    ee->checkpoint_count = 0;
    ee->variables_json = AGENTOS_STRDUP("{}");

    if (wf->initial_node_id) {
        ee->execution.current_node_id = AGENTOS_STRDUP(wf->initial_node_id);
    } else if (wf->node_count > 0) {
        ee->execution.current_node_id = AGENTOS_STRDUP(wf->nodes[0].id);
    }

    if (execution_id) {
        *execution_id = AGENTOS_STRDUP(ee->execution.execution_id);
    }

    engine->execution_count++;

    if (wf->initial_node_id && wf->node_count > 0) {
        taskflow_node_t *start_node = NULL;
        for (size_t i = 0; i < wf->node_count; i++) {
            if (strcmp(wf->nodes[i].id, wf->initial_node_id) == 0) {
                start_node = &wf->nodes[i];
                break;
            }
        }
        if (start_node && start_node->task_handler_name) {
            handler_entry_t *h = find_handler(engine, start_node->task_handler_name);
            if (h && h->handler) {
                char *output = NULL;
                int ret = h->handler(engine, start_node->id, input_json ? input_json : "", &output,
                                     h->user_data);
                if (ret == 0) {
                    ee->execution.output_json = output;
                    ee->execution.completed_nodes = 1;
                    ee->execution.progress =
                        wf->node_count > 0
                            ? (double)ee->execution.completed_nodes / (double)wf->node_count
                            : 1.0;
                } else {
                    /* P3.21/W13.1：记录 handler 失败码，函数返回时反映真实结果 */
                    handler_ret = ret;
                    ee->execution.state = TASKFLOW_STATE_FAILED;
                    ee->execution.error_message = (char *)AGENTOS_MALLOC(128);
                    if (ee->execution.error_message)
                        snprintf(ee->execution.error_message, 128, "Handler returned %d", ret);
                    if (output)
                        AGENTOS_FREE(output);
                }
            }
        }
    }

    if (ee->execution.completed_nodes >= ee->execution.total_nodes &&
        ee->execution.state == TASKFLOW_STATE_RUNNING) {
        ee->execution.state = TASKFLOW_STATE_COMPLETED;
        ee->execution.completed_at = (uint64_t)(time_t)(agentos_time_ms() / 1000ULL);
        ee->execution.progress = 1.0;
    }

    if (engine->progress_cb) {
        engine->progress_cb(ee->execution.execution_id, ee->execution.current_node_id,
                            ee->execution.state, ee->execution.progress,
                            engine->progress_user_data);
    }

    /* P3.21/W13.1：返回 handler 真实结果，而非总返回 SUCCESS。
     * handler 成功时 handler_ret=0；handler 失败时 handler_ret=ret（非 0）。
     * 调用方可通过返回码感知执行失败，符合 BAN-334 要求。 */
    return handler_ret;
}

int taskflow_engine_cancel(taskflow_engine_t *engine, const char *execution_id)
{
    if (!engine || !execution_id)
        ATM_RET_ERR(AGENTOS_EINVAL);
    for (size_t i = 0; i < engine->execution_count; i++) {
        if (strcmp(engine->executions[i].execution.execution_id, execution_id) == 0) {
            engine->executions[i].execution.state = TASKFLOW_STATE_CANCELED;
            engine->executions[i].execution.completed_at =
                (uint64_t)(time_t)(agentos_time_ms() / 1000ULL);
            return 0;
        }
    }
    return 1;
}

int taskflow_engine_pause(taskflow_engine_t *engine, const char *execution_id)
{
    if (!engine || !execution_id)
        ATM_RET_ERR(AGENTOS_EINVAL);
    for (size_t i = 0; i < engine->execution_count; i++) {
        if (strcmp(engine->executions[i].execution.execution_id, execution_id) == 0) {
            if (engine->executions[i].execution.state != TASKFLOW_STATE_RUNNING)
                ATM_RET_ERR(AGENTOS_ERR_IO);
            engine->executions[i].execution.state = TASKFLOW_STATE_WAITING;
            return 0;
        }
    }
    return 1;
}

int taskflow_engine_resume(taskflow_engine_t *engine, const char *execution_id)
{
    if (!engine || !execution_id)
        ATM_RET_ERR(AGENTOS_EINVAL);
    for (size_t i = 0; i < engine->execution_count; i++) {
        if (strcmp(engine->executions[i].execution.execution_id, execution_id) == 0) {
            if (engine->executions[i].execution.state != TASKFLOW_STATE_WAITING)
                ATM_RET_ERR(AGENTOS_ERR_IO);
            engine->executions[i].execution.state = TASKFLOW_STATE_RUNNING;
            return 0;
        }
    }
    return 1;
}

int taskflow_engine_get_execution(taskflow_engine_t *engine, const char *execution_id,
                                  taskflow_execution_t **execution)
{
    if (!engine || !execution_id || !execution)
        AGENTOS_ERROR(AGENTOS_EINVAL, "failed to get execution: null engine, execution_id, or execution");
    for (size_t i = 0; i < engine->execution_count; i++) {
        if (strcmp(engine->executions[i].execution.execution_id, execution_id) == 0) {
            *execution = &engine->executions[i].execution;
            return 0;
        }
    }
    return 1;
}

int taskflow_engine_step(taskflow_engine_t *engine, const char *execution_id)
{
    if (!engine || !execution_id)
        ATM_RET_ERR(AGENTOS_EINVAL);
    for (size_t i = 0; i < engine->execution_count; i++) {
        execution_entry_t *ee = &engine->executions[i];
        if (strcmp(ee->execution.execution_id, execution_id) != 0)
            continue;
        if (ee->execution.state != TASKFLOW_STATE_RUNNING)
            ATM_RET_ERR(AGENTOS_ERR_IO);

        taskflow_workflow_t *wf = find_workflow(engine, ee->execution.workflow_id);
        if (!wf)
            ATM_RET_ERR(AGENTOS_ERR_NULL_POINTER);

        ee->execution.superstep++;
        if (ee->execution.completed_nodes < wf->node_count) {
            ee->execution.completed_nodes++;
            ee->execution.progress = (double)ee->execution.completed_nodes / (double)wf->node_count;
            if (ee->execution.completed_nodes >= wf->node_count) {
                ee->execution.state = TASKFLOW_STATE_COMPLETED;
                ee->execution.completed_at = (uint64_t)(time_t)(agentos_time_ms() / 1000ULL);
                ee->execution.progress = 1.0;
            }
        }
        return 0;
    }
    return 1;
}

int taskflow_engine_run_to_completion(taskflow_engine_t *engine, const char *execution_id)
{
    if (!engine || !execution_id)
        ATM_RET_ERR(AGENTOS_EINVAL);
    for (size_t i = 0; i < engine->execution_count; i++) {
        execution_entry_t *ee = &engine->executions[i];
        if (strcmp(ee->execution.execution_id, execution_id) != 0)
            continue;
        if (ee->execution.state != TASKFLOW_STATE_RUNNING)
            ATM_RET_ERR(AGENTOS_ERR_IO);

        taskflow_workflow_t *wf = find_workflow(engine, ee->execution.workflow_id);
        if (!wf)
            ATM_RET_ERR(AGENTOS_ERR_NULL_POINTER);

        ee->execution.completed_nodes = wf->node_count;
        ee->execution.progress = 1.0;
        ee->execution.state = TASKFLOW_STATE_COMPLETED;
        ee->execution.completed_at = (uint64_t)(time_t)(agentos_time_ms() / 1000ULL);

        if (engine->progress_cb) {
            engine->progress_cb(ee->execution.execution_id, NULL, TASKFLOW_STATE_COMPLETED, 1.0,
                                engine->progress_user_data);
        }
        return 0;
    }
    return 1;
}

int taskflow_engine_create_checkpoint(taskflow_engine_t *engine, const char *execution_id,
                                      char **checkpoint_id)
{
    if (!engine || !execution_id)
        AGENTOS_ERROR(AGENTOS_EINVAL, "failed to create checkpoint: null engine or execution_id");
    for (size_t i = 0; i < engine->execution_count; i++) {
        execution_entry_t *ee = &engine->executions[i];
        if (strcmp(ee->execution.execution_id, execution_id) != 0)
            continue;
        if (ee->checkpoint_count >= TASKFLOW_MAX_CHECKPOINTS)
            ATM_RET_ERR(AGENTOS_ERR_OVERFLOW);

        taskflow_checkpoint_t *cp = &ee->checkpoints[ee->checkpoint_count];
        uint64_t cid = generate_id(engine);
        snprintf(cp->id, sizeof(cp->id), "cp_%lu", (unsigned long)cid);
AGENTOS_STRNCPY_TERM(cp->execution_id, execution_id, sizeof(cp->execution_id));
AGENTOS_STRNCPY_TERM(cp->workflow_id, ee->execution.workflow_id, sizeof(cp->workflow_id));
        if (ee->execution.current_node_id)
AGENTOS_STRNCPY_TERM(cp->node_id, ee->execution.current_node_id, sizeof(cp->node_id));
        cp->state = ee->execution.state;
        size_t snap_len = 256 + (ee->variables_json ? strlen(ee->variables_json) : 0);
        cp->snapshot_json = (char *)AGENTOS_MALLOC(snap_len);
        if (cp->snapshot_json) {
            snprintf(
                cp->snapshot_json, snap_len,
                "{\"superstep\":%d,\"progress\":%.2f,\"completed_nodes\":%zu,\"variables\":%s}",
                ee->execution.superstep, ee->execution.progress, ee->execution.completed_nodes,
                ee->variables_json ? ee->variables_json : "{}");
        }
        cp->timestamp = (uint64_t)(time_t)(agentos_time_ms() / 1000ULL);

        if (checkpoint_id) {
            *checkpoint_id = AGENTOS_STRDUP(cp->id);
        }
        ee->checkpoint_count++;
        return 0;
    }
    return 1;
}

int taskflow_engine_restore_checkpoint(taskflow_engine_t *engine, const char *checkpoint_id)
{
    if (!engine || !checkpoint_id)
        ATM_RET_ERR(AGENTOS_EINVAL);
    for (size_t i = 0; i < engine->execution_count; i++) {
        execution_entry_t *ee = &engine->executions[i];
        for (size_t j = 0; j < ee->checkpoint_count; j++) {
            if (strcmp(ee->checkpoints[j].id, checkpoint_id) == 0) {
                ee->execution.state = ee->checkpoints[j].state;
                if (ee->execution.current_node_id)
                    AGENTOS_FREE(ee->execution.current_node_id);
                ee->execution.current_node_id = AGENTOS_STRDUP(ee->checkpoints[j].node_id);
                ee->execution.state = TASKFLOW_STATE_RUNNING;
                return 0;
            }
        }
    }
    return 1;
}

int taskflow_engine_list_checkpoints(taskflow_engine_t *engine, const char *execution_id,
                                     taskflow_checkpoint_t **checkpoints, size_t *count)
{
    if (!engine || !execution_id || !checkpoints || !count)
        AGENTOS_ERROR(AGENTOS_EINVAL, "failed to list checkpoints: null engine, execution_id, checkpoints, or count");
    for (size_t i = 0; i < engine->execution_count; i++) {
        execution_entry_t *ee = &engine->executions[i];
        if (strcmp(ee->execution.execution_id, execution_id) != 0)
            continue;
        *checkpoints = ee->checkpoints;
        *count = ee->checkpoint_count;
        return 0;
    }
    *count = 0;
    return 1;
}

int taskflow_engine_set_condition_fn(taskflow_engine_t *engine, taskflow_condition_fn fn,
                                     void *user_data)
{
    if (!engine)
        ATM_RET_ERR(AGENTOS_EINVAL);
    engine->condition_fn = fn;
    engine->condition_user_data = user_data;
    return 0;
}

int taskflow_engine_set_progress_callback(taskflow_engine_t *engine,
                                          taskflow_progress_callback_t callback, void *user_data)
{
    if (!engine)
        ATM_RET_ERR(AGENTOS_EINVAL);
    engine->progress_cb = callback;
    engine->progress_user_data = user_data;
    return 0;
}

int taskflow_engine_set_event_callback(taskflow_engine_t *engine,
                                       taskflow_event_callback_t callback, void *user_data)
{
    if (!engine)
        ATM_RET_ERR(AGENTOS_EINVAL);
    engine->event_cb = callback;
    engine->event_user_data = user_data;
    return 0;
}

int taskflow_engine_notify_event(taskflow_engine_t *engine, const char *execution_id,
                                 const char *event_type, const char *data_json)
{
    if (!engine || !event_type)
        ATM_RET_ERR(AGENTOS_EINVAL);
    if (engine->event_cb) {
        engine->event_cb(execution_id, event_type, data_json, engine->event_user_data);
    }
    return 0;
}

int taskflow_engine_set_variable(taskflow_engine_t *engine, const char *execution_id,
                                 const char *key, const char *value_json)
{
    if (!engine || !execution_id || !key || !value_json)
        AGENTOS_ERROR(AGENTOS_EINVAL, "failed to set variable: null engine, execution_id, key, or value_json");
    for (size_t i = 0; i < engine->execution_count; i++) {
        execution_entry_t *ee = &engine->executions[i];
        if (strcmp(ee->execution.execution_id, execution_id) != 0)
            continue;
        size_t old_len = ee->variables_json ? strlen(ee->variables_json) : 2;
        size_t new_len = old_len + strlen(key) + strlen(value_json) + 32;
        char *new_vars = (char *)AGENTOS_MALLOC(new_len);
        if (!new_vars)
            ATM_RET_ERR(AGENTOS_ERR_OUT_OF_MEMORY);
        if (old_len <= 2) {
            snprintf(new_vars, new_len, "{\"%s\":%s}", key, value_json);
        } else {
            snprintf(new_vars, new_len, "{\"%s\":%s,\"_prev\":%s}", key, value_json,
                     ee->variables_json ? ee->variables_json : "{}");
        }
        if (ee->variables_json)
            AGENTOS_FREE(ee->variables_json);
        ee->variables_json = new_vars;
        return 0;
    }
    return 1;
}

int taskflow_engine_get_variable(taskflow_engine_t *engine, const char *execution_id,
                                 const char *key, char **value_json)
{
    if (!engine || !execution_id || !key || !value_json)
        ATM_RET_ERR(AGENTOS_EINVAL);
    *value_json = NULL;
    for (size_t i = 0; i < engine->execution_count; i++) {
        execution_entry_t *ee = &engine->executions[i];
        if (strcmp(ee->execution.execution_id, execution_id) != 0)
            continue;
        if (!ee->variables_json)
            return 1;
        char search_key[128];
        snprintf(search_key, sizeof(search_key), "\"%s\":", key);
        const char *found = strstr(ee->variables_json, search_key);
        if (!found)
            return 1;
        found += strlen(search_key);
        while (*found == ' ')
            found++;
        const char *end = found;
        if (*end == '"') {
            end++;
            while (*end && *end != '"')
                end++;
            if (*end)
                end++;
        } else if (*end == '{' || *end == '[') {
            int depth = 1;
            end++;
            while (*end && depth > 0) {
                if (*end == '{' || *end == '[')
                    depth++;
                else if (*end == '}' || *end == ']')
                    depth--;
                end++;
            }
        } else {
            while (*end && *end != ',' && *end != '}')
                end++;
        }
        size_t val_len = (size_t)(end - found);
        *value_json = (char *)AGENTOS_MALLOC(val_len + 1);
        if (*value_json) {
            __builtin_memcpy(*value_json, found, val_len);
            (*value_json)[val_len] = '\0';
        }
        return 0;
    }
    return 1;
}

size_t taskflow_engine_get_workflow_count(taskflow_engine_t *engine)
{
    return engine ? engine->workflow_count : 0;
}

size_t taskflow_engine_get_execution_count(taskflow_engine_t *engine)
{
    return engine ? engine->execution_count : 0;
}

void taskflow_workflow_destroy(taskflow_workflow_t *workflow)
{
    if (!workflow)
        return;
    if (workflow->nodes) {
        for (size_t i = 0; i < workflow->node_count; i++) {
            AGENTOS_FREE(workflow->nodes[i].task_handler_name);
            AGENTOS_FREE(workflow->nodes[i].config_json);
            AGENTOS_FREE(workflow->nodes[i].input_transform_json);
            AGENTOS_FREE(workflow->nodes[i].output_transform_json);
            AGENTOS_FREE(workflow->nodes[i].fallback_handler_name);
            AGENTOS_FREE(workflow->nodes[i].condition_expr);
            AGENTOS_FREE(workflow->nodes[i].subflow_id);
            AGENTOS_FREE(workflow->nodes[i].loop_condition_expr);
            AGENTOS_FREE(workflow->nodes[i].loop_foreach_json);
            AGENTOS_FREE(workflow->nodes[i].event_type);
        }
        AGENTOS_FREE(workflow->nodes);
    }
    AGENTOS_FREE(workflow->edges);
    AGENTOS_FREE(workflow->initial_node_id);
    AGENTOS_FREE(workflow->input_schema_json);
    AGENTOS_FREE(workflow->output_schema_json);
}

void taskflow_execution_destroy(taskflow_execution_t *execution)
{
    if (!execution)
        return;
    if (execution->current_node_id)
        AGENTOS_FREE(execution->current_node_id);
    if (execution->input_json)
        AGENTOS_FREE(execution->input_json);
    if (execution->output_json)
        AGENTOS_FREE(execution->output_json);
    if (execution->error_message)
        AGENTOS_FREE(execution->error_message);
    if (execution->variables_json)
        AGENTOS_FREE(execution->variables_json);
}

void taskflow_checkpoint_destroy(taskflow_checkpoint_t *checkpoint)
{
    if (!checkpoint)
        return;
    if (checkpoint->snapshot_json)
        AGENTOS_FREE(checkpoint->snapshot_json);
}
