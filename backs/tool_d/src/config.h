/**
 * @file config.h
 * @brief 工具服务配置结构
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef TOOL_CONFIG_H
#define TOOL_CONFIG_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char* name;
    char* executable;
    char** params;          /* 参数模式定义（可选） */
    int timeout_sec;
    int cacheable;
    char* permission_rule;
} tool_def_t;

typedef struct {
    tool_def_t* tools;       /* 内置工具定义，以 .name == NULL 结尾 */
    size_t cache_capacity;
    int cache_ttl_sec;
    int executor_workers;    /* 执行线程池大小 */
    char* workbench_type;    /* "process" 或 "container" */
    char* container_image;   /* 默认容器镜像 */
} tool_config_t;

tool_config_t* tool_config_load(const char* path);
void tool_config_free(tool_config_t* cfg);

#ifdef __cplusplus
}
#endif

#endif /* TOOL_CONFIG_H */