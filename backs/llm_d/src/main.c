/**
 * @file main.c
 * @brief LLM 服务守护进程入口（Unix Socket JSON-RPC 服务器）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "llm_service.h"
#include "svc_logger.h"
#include "svc_error.h"
#include "response.h"
#include "monitor_service.h"
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <pthread.h>
#include <cjson/cJSON.h>
#include <time.h>

#define SOCKET_PATH "/var/run/agentos/llm.sock"
#define MAX_BUFFER 65536
#define MAX_CLIENTS 64
// From data intelligence emerges. by spharx

static llm_service_t* g_service = NULL;
static monitor_service_t* g_monitor = NULL;
static int g_server_fd = -1;
static volatile int g_running = 1;

static void signal_handler(int sig) {
    (void)sig;
    g_running = 0;
}

/* ---------- JSON-RPC 错误码 ---------- */
#define PARSE_ERROR     -32700
#define INVALID_REQUEST -32600
#define METHOD_NOT_FOUND -32601
#define INVALID_PARAMS  -32602
#define INTERNAL_ERROR  -32000

/* ---------- 构建错误响应 ---------- */
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

/* ---------- 构建成功响应 ---------- */
static char* build_success_response(cJSON* result, int id) {
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "jsonrpc", "2.0");
    cJSON_AddItemToObject(root, "result", result ? cJSON_Duplicate(result, 1) : cJSON_CreateNull());
    cJSON_AddNumberToObject(root, "id", id);
    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}

/* ---------- 解析请求参数为 llm_request_config_t ---------- */
static int parse_params(cJSON* params, llm_request_config_t* cfg) {
    memset(cfg, 0, sizeof(llm_request_config_t));
    cJSON* model = cJSON_GetObjectItem(params, "model");
    if (!cJSON_IsString(model)) return -1;
    cfg->model = model->valuestring;

    cJSON* messages = cJSON_GetObjectItem(params, "messages");
    if (cJSON_IsArray(messages)) {
        cfg->message_count = cJSON_GetArraySize(messages);
        /* 注意：这里仅临时指向，不分配内存，调用者需保证params生命周期 */
        static llm_message_t msgs[64]; /* 简化，固定大小 */
        if (cfg->message_count > 64) return -1;
        for (size_t i = 0; i < cfg->message_count; ++i) {
            cJSON* item = cJSON_GetArrayItem(messages, i);
            cJSON* role = cJSON_GetObjectItem(item, "role");
            cJSON* content = cJSON_GetObjectItem(item, "content");
            if (!cJSON_IsString(role) || !cJSON_IsString(content)) return -1;
            msgs[i].role = role->valuestring;
            msgs[i].content = content->valuestring;
        }
        cfg->messages = msgs;
    }

    cJSON* temp = cJSON_GetObjectItem(params, "temperature");
    if (cJSON_IsNumber(temp)) cfg->temperature = (float)temp->valuedouble;
    cJSON* top_p = cJSON_GetObjectItem(params, "top_p");
    if (cJSON_IsNumber(top_p)) cfg->top_p = (float)top_p->valuedouble;
    cJSON* max_tokens = cJSON_GetObjectItem(params, "max_tokens");
    if (cJSON_IsNumber(max_tokens)) cfg->max_tokens = max_tokens->valueint;
    cJSON* stream = cJSON_GetObjectItem(params, "stream");
    if (cJSON_IsBool(stream)) cfg->stream = stream->valueint;
    /* 省略 stop, penalties 等以简化 */
    return 0;
}

