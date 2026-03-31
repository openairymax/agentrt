/**
 * @file main.c
 * @brief 工具服务守护进程入口（Unix Socket JSON-RPC 服务器）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "tool_service.h"
#include "svc_logger.h"
#include "error.h"
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <pthread.h>
#include <cjson/cJSON.h>

#define SOCKET_PATH "/var/run/agentos/tool.sock"
#define MAX_BUFFER 65536

static tool_service_t* g_service = NULL;
static int g_server_fd = -1;
static volatile int g_running = 1;

static void signal_handler(int sig) {
    (void)sig;
    g_running = 0;
}

/* JSON-RPC 错误码 */
#define PARSE_ERROR     -32700
#define INVALID_REQUEST -32600
#define METHOD_NOT_FOUND -32601
#define INVALID_PARAMS  -32602
#define INTERNAL_ERROR  -32000

/* 构建错误响应 */
static char* build_error_response(int code, const char* message, int id) {
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "jsonrpc", "2.0");
    cJSON* err = cJSON_CreateObject();
    cJSON_AddNumberToObject(err, "code", code);
    cJSON_AddStringToObject(err, "message", message);
    cJSON_AddItemToObject(root, "error", err);
    if (id >= 0)
        cJSON_AddNumberToObject(root, "id", id);
    else
        cJSON_AddNullToObject(root, "id");
    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}

/* 构建成功响应 */
static char* build_success_response(cJSON* result, int id) {
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "jsonrpc", "2.0");
    cJSON_AddItemToObject(root, "result", result ? cJSON_Duplicate(result, 1) : cJSON_CreateNull());
    cJSON_AddNumberToObject(root, "id", id);
    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}

/* 处理 register_tool 方法 */
static void handle_register(cJSON* params, int id, int client_fd) {
    /* 解析参数：需要 tool 对象 */
    cJSON* tool = cJSON_GetObjectItem(params, "tool");
    if (!cJSON_IsObject(tool)) {
        char* err = build_error_response(INVALID_PARAMS, "Missing tool object", id);
        write(client_fd, err, strlen(err));
        free(err);
        return;
    }

    tool_metadata_t meta = {0};
    cJSON* jid = cJSON_GetObjectItem(tool, "id");
    cJSON* jname = cJSON_GetObjectItem(tool, "name");
    cJSON* jexec = cJSON_GetObjectItem(tool, "executable");
    if (!cJSON_IsString(jid) || !cJSON_IsString(jname) || !cJSON_IsString(jexec)) {
        char* err = build_error_response(INVALID_PARAMS, "Invalid tool fields", id);
        write(client_fd, err, strlen(err));
        free(err);
        return;
    }
    meta.id = jid->valuestring;
    meta.name = jname->valuestring;
    meta.executable = jexec->valuestring;
    /* 其他字段可选 */
    cJSON* desc = cJSON_GetObjectItem(tool, "description");
    if (cJSON_IsString(desc)) meta.description = desc->valuestring;
    cJSON* timeout = cJSON_GetObjectItem(tool, "timeout_sec");
    if (cJSON_IsNumber(timeout)) meta.timeout_sec = timeout->valueint;
    cJSON* cacheable = cJSON_GetObjectItem(tool, "cacheable");
    if (cJSON_IsBool(cacheable)) meta.cacheable = cacheable->valueint;
    cJSON* rule = cJSON_GetObjectItem(tool, "permission_rule");
    if (cJSON_IsString(rule)) meta.permission_rule = rule->valuestring;

    /* 参数列表 */
    cJSON* params_arr = cJSON_GetObjectItem(tool, "params");
    if (cJSON_IsArray(params_arr)) {
        size_t cnt = cJSON_GetArraySize(params_arr);
        tool_param_t* p = calloc(cnt, sizeof(tool_param_t));
        if (p) {
            for (size_t i = 0; i < cnt; ++i) {
                cJSON* item = cJSON_GetArrayItem(params_arr, i);
                cJSON* pname = cJSON_GetObjectItem(item, "name");
                cJSON* pschema = cJSON_GetObjectItem(item, "schema");
                if (cJSON_IsString(pname)) p[i].name = pname->valuestring;
                if (cJSON_IsString(pschema)) p[i].schema = pschema->valuestring;
            }
            meta.params = p;
            meta.param_count = cnt;
        }
    }

    int ret = tool_service_register(g_service, &meta);
    free((void*)meta.params); /* 注意：params 是临时分配，但注册内部会复制，所以可释放 */

    if (ret != 0) {
        char* err = build_error_response(INTERNAL_ERROR, "Register failed", id);
        write(client_fd, err, strlen(err));
        free(err);
    } else {
        char* success = build_success_response(NULL, id);
        write(client_fd, success, strlen(success));
        free(success);
    }
}

