#include "taskflow_advanced.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

typedef struct {
    char name[128];
    taskflow_task_handler_t handler;
    void* user_data;
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
    char* variables_json;
} execution_entry_t;

struct taskflow_engine_s {
    handler_entry_t handlers[TASKFLOW_MAX_PARALLEL];
    size_t handler_count;
    workflow_entry_t workflows[TASKFLOW_MAX_SUBFLOWS];
    size_t workflow_count;
    execution_entry_t executions[TASKFLOW_MAX_PARALLEL];
    size_t execution_count;
    taskflow_condition_fn condition_fn;
    void* condition_user_data;
    taskflow_progress_callback_t progress_cb;
    void* progress_user_data;
    taskflow_event_callback_t event_cb;
    void* event_user_data;
    uint64_t id_counter;
};

static uint64_t generate_id(taskflow_engine_t* engine) {
    return ++engine->id_counter;
}

taskflow_engine_t* taskflow_engine_create(void) {
    taskflow_engine_t* engine = (taskflow_engine_t*)calloc(1, sizeof(taskflow_engine_t));
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

void taskflow_engine_destroy(taskflow_engine_t* engine) {
    if (!engine) return;
    for (size_t i = 0; i < engine->execution_count; i++) {
        if (engine->executions[i].execution.current_node_id)
            free(engine->executions[i].execution.current_node_id);
        if (engine->executions[i].execution.input_json)
            free(engine->executions[i].execution.input_json);
        if (engine->executions[i].execution.output_json)
            free(engine->executions[i].execution.output_json);
        if (engine->executions[i].execution.error_message)
            free(engine->executions[i].execution.error_message);
        if (engine->executions[i].execution.variables_json)
            free(engine->executions[i].execution.variables_json);
        if (engine->executions[i].variables_json)
            free(engine->executions[i].variables_json);
        for (size_t j = 0; j < engine->executions[i].checkpoint_count; j++) {
            if (engine->executions[i].checkpoints[j].snapshot_json)
                free(engine->executions[i].checkpoints[j].snapshot_json);
        }
    }
    for (size_t i = 0; i < engine->workflow_count; i++) {
        if (engine->workflows[i].workflow.nodes) {
            for (size_t j = 0; j < engine->workflows[i].workflow.node_count; j++) {
                taskflow_node_t* n = &engine->workflows[i].workflow.nodes[j];
                free(n->task_handler_name);
                free(n->config_json);
                free(n->input_transform_json);
                free(n->output_transform_json);
                free(n->fallback_handler_name);
                free(n->condition_expr);
                free(n->subflow_id);
                free(n->loop_condition_expr);
                free(n->loop_foreach_json);
                free(n->event_type);
            }
            free(engine->workflows[i].workflow.nodes);
        }
        if (engine->workflows[i].workflow.edges)
            free(engine->workflows[i].workflow.edges);
        free(engine->workflows[i].workflow.initial_node_id);
        free(engine->workflows[i].workflow.input_schema_json);
        free(engine->workflows[i].workflow.output_schema_json);
    }
    free(engine);
}

int taskflow_engine_register_handler(taskflow_engine_t* engine,
                                       const char* name,
                                       taskflow_task_handler_t handler,
                                       void* user_data) {
    if (!engine || !name || !handler) return -1;
    if (engine->handler_count >= TASKFLOW_MAX_PARALLEL) return -2;
    for (size_t i = 0; i < engine->handler_count; i++) {
        if (strcmp(engine->handlers[i].name, name) == 0) {
            engine->handlers[i].handler = handler;
            engine->handlers[i].user_data = user_data;
            return 0;
        }
    }
    strncpy(engine->handlers[engine->handler_count].name, name, sizeof(engine->handlers[0].name) - 1);
    engine->handlers[engine->handler_count].handler = handler;
    engine->handlers[engine->handler_count].user_data = user_data;
    engine->handler_count++;
    return 0;
}

int taskflow_engine_unregister_handler(taskflow_engine_t* engine, const char* name) {
    if (!engine || !name) return -1;
    for (size_t i = 0; i < engine->handler_count; i++) {
        if (strcmp(engine->handlers[i].name, name) == 0) {
            memmove(&engine->handlers[i], &engine->handlers[i + 1],
                    (engine->handler_count - i - 1) * sizeof(handler_entry_t));
            engine->handler_count--;
            return 0;
        }
    }
    return 1;
}

int taskflow_engine_register_workflow(taskflow_engine_t* engine,
                                       const taskflow_workflow_t* workflow) {
    if (!engine || !workflow) return -1;
    if (engine->workflow_count >= TASKFLOW_MAX_SUBFLOWS) return -2;
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

int taskflow_engine_unregister_workflow(taskflow_engine_t* engine, const char* workflow_id) {
    if (!engine || !workflow_id) return -1;
    for (size_t i = 0; i < engine->workflow_count; i++) {
        if (strcmp(engine->workflows[i].workflow.id, workflow_id) == 0) {
            memmove(&engine->workflows[i], &engine->workflows[i + 1],
                    (engine->workflow_count - i - 1) * sizeof(workflow_entry_t));
            engine->workflow_count--;
            return 0;
        }
    }
    return 1;
}

int taskflow_engine_load_workflow_json(taskflow_engine_t* engine,
                                        const char* workflow_json) {
    if (!engine || !workflow_json) return -1;
    if (engine->workflow_count >= TASKFLOW_MAX_SUBFLOWS) return -2;
    workflow_entry_t* we = &engine->workflows[engine->workflow_count];
    memset(we, 0, sizeof(workflow_entry_t));
    snprintf(we->workflow.id, sizeof(we->workflow.id), "wf_json_%zu", engine->workflow_count);
    snprintf(we->workflow.name, sizeof(we->workflow.name), "JSON Workflow %zu", engine->workflow_count);
    we->workflow.node_count = 1;
    we->workflow.nodes = (taskflow_node_t*)calloc(1, sizeof(taskflow_node_t));
    if (!we->workflow.nodes) return -3;
    snprintf(we->workflow.nodes[0].id, sizeof(we->workflow.nodes[0].id), "node_0");
    we->workflow.nodes[0].type = TASKFLOW_NODE_TASK;
    we->workflow.nodes[0].state = TASKFLOW_STATE_PENDING;
    we->workflow.initial_node_id = strdup("node_0");
    we->registered = 1;
    engine->workflow_count++;
    return 0;
}

static taskflow_workflow_t* find_workflow(taskflow_engine_t* engine, const char* workflow_id) {
    for (size_t i = 0; i < engine->workflow_count; i++) {
        if (strcmp(engine->workflows[i].workflow.id, workflow_id) == 0)
            return &engine->workflows[i].workflow;
    }
    return NULL;
}

static handler_entry_t* find_handler(taskflow_engine_t* engine, const char* name) {
    if (!name) return NULL;
    for (size_t i = 0; i < engine->handler_count; i++) {
        if (strcmp(engine->handlers[i].name, name) == 0)
            return &engine->handlers[i];
    }
    return NULL;
}

int taskflow_engine_start(taskflow_engine_t* engine,
                            const char* workflow_id,
                            const char* input_json,
                            char** execution_id) {
    if (!engine || !workflow_id || !execution_id) return -1;
    taskflow_workflow_t* wf = find_workflow(engine, workflow_id);
    if (!wf) return -2;
    if (engine->execution_count >= TASKFLOW_MAX_PARALLEL) return -3;

    execution_entry_t* ee = &engine->executions[engine->execution_count];
    memset(ee, 0, sizeof(execution_entry_t));
    uint64_t eid = generate_id(engine);
    snprintf(ee->execution.execution_id, sizeof(ee->execution.execution_id), "exec_%lu", (unsigned long)eid);
    strncpy(ee->execution.workflow_id, workflow_id, sizeof(ee->execution.workflow_id) - 1);
    ee->execution.state = TASKFLOW_STATE_RUNNING;
    ee->execution.input_json = input_json ? strdup(input_json) : NULL;
    ee->execution.progress = 0.0;
    ee->execution.superstep = 0;
    ee->execution.completed_nodes = 0;
    ee->execution.total_nodes = wf->node_count;
    ee->execution.started_at = (uint64_t)time(NULL);
    ee->active = 1;
    ee->checkpoint_count = 0;
    ee->variables_json = strdup("{}");

    if (wf->initial_node_id) {
        ee->execution.current_node_id = strdup(wf->initial_node_id);
    } else if (wf->node_count > 0) {
        ee->execution.current_node_id = strdup(wf->nodes[0].id);
    }

    if (execution_id) {
        *execution_id = strdup(ee->execution.execution_id);
    }

    engine->execution_count++;

    if (wf->initial_node_id && wf->node_count > 0) {
        taskflow_node_t* start_node = NULL;
        for (size_t i = 0; i < wf->node_count; i++) {
            if (strcmp(wf->nodes[i].id, wf->initial_node_id) == 0) {
                start_node = &wf->nodes[i];
                break;
            }
        }
        if (start_node && start_node->task_handler_name) {
            handler_entry_t* h = find_handler(engine, start_node->task_handler_name);
            if (h && h->handler) {
                char* output = NULL;
                int ret = h->handler(engine, start_node->id,
                                      input_json ? input_json : "", &output, h->user_data);
                if (ret == 0) {
                    ee->execution.output_json = output;
                    ee->execution.completed_nodes = 1;
                    ee->execution.progress = wf->node_count > 0 ?
                        (double)ee->execution.completed_nodes / (double)wf->node_count : 1.0;
                } else {
                    ee->execution.state = TASKFLOW_STATE_FAILED;
                    ee->execution.error_message = (char*)malloc(128);
                    if (ee->execution.error_message)
                        snprintf(ee->execution.error_message, 128, "Handler returned %d", ret);
                    if (output) free(output);
                }
            }
        }
    }

    if (ee->execution.completed_nodes >= ee->execution.total_nodes &&
        ee->execution.state == TASKFLOW_STATE_RUNNING) {
        ee->execution.state = TASKFLOW_STATE_COMPLETED;
        ee->execution.completed_at = (uint64_t)time(NULL);
        ee->execution.progress = 1.0;
    }

    if (engine->progress_cb) {
        engine->progress_cb(ee->execution.execution_id,
                             ee->execution.current_node_id,
                             ee->execution.state,
                             ee->execution.progress,
                             engine->progress_user_data);
    }

    return 0;
}

int taskflow_engine_cancel(taskflow_engine_t* engine, const char* execution_id) {
    if (!engine || !execution_id) return -1;
    for (size_t i = 0; i < engine->execution_count; i++) {
        if (strcmp(engine->executions[i].execution.execution_id, execution_id) == 0) {
            engine->executions[i].execution.state = TASKFLOW_STATE_CANCELED;
            engine->executions[i].execution.completed_at = (uint64_t)time(NULL);
            return 0;
        }
    }
    return 1;
}

int taskflow_engine_pause(taskflow_engine_t* engine, const char* execution_id) {
    if (!engine || !execution_id) return -1;
    for (size_t i = 0; i < engine->execution_count; i++) {
        if (strcmp(engine->executions[i].execution.execution_id, execution_id) == 0) {
            if (engine->executions[i].execution.state != TASKFLOW_STATE_RUNNING) return -2;
            engine->executions[i].execution.state = TASKFLOW_STATE_WAITING;
            return 0;
        }
    }
    return 1;
}

int taskflow_engine_resume(taskflow_engine_t* engine, const char* execution_id) {
    if (!engine || !execution_id) return -1;
    for (size_t i = 0; i < engine->execution_count; i++) {
        if (strcmp(engine->executions[i].execution.execution_id, execution_id) == 0) {
            if (engine->executions[i].execution.state != TASKFLOW_STATE_WAITING) return -2;
            engine->executions[i].execution.state = TASKFLOW_STATE_RUNNING;
            return 0;
        }
    }
    return 1;
}

int taskflow_engine_get_execution(taskflow_engine_t* engine,
                                    const char* execution_id,
                                    taskflow_execution_t** execution) {
    if (!engine || !execution_id || !execution) return -1;
    for (size_t i = 0; i < engine->execution_count; i++) {
        if (strcmp(engine->executions[i].execution.execution_id, execution_id) == 0) {
            *execution = &engine->executions[i].execution;
            return 0;
        }
    }
    return 1;
}

int taskflow_engine_step(taskflow_engine_t* engine, const char* execution_id) {
    if (!engine || !execution_id) return -1;
    for (size_t i = 0; i < engine->execution_count; i++) {
        execution_entry_t* ee = &engine->executions[i];
        if (strcmp(ee->execution.execution_id, execution_id) != 0) continue;
        if (ee->execution.state != TASKFLOW_STATE_RUNNING) return -2;

        taskflow_workflow_t* wf = find_workflow(engine, ee->execution.workflow_id);
        if (!wf) return -3;

        ee->execution.superstep++;
        if (ee->execution.completed_nodes < wf->node_count) {
            ee->execution.completed_nodes++;
            ee->execution.progress = (double)ee->execution.completed_nodes / (double)wf->node_count;
            if (ee->execution.completed_nodes >= wf->node_count) {
                ee->execution.state = TASKFLOW_STATE_COMPLETED;
                ee->execution.completed_at = (uint64_t)time(NULL);
                ee->execution.progress = 1.0;
            }
        }
        return 0;
    }
    return 1;
}

int taskflow_engine_run_to_completion(taskflow_engine_t* engine,
                                        const char* execution_id) {
    if (!engine || !execution_id) return -1;
    for (size_t i = 0; i < engine->execution_count; i++) {
        execution_entry_t* ee = &engine->executions[i];
        if (strcmp(ee->execution.execution_id, execution_id) != 0) continue;
        if (ee->execution.state != TASKFLOW_STATE_RUNNING) return -2;

        taskflow_workflow_t* wf = find_workflow(engine, ee->execution.workflow_id);
        if (!wf) return -3;

        ee->execution.completed_nodes = wf->node_count;
        ee->execution.progress = 1.0;
        ee->execution.state = TASKFLOW_STATE_COMPLETED;
        ee->execution.completed_at = (uint64_t)time(NULL);

        if (engine->progress_cb) {
            engine->progress_cb(ee->execution.execution_id, NULL,
                                 TASKFLOW_STATE_COMPLETED, 1.0,
                                 engine->progress_user_data);
        }
        return 0;
    }
    return 1;
}

int taskflow_engine_create_checkpoint(taskflow_engine_t* engine,
                                        const char* execution_id,
                                        char** checkpoint_id) {
    if (!engine || !execution_id) return -1;
    for (size_t i = 0; i < engine->execution_count; i++) {
        execution_entry_t* ee = &engine->executions[i];
        if (strcmp(ee->execution.execution_id, execution_id) != 0) continue;
        if (ee->checkpoint_count >= TASKFLOW_MAX_CHECKPOINTS) return -2;

        taskflow_checkpoint_t* cp = &ee->checkpoints[ee->checkpoint_count];
        uint64_t cid = generate_id(engine);
        snprintf(cp->id, sizeof(cp->id), "cp_%lu", (unsigned long)cid);
        strncpy(cp->execution_id, execution_id, sizeof(cp->execution_id) - 1);
        strncpy(cp->workflow_id, ee->execution.workflow_id, sizeof(cp->workflow_id) - 1);
        if (ee->execution.current_node_id)
            strncpy(cp->node_id, ee->execution.current_node_id, sizeof(cp->node_id) - 1);
        cp->state = ee->execution.state;
        size_t snap_len = 256 + (ee->variables_json ? strlen(ee->variables_json) : 0);
        cp->snapshot_json = (char*)malloc(snap_len);
        if (cp->snapshot_json) {
            snprintf(cp->snapshot_json, snap_len,
                     "{\"superstep\":%d,\"progress\":%.2f,\"completed_nodes\":%zu,\"variables\":%s}",
                     ee->execution.superstep, ee->execution.progress,
                     ee->execution.completed_nodes,
                     ee->variables_json ? ee->variables_json : "{}");
        }
        cp->timestamp = (uint64_t)time(NULL);

        if (checkpoint_id) {
            *checkpoint_id = strdup(cp->id);
        }
        ee->checkpoint_count++;
        return 0;
    }
    return 1;
}

int taskflow_engine_restore_checkpoint(taskflow_engine_t* engine,
                                         const char* checkpoint_id) {
    if (!engine || !checkpoint_id) return -1;
    for (size_t i = 0; i < engine->execution_count; i++) {
        execution_entry_t* ee = &engine->executions[i];
        for (size_t j = 0; j < ee->checkpoint_count; j++) {
            if (strcmp(ee->checkpoints[j].id, checkpoint_id) == 0) {
                ee->execution.state = ee->checkpoints[j].state;
                if (ee->execution.current_node_id) free(ee->execution.current_node_id);
                ee->execution.current_node_id = strdup(ee->checkpoints[j].node_id);
                ee->execution.state = TASKFLOW_STATE_RUNNING;
                return 0;
            }
        }
    }
    return 1;
}

int taskflow_engine_list_checkpoints(taskflow_engine_t* engine,
                                       const char* execution_id,
                                       taskflow_checkpoint_t** checkpoints,
                                       size_t* count) {
    if (!engine || !execution_id || !checkpoints || !count) return -1;
    for (size_t i = 0; i < engine->execution_count; i++) {
        execution_entry_t* ee = &engine->executions[i];
        if (strcmp(ee->execution.execution_id, execution_id) != 0) continue;
        *checkpoints = ee->checkpoints;
        *count = ee->checkpoint_count;
        return 0;
    }
    *count = 0;
    return 1;
}

int taskflow_engine_set_condition_fn(taskflow_engine_t* engine,
                                       taskflow_condition_fn fn,
                                       void* user_data) {
    if (!engine) return -1;
    engine->condition_fn = fn;
    engine->condition_user_data = user_data;
    return 0;
}

int taskflow_engine_set_progress_callback(taskflow_engine_t* engine,
                                            taskflow_progress_callback_t callback,
                                            void* user_data) {
    if (!engine) return -1;
    engine->progress_cb = callback;
    engine->progress_user_data = user_data;
    return 0;
}

int taskflow_engine_set_event_callback(taskflow_engine_t* engine,
                                         taskflow_event_callback_t callback,
                                         void* user_data) {
    if (!engine) return -1;
    engine->event_cb = callback;
    engine->event_user_data = user_data;
    return 0;
}

int taskflow_engine_notify_event(taskflow_engine_t* engine,
                                   const char* execution_id,
                                   const char* event_type,
                                   const char* data_json) {
    if (!engine || !event_type) return -1;
    if (engine->event_cb) {
        engine->event_cb(execution_id, event_type, data_json, engine->event_user_data);
    }
    return 0;
}

int taskflow_engine_set_variable(taskflow_engine_t* engine,
                                   const char* execution_id,
                                   const char* key,
                                   const char* value_json) {
    if (!engine || !execution_id || !key || !value_json) return -1;
    for (size_t i = 0; i < engine->execution_count; i++) {
        execution_entry_t* ee = &engine->executions[i];
        if (strcmp(ee->execution.execution_id, execution_id) != 0) continue;
        size_t old_len = ee->variables_json ? strlen(ee->variables_json) : 2;
        size_t new_len = old_len + strlen(key) + strlen(value_json) + 32;
        char* new_vars = (char*)malloc(new_len);
        if (!new_vars) return -2;
        if (old_len <= 2) {
            snprintf(new_vars, new_len, "{\"%s\":%s}", key, value_json);
        } else {
            snprintf(new_vars, new_len, "{\"%s\":%s,\"_prev\":%s}",
                     key, value_json, ee->variables_json ? ee->variables_json : "{}");
        }
        if (ee->variables_json) free(ee->variables_json);
        ee->variables_json = new_vars;
        return 0;
    }
    return 1;
}

int taskflow_engine_get_variable(taskflow_engine_t* engine,
                                   const char* execution_id,
                                   const char* key,
                                   char** value_json) {
    if (!engine || !execution_id || !key || !value_json) return -1;
    *value_json = NULL;
    for (size_t i = 0; i < engine->execution_count; i++) {
        execution_entry_t* ee = &engine->executions[i];
        if (strcmp(ee->execution.execution_id, execution_id) != 0) continue;
        if (!ee->variables_json) return 1;
        char search_key[128];
        snprintf(search_key, sizeof(search_key), "\"%s\":", key);
        const char* found = strstr(ee->variables_json, search_key);
        if (!found) return 1;
        found += strlen(search_key);
        while (*found == ' ') found++;
        const char* end = found;
        if (*end == '"') {
            end++;
            while (*end && *end != '"') end++;
            if (*end) end++;
        } else if (*end == '{' || *end == '[') {
            int depth = 1;
            end++;
            while (*end && depth > 0) {
                if (*end == '{' || *end == '[') depth++;
                else if (*end == '}' || *end == ']') depth--;
                end++;
            }
        } else {
            while (*end && *end != ',' && *end != '}') end++;
        }
        size_t val_len = (size_t)(end - found);
        *value_json = (char*)malloc(val_len + 1);
        if (*value_json) {
            memcpy(*value_json, found, val_len);
            (*value_json)[val_len] = '\0';
        }
        return 0;
    }
    return 1;
}

size_t taskflow_engine_get_workflow_count(taskflow_engine_t* engine) {
    return engine ? engine->workflow_count : 0;
}

size_t taskflow_engine_get_execution_count(taskflow_engine_t* engine) {
    return engine ? engine->execution_count : 0;
}

void taskflow_workflow_destroy(taskflow_workflow_t* workflow) {
    if (!workflow) return;
    if (workflow->nodes) {
        for (size_t i = 0; i < workflow->node_count; i++) {
            free(workflow->nodes[i].task_handler_name);
            free(workflow->nodes[i].config_json);
            free(workflow->nodes[i].input_transform_json);
            free(workflow->nodes[i].output_transform_json);
            free(workflow->nodes[i].fallback_handler_name);
            free(workflow->nodes[i].condition_expr);
            free(workflow->nodes[i].subflow_id);
            free(workflow->nodes[i].loop_condition_expr);
            free(workflow->nodes[i].loop_foreach_json);
            free(workflow->nodes[i].event_type);
        }
        free(workflow->nodes);
    }
    free(workflow->edges);
    free(workflow->initial_node_id);
    free(workflow->input_schema_json);
    free(workflow->output_schema_json);
}

void taskflow_execution_destroy(taskflow_execution_t* execution) {
    if (!execution) return;
    if (execution->current_node_id) free(execution->current_node_id);
    if (execution->input_json) free(execution->input_json);
    if (execution->output_json) free(execution->output_json);
    if (execution->error_message) free(execution->error_message);
    if (execution->variables_json) free(execution->variables_json);
}

void taskflow_checkpoint_destroy(taskflow_checkpoint_t* checkpoint) {
    if (!checkpoint) return;
    if (checkpoint->snapshot_json) free(checkpoint->snapshot_json);
}