/* ---------- 处理 complete 方法 ---------- */
static void handle_complete(cJSON* params, int id, int client_fd) {
    llm_request_config_t cfg;
    if (parse_params(params, &cfg) != 0) {
        char* err = build_error_response(INVALID_PARAMS, "Invalid params", id);
        write(client_fd, err, strlen(err));
        free(err);
        return;
    }

    // 记录请求开始时间
    uint64_t start_time = (uint64_t)time(NULL) * 1000;

    llm_response_t* resp = NULL;
    int ret = llm_service_complete(g_service, &cfg, &resp);
    
    // 记录请求结束时间
    uint64_t end_time = (uint64_t)time(NULL) * 1000;
    uint32_t execution_time = (uint32_t)(end_time - start_time);

    if (ret != 0) {
        char* err = build_error_response(INTERNAL_ERROR, "Service error", id);
        write(client_fd, err, strlen(err));
        free(err);
        
        // 记录失败指标
        metric_info_t metric = {
            .name = "llm.request.failed",
            .description = "LLM 请求失败",
            .type = METRIC_TYPE_COUNTER,
            .labels = NULL,
            .label_count = 0,
            .value = 1.0,
            .timestamp = end_time
        };
        monitor_service_record_metric(g_monitor, &metric);
        
        return;
    }

    /* 将响应转换为 JSON */
    char* resp_json = response_to_json(resp);
    llm_response_free(resp);
    if (!resp_json) {
        char* err = build_error_response(INTERNAL_ERROR, "Failed to serialize response", id);
        write(client_fd, err, strlen(err));
        free(err);
        return;
    }

    cJSON* result = cJSON_Parse(resp_json);
    free(resp_json);
    if (!result) {
        char* err = build_error_response(INTERNAL_ERROR, "Invalid response format", id);
        write(client_fd, err, strlen(err));
        free(err);
        return;
    }

    char* success = build_success_response(result, id);
    cJSON_Delete(result);
    write(client_fd, success, strlen(success));
    free(success);
    
    // 记录成功指标
    metric_info_t metric = {
        .name = "llm.request.succeeded",
        .description = "LLM 请求成功",
        .type = METRIC_TYPE_COUNTER,
        .labels = NULL,
        .label_count = 0,
        .value = 1.0,
        .timestamp = end_time
    };
    monitor_service_record_metric(g_monitor, &metric);
    
    metric.name = "llm.request.execution_time";
    metric.description = "LLM 请求执行时间";
    metric.type = METRIC_TYPE_GAUGE;
    metric.value = (double)execution_time;
    monitor_service_record_metric(g_monitor, &metric);
    
    // 记录日志
    log_info_t log = {
        .level = LOG_LEVEL_INFO,
        .message = "LLM request completed successfully",
        .service_name = "llm_d",
        .file = __FILE__,
        .line = __LINE__,
        .function = __func__,
        .timestamp = end_time,
        .context = NULL,
        .context_count = 0
    };
    monitor_service_log(g_monitor, &log);
}

/* ---------- 流式回调 ---------- */
static void stream_callback(const char* chunk, void* user_data) {
    int client_fd = *((int*)user_data);
    write(client_fd, chunk, strlen(chunk));
}

/* ---------- 处理 complete_stream 方法 ---------- */
static void handle_complete_stream(cJSON* params, int id, int client_fd) {
    llm_request_config_t cfg;
    if (parse_params(params, &cfg) != 0) {
        char* err = build_error_response(INVALID_PARAMS, "Invalid params", id);
        write(client_fd, err, strlen(err));
        free(err);
        return;
    }

    // 记录请求开始时间
    uint64_t start_time = (uint64_t)time(NULL) * 1000;

    llm_response_t* resp = NULL;
    int ret = llm_service_complete_stream(g_service, &cfg, stream_callback, &resp);
    
    // 记录请求结束时间
    uint64_t end_time = (uint64_t)time(NULL) * 1000;
    uint32_t execution_time = (uint32_t)(end_time - start_time);

    if (ret != 0) {
        char* err = build_error_response(INTERNAL_ERROR, "Service error", id);
        write(client_fd, err, strlen(err));
        free(err);
        
        // 记录失败指标
        metric_info_t metric = {
            .name = "llm.stream.request.failed",
            .description = "LLM 流式请求失败",
            .type = METRIC_TYPE_COUNTER,
            .labels = NULL,
            .label_count = 0,
            .value = 1.0,
            .timestamp = end_time
        };
        monitor_service_record_metric(g_monitor, &metric);
        
        return;
    }

    if (resp) {
        llm_response_free(resp);
    }
    
    // 记录成功指标
    metric_info_t metric = {
        .name = "llm.stream.request.succeeded",
        .description = "LLM 流式请求成功",
        .type = METRIC_TYPE_COUNTER,
        .labels = NULL,
        .label_count = 0,
        .value = 1.0,
        .timestamp = end_time
    };
    monitor_service_record_metric(g_monitor, &metric);
    
    metric.name = "llm.stream.request.execution_time";
    metric.description = "LLM 流式请求执行时间";
    metric.type = METRIC_TYPE_GAUGE;
    metric.value = (double)execution_time;
    monitor_service_record_metric(g_monitor, &metric);
    
    // 记录日志
    log_info_t log = {
        .level = LOG_LEVEL_INFO,
        .message = "LLM stream request completed successfully",
        .service_name = "llm_d",
        .file = __FILE__,
        .line = __LINE__,
        .function = __func__,
        .timestamp = end_time,
        .context = NULL,
        .context_count = 0
    };
    monitor_service_log(g_monitor, &log);
}

/* ---------- 处理单个客户端连接 ---------- */
static void handle_client(int client_fd) {
    char buffer[MAX_BUFFER];
    ssize_t n = read(client_fd, buffer, sizeof(buffer) - 1);
    if (n <= 0) {
        close(client_fd);
        return;
    }
    buffer[n] = '\0';

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
        !cJSON_IsString(method) || !params || !id) {
        char* err = build_error_response(INVALID_REQUEST, "Invalid Request", -1);
        write(client_fd, err, strlen(err));
        free(err);
        cJSON_Delete(req);
        close(client_fd);
        return;
    }

    int req_id = cJSON_IsNumber(id) ? id->valueint : 0;
    if (strcmp(method->valuestring, "complete") == 0) {
        handle_complete(params, req_id, client_fd);
    } else if (strcmp(method->valuestring, "complete_stream") == 0) {
        handle_complete_stream(params, req_id, client_fd);
    } else {
        char* err = build_error_response(METHOD_NOT_FOUND, "Method not found", req_id);
        write(client_fd, err, strlen(err));
        free(err);
    }

    cJSON_Delete(req);
    close(client_fd);
}

/* ---------- 主函数 ---------- */
int main(int argc, char** argv) {
    const char* config_path = "config/services/llm.yaml";
    if (argc > 1) config_path = argv[1];

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    SVC_LOG_INFO("LLM service starting, config=%s", config_path);
    g_service = llm_service_create(config_path);
    if (!g_service) {
        SVC_LOG_ERROR("Failed to create service");
        return 1;
    }

    // 初始化监控服务
    monitor_config_t monitor_config = {
        .metrics_collection_interval_ms = 5000,
        .health_check_interval_ms = 10000,
        .log_flush_interval_ms = 30000,
        .alert_check_interval_ms = 5000,
        .log_file_path = "llm_monitor.log",
        .metrics_storage_path = "llm_metrics",
        .enable_tracing = true,
        .enable_alerting = true
    };

    if (monitor_service_create(&monitor_config, &g_monitor) != 0) {
        SVC_LOG_ERROR("Failed to create monitor service");
        llm_service_destroy(g_service);
        return 1;
    }

    // 记录服务启动指标
    metric_info_t metric = {
        .name = "llm.service.started",
        .description = "LLM 服务启动",
        .type = METRIC_TYPE_COUNTER,
        .labels = NULL,
        .label_count = 0,
        .value = 1.0,
        .timestamp = (uint64_t)time(NULL) * 1000
    };
    monitor_service_record_metric(g_monitor, &metric);

    /* 创建 Unix socket 服务器 */
    struct sockaddr_un addr;
    g_server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (g_server_fd < 0) {
        SVC_LOG_ERROR("socket failed");
        llm_service_destroy(g_service);
        return 1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);
    unlink(SOCKET_PATH);

    if (bind(g_server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        SVC_LOG_ERROR("bind failed");
        close(g_server_fd);
        llm_service_destroy(g_service);
        return 1;
    }

    if (listen(g_server_fd, 5) < 0) {
        SVC_LOG_ERROR("listen failed");
        close(g_server_fd);
        llm_service_destroy(g_service);
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
    llm_service_destroy(g_service);
    
    // 销毁监控服务
    if (g_monitor) {
        monitor_service_destroy(g_monitor);
    }
    
    SVC_LOG_INFO("LLM service stopped");
    return 0;
}