/* 处理 list_tools 方法 */
static void handle_list(int id, int client_fd) {
    char* list_json = tool_service_list(g_service);
    if (!list_json) {
        char* err = build_error_response(INTERNAL_ERROR, "List failed", id);
        write(client_fd, err, strlen(err));
        free(err);
        return;
    }
    cJSON* result = cJSON_Parse(list_json);
    free(list_json);
    if (!result) {
        char* err = build_error_response(INTERNAL_ERROR, "Invalid JSON", id);
        write(client_fd, err, strlen(err));
        free(err);
        return;
    }
    char* success = build_success_response(result, id);
    cJSON_Delete(result);
    write(client_fd, success, strlen(success));
    free(success);
}

/* 处理 get_tool 方法 */
static void handle_get(cJSON* params, int id, int client_fd) {
    cJSON* jid = cJSON_GetObjectItem(params, "tool_id");
    if (!cJSON_IsString(jid)) {
        char* err = build_error_response(INVALID_PARAMS, "Missing tool_id", id);
        write(client_fd, err, strlen(err));
        free(err);
        return;
    }
    tool_metadata_t* meta = tool_service_get(g_service, jid->valuestring);
    if (!meta) {
        char* err = build_error_response(METHOD_NOT_FOUND, "Tool not found", id);
        write(client_fd, err, strlen(err));
        free(err);
        return;
    }

    /* 转换为 JSON */
    cJSON* obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj, "id", meta->id);
    cJSON_AddStringToObject(obj, "name", meta->name);
    cJSON_AddStringToObject(obj, "executable", meta->executable);
    if (meta->description) cJSON_AddStringToObject(obj, "description", meta->description);
    cJSON_AddNumberToObject(obj, "timeout_sec", meta->timeout_sec);
    cJSON_AddBoolToObject(obj, "cacheable", meta->cacheable);
    if (meta->permission_rule) cJSON_AddStringToObject(obj, "permission_rule", meta->permission_rule);
    if (meta->param_count > 0) {
        cJSON* params_arr = cJSON_CreateArray();
        for (size_t i = 0; i < meta->param_count; ++i) {
            cJSON* pobj = cJSON_CreateObject();
            cJSON_AddStringToObject(pobj, "name", meta->params[i].name);
            cJSON_AddStringToObject(pobj, "schema", meta->params[i].schema);
            cJSON_AddItemToArray(params_arr, pobj);
        }
        cJSON_AddItemToObject(obj, "params", params_arr);
    }

    char* success = build_success_response(obj, id);
    cJSON_Delete(obj);
    write(client_fd, success, strlen(success));
    free(success);
    tool_metadata_free(meta);
}

/* 处理 execute_tool 方法 */
static void handle_execute(cJSON* params, int id, int client_fd) {
    cJSON* jid = cJSON_GetObjectItem(params, "tool_id");
    cJSON* jparams = cJSON_GetObjectItem(params, "params");
    if (!cJSON_IsString(jid) || !cJSON_IsObject(jparams)) {
        char* err = build_error_response(INVALID_PARAMS, "Invalid execute params", id);
        write(client_fd, err, strlen(err));
        free(err);
        return;
    }

    char* params_json = cJSON_PrintUnformatted(jparams);
    if (!params_json) {
        char* err = build_error_response(INTERNAL_ERROR, "JSON error", id);
        write(client_fd, err, strlen(err));
        free(err);
        return;
    }

    tool_execute_request_t req = {
        .tool_id = jid->valuestring,
        .params_json = params_json,
        .stream = 0
    };
    tool_result_t* res = NULL;
    int ret = tool_service_execute(g_service, &req, &res);
    free((void*)params_json);

    if (ret != 0) {
        char* err = build_error_response(INTERNAL_ERROR, "Execution failed", id);
        write(client_fd, err, strlen(err));
        free(err);
        return;
    }

    /* 结果转 JSON */
    cJSON* result = cJSON_CreateObject();
    cJSON_AddNumberToObject(result, "success", res->success);
    if (res->output) cJSON_AddStringToObject(result, "output", res->output);
    if (res->error) cJSON_AddStringToObject(result, "error", res->error);
    cJSON_AddNumberToObject(result, "exit_code", res->exit_code);
    char* success = build_success_response(result, id);
    cJSON_Delete(result);
    write(client_fd, success, strlen(success));
    free(success);
    tool_result_free(res);
}

