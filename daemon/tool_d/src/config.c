/**
 * @file manager.c
 * @brief 工具服务配置加载（YAML）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "manager.h"
#include "svc_logger.h"
#include <yaml.h>
#include <stdlib.h>
#include <string.h>

static void free_tool_def(tool_def_t* def) {
    free(def->name);
    free(def->executable);
    if (def->params) {
        for (char** p = def->params; *p; ++p) free(*p);
        free(def->params);
    }
    free(def->permission_rule);
}

tool_config_t* tool_config_load(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        SVC_LOG_ERROR("Cannot open manager: %s", path);
        return NULL;
    }

    yaml_parser_t parser;
    yaml_event_t event;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_file(&parser, f);

    tool_config_t* cfg = calloc(1, sizeof(tool_config_t));
    if (!cfg) {
        fclose(f);
        yaml_parser_delete(&parser);
        return NULL;
    }

    /* 设置默认值 */
    cfg->cache_capacity = 100;
    cfg->cache_ttl_sec = 300;
    cfg->executor_workers = 4;
    cfg->workbench_type = strdup("process");

    /* 解析状态机 */
    enum { STATE_ROOT, STATE_TOOLS, STATE_TOOL } state = STATE_ROOT;
    tool_def_t cur_tool = {0};
    char** params = NULL;
    size_t params_cnt = 0, params_cap = 0;
    int in_tools_list = 0;

    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;

        if (event.type == YAML_SCALAR_EVENT) {
            const char* val = (const char*)event.data.scalar.value;

            if (state == STATE_ROOT && strcmp(val, "tools") == 0) {
                state = STATE_TOOLS;
            } else if (state == STATE_TOOLS && strcmp(val, "-") == 0) {
                in_tools_list = 1;
                state = STATE_TOOL;
            } else if (state == STATE_TOOL) {
                if (strcmp(val, "name") == 0) {
                    yaml_parser_parse(&parser, &event);
                    cur_tool.name = strdup((const char*)event.data.scalar.value);
                } else if (strcmp(val, "executable") == 0) {
                    yaml_parser_parse(&parser, &event);
                    cur_tool.executable = strdup((const char*)event.data.scalar.value);
                } else if (strcmp(val, "params") == 0) {
                    /* 进入参数列表 */
                } else if (strcmp(val, "-") == 0 && in_tools_list) {
                    /* 参数列表项 */
                    yaml_parser_parse(&parser, &event);
                    const char* param = (const char*)event.data.scalar.value;
                    if (params_cnt + 1 >= params_cap) {
                        params_cap = params_cap ? params_cap * 2 : 4;
                        char** new_params = realloc(params, params_cap * sizeof(char*));
                        if (!new_params) {
                            SVC_LOG_ERROR("Out of memory");
                            goto error;
                        }
                        params = new_params;
                    }
                    params[params_cnt++] = strdup(param);
                } else if (strcmp(val, "timeout_sec") == 0) {
                    yaml_parser_parse(&parser, &event);
                    cur_tool.timeout_sec = atoi((const char*)event.data.scalar.value);
                } else if (strcmp(val, "cacheable") == 0) {
                    yaml_parser_parse(&parser, &event);
                    cur_tool.cacheable = atoi((const char*)event.data.scalar.value);
                } else if (strcmp(val, "permission_rule") == 0) {
                    yaml_parser_parse(&parser, &event);
                    cur_tool.permission_rule = strdup((const char*)event.data.scalar.value);
                } else if (strcmp(val, "cache_capacity") == 0 && state == STATE_ROOT) {
                    yaml_parser_parse(&parser, &event);
                    cfg->cache_capacity = atoi((const char*)event.data.scalar.value);
                } else if (strcmp(val, "cache_ttl_sec") == 0 && state == STATE_ROOT) {
                    yaml_parser_parse(&parser, &event);
                    cfg->cache_ttl_sec = atoi((const char*)event.data.scalar.value);
                } else if (strcmp(val, "executor_workers") == 0 && state == STATE_ROOT) {
                    yaml_parser_parse(&parser, &event);
                    cfg->executor_workers = atoi((const char*)event.data.scalar.value);
                } else if (strcmp(val, "workbench_type") == 0 && state == STATE_ROOT) {
                    yaml_parser_parse(&parser, &event);
                    free(cfg->workbench_type);
                    cfg->workbench_type = strdup((const char*)event.data.scalar.value);
                } else if (strcmp(val, "container_image") == 0 && state == STATE_ROOT) {
                    yaml_parser_parse(&parser, &event);
                    cfg->container_image = strdup((const char*)event.data.scalar.value);
                }
            }
        } else if (event.type == YAML_MAPPING_END_EVENT) {
            if (state == STATE_TOOL && cur_tool.name) {
                /* 完成一个工具的解析 */
                if (params) {
                    params[params_cnt] = NULL;
                    cur_tool.params = params;
                } else {
                    cur_tool.params = NULL;
                }

                /* 添加到 cfg->tools 数组 */
                size_t cur_cnt = 0;
                while (cfg->tools && cfg->tools[cur_cnt].name) cur_cnt++;
                tool_def_t* new_tools = realloc(cfg->tools, (cur_cnt + 2) * sizeof(tool_def_t));
                if (!new_tools) {
                    SVC_LOG_ERROR("Out of memory");
                    goto error;
                }
                cfg->tools = new_tools;
                cfg->tools[cur_cnt] = cur_tool;
                memset(&cfg->tools[cur_cnt + 1], 0, sizeof(tool_def_t));

                /* 重置临时变量 */
                memset(&cur_tool, 0, sizeof(cur_tool));
                params = NULL;
                params_cnt = params_cap = 0;
                in_tools_list = 0;
                state = STATE_TOOLS;
            }
        }

        yaml_event_delete(&event);
        if (event.type == YAML_STREAM_END_EVENT) break;
    }

    yaml_parser_delete(&parser);
    fclose(f);
    return cfg;

error:
    yaml_parser_delete(&parser);
    fclose(f);
    tool_config_free(cfg);
    free_tool_def(&cur_tool);
    if (params) {
        for (size_t i = 0; i < params_cnt; ++i) free(params[i]);
        free(params);
    }
    return NULL;
}

void tool_config_free(tool_config_t* cfg) {
    if (!cfg) return;
    if (cfg->tools) {
        for (tool_def_t* t = cfg->tools; t->name; ++t) {
            free_tool_def(t);
        }
        free(cfg->tools);
    }
    free(cfg->workbench_type);
    free(cfg->container_image);
    free(cfg);
}