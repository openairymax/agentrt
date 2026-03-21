/**
 * @file tool_service.h
 * @brief 工具服务对外接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
 */

#ifndef AGENTOS_TOOL_SERVICE_H
#define AGENTOS_TOOL_SERVICE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- 公共类型定义 ---------- */

typedef struct tool_service tool_service_t;

/**
 * @brief 工具参数定义（JSON Schema 格式字符串）
 */
typedef struct {
    const char* name;          /* 参数名 */
    const char* schema;         /* JSON Schema 片段，如 "{\"type\":\"string\"}" */
} tool_param_t;

/**
 * @brief 工具元数据
 */
typedef struct {
    char* id;                    /* 工具唯一标识 */
    char* name;                  /* 工具名称 */
    char* description;           /* 描述 */
    char* executable;            /* 可执行文件路径或命令 */
    tool_param_t* params;        /* 参数定义数组 */
    size_t param_count;
    int timeout_sec;             /* 执行超时（秒） */
    int cacheable;               /* 结果是否可缓存（0/1） */
    char* permission_rule;       /* 权限规则标识（与 domes 配合） */
} tool_metadata_t;

/**
 * @brief 工具执行请求
 */
typedef struct {
    const char* tool_id;         /* 工具ID */
    const char* params_json;     /* 参数 JSON 字符串 */
    int stream;                  /* 是否流式输出 */
    void* user_data;             /* 透传用户数据 */
} tool_execute_request_t;

/**
 * @brief 工具执行结果（非流式）
 */
typedef struct {
    int success;                 /* 0 成功，非0失败 */
    char* output;                /* 标准输出（文本） */
    char* error;                 /* 标准错误 */
    int exit_code;               /* 进程退出码 */
    uint64_t duration_ms;        /* 执行耗时 */
} tool_result_t;

/**
 * @brief 流式输出回调
 * @param chunk  输出数据块
 * @param is_stderr 是否为错误输出
 * @param user_data 用户数据
 */
typedef void (*tool_stream_callback_t)(const char* chunk, int is_stderr, void* user_data);

/* ---------- 生命周期 ---------- */

tool_service_t* tool_service_create(const char* config_path);
void tool_service_destroy(tool_service_t* svc);

/* ---------- 工具管理接口 ---------- */

int tool_service_register(tool_service_t* svc, const tool_metadata_t* meta);
int tool_service_unregister(tool_service_t* svc, const char* tool_id);
tool_metadata_t* tool_service_get(tool_service_t* svc, const char* tool_id);
void tool_metadata_free(tool_metadata_t* meta);
char* tool_service_list(tool_service_t* svc);  /* 返回 JSON 数组 */

/* ---------- 工具执行接口 ---------- */

int tool_service_execute(tool_service_t* svc,
                         const tool_execute_request_t* req,
                         tool_result_t** out_result);

int tool_service_execute_stream(tool_service_t* svc,
                                const tool_execute_request_t* req,
                                tool_stream_callback_t callback,
                                tool_result_t** out_result);

void tool_result_free(tool_result_t* res);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_TOOL_SERVICE_H */