/* 处理客户端连接 */
static void handle_client(int client_fd) {
    char buffer[MAX_BUFFER];
    ssize_t n = read(client_fd, buffer, sizeof(buffer) - 1);
    if (n <= 0) {
        close(client_fd);
        return;
    }
    buffer[n] = '\0';

    // 检查请求大小
    if (n >= sizeof(buffer) - 1) {
        char* err = build_error_response(INVALID_REQUEST, "Request too large", -1);
        write(client_fd, err, strlen(err));
        free(err);
        close(client_fd);
        return;
    }

    cJSON* req = cJSON_Parse(buffer);
    if (!req) {
        char* err = build_error_response(PARSE_ERROR, "Parse error", -1);
        write(client_fd, err, strlen(err));
        free(err);
        close(client_fd);
        return;
    }

    cJSON* jsonrpc = cJSON_GetObjectItem(req, "jsonrpc");
    cJSON* method = cJSON_GetObjectItem(req, "method");
    cJSON* params = cJSON_GetObjectItem(req, "params");
    cJSON* id = cJSON_GetObjectItem(req, "id");

    if (!cJSON_IsString(jsonrpc) || strcmp(jsonrpc->valuestring, "2.0") != 0 ||
        !cJSON_IsString(method) || !id) {
        char* err = build_error_response(INVALID_REQUEST, "Invalid Request", -1);
        write(client_fd, err, strlen(err));
        free(err);
        cJSON_Delete(req);
        close(client_fd);
        return;
    }

    int req_id = cJSON_IsNumber(id) ? id->valueint : 0;

    if (strcmp(method->valuestring, "register_tool") == 0) {
        handle_register(params, req_id, client_fd);
    } else if (strcmp(method->valuestring, "list_tools") == 0) {
        handle_list(req_id, client_fd);
    } else if (strcmp(method->valuestring, "get_tool") == 0) {
        handle_get(params, req_id, client_fd);
    } else if (strcmp(method->valuestring, "execute_tool") == 0) {
        handle_execute(params, req_id, client_fd);
    } else {
        char* err = build_error_response(METHOD_NOT_FOUND, "Method not found", req_id);
        write(client_fd, err, strlen(err));
        free(err);
    }

    cJSON_Delete(req);
    close(client_fd);
}

int main(int argc, char** argv) {
    const char* config_path = "manager/service/tool_d/tool.yaml";
    if (argc > 1) config_path = argv[1];

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    SVC_LOG_INFO("Tool service starting, manager=%s", config_path);
    g_service = tool_service_create(config_path);
    if (!g_service) {
        SVC_LOG_ERROR("Failed to create service");
        return 1;
    }

    /* 创建 Unix socket 服务器 */
    struct sockaddr_un addr;
    g_server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (g_server_fd < 0) {
        SVC_LOG_ERROR("socket failed");
        tool_service_destroy(g_service);
        return 1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);
    addr.sun_path[sizeof(addr.sun_path) - 1] = '\0';
    unlink(SOCKET_PATH);

    if (bind(g_server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        SVC_LOG_ERROR("bind failed");
        close(g_server_fd);
        tool_service_destroy(g_service);
        return 1;
    }

    if (listen(g_server_fd, 5) < 0) {
        SVC_LOG_ERROR("listen failed");
        close(g_server_fd);
        tool_service_destroy(g_service);
        return 1;
    }

    SVC_LOG_INFO("Listening on %s", SOCKET_PATH);

    while (g_running) {
        int client_fd = accept(g_server_fd, NULL, NULL);
        if (client_fd < 0) {
            if (g_running) SVC_LOG_ERROR("accept failed");
            continue;
        }
        handle_client(client_fd);
    }

    close(g_server_fd);
    unlink(SOCKET_PATH);
    tool_service_destroy(g_service);
    SVC_LOG_INFO("Tool service stopped");
    return 0;
}