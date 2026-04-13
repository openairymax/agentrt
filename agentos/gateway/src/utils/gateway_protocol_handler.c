// SPDX-FileCopyrightText: 2026 SPHARX.
// SPDX-License-Identifier: Apache-2.0
/**
 * @file gateway_protocol_handler.c
 * @brief 多协议网关请求处理器实现
 * 
 * 实现多协议检测、转换和统一处理逻辑。
 */

#include "gateway_protocol_handler.h"
#include "jsonrpc.h"
#include "syscall_router.h"
#include <cJSON.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// ============================================================================
// 内部数据结构
// ============================================================================

struct gateway_protocol_handler_s {
    gateway_protocol_config_t config;
    protocol_router_handle_t router;
    
    // 统计信息
    uint64_t total_requests;
    uint64_t jsonrpc_requests;
    uint64_t mcp_requests;
    uint64_t a2a_requests;
    uint64_t openai_requests;
    uint64_t conversion_errors;
};

// ============================================================================
// 静态函数声明
// ============================================================================

static void init_default_config(gateway_protocol_config_t* config);
static unified_message_t create_unified_message(
    const char* data,
    size_t size,
    unified_protocol_type_t protocol_type);
static int jsonrpc_handler_adapter(const char* request_json,
                                   char** response_json,
                                   void* handler_data);
static int detect_protocol_internal(const char* data, size_t size);

// ============================================================================
// 核心API实现
// ============================================================================

gateway_protocol_handler_t gateway_protocol_handler_create(
    const gateway_protocol_config_t* config)
{
    struct gateway_protocol_handler_s* handler = 
        (struct gateway_protocol_handler_s*)calloc(1, sizeof(struct gateway_protocol_handler_s));
    if (!handler) {
        return NULL;
    }
    
    // 初始化配置
    if (config) {
        handler->config = *config;
    } else {
        init_default_config(&handler->config);
    }
    
    // 创建协议路由器（默认协议为JSON-RPC）
    handler->router = protocol_router_create(UNIFIED_PROTOCOL_HTTP);
    if (!handler->router) {
        free(handler);
        return NULL;
    }
    
    // TODO: 添加预定义协议转换规则
    // 这里可以添加MCP->JSON-RPC, A2A->JSON-RPC等规则
    
    handler->total_requests = 0;
    handler->jsonrpc_requests = 0;
    handler->mcp_requests = 0;
    handler->a2a_requests = 0;
    handler->openai_requests = 0;
    handler->conversion_errors = 0;
    
    return handler;
}

void gateway_protocol_handler_destroy(gateway_protocol_handler_t handler)
{
    if (!handler) return;
    
    struct gateway_protocol_handler_s* h = 
        (struct gateway_protocol_handler_s*)handler;
    
    if (h->router) {
        protocol_router_destroy(h->router);
    }
    
    free(h);
}

rpc_result_t gateway_protocol_handle_request(
    gateway_protocol_handler_t handler,
    const char* request_data,
    size_t request_size,
    unified_protocol_type_t protocol_type,
    int (*custom_handler)(const char*, char**, void*),
    void* handler_data)
{
    if (!handler || !request_data || request_size == 0) {
        return gateway_rpc_create_error(-1, "Invalid parameters");
    }
    
    struct gateway_protocol_handler_s* h = 
        (struct gateway_protocol_handler_s*)handler;
    h->total_requests++;
    
    // 协议检测
    unified_protocol_type_t detected_protocol = protocol_type;
    if (protocol_type == UNIFIED_PROTOCOL_AUTO) {
        detected_protocol = gateway_protocol_detect(request_data, request_size);
        
        // 更新统计信息
        switch (detected_protocol) {
            case UNIFIED_PROTOCOL_HTTP:
            case UNIFIED_PROTOCOL_WEBSOCKET:
                h->jsonrpc_requests++;
                break;
            case UNIFIED_PROTOCOL_MCP:
                h->mcp_requests++;
                break;
            case UNIFIED_PROTOCOL_A2A:
                h->a2a_requests++;
                break;
            case UNIFIED_PROTOCOL_OPENAI:
                h->openai_requests++;
                break;
            default:
                break;
        }
    } else {
        // 手动指定协议类型，更新对应统计
        switch (protocol_type) {
            case UNIFIED_PROTOCOL_HTTP:
            case UNIFIED_PROTOCOL_WEBSOCKET:
                h->jsonrpc_requests++;
                break;
            case UNIFIED_PROTOCOL_MCP:
                h->mcp_requests++;
                break;
            case UNIFIED_PROTOCOL_A2A:
                h->a2a_requests++;
                break;
            case UNIFIED_PROTOCOL_OPENAI:
                h->openai_requests++;
                break;
            default:
                break;
        }
    }
    
    // 如果已经是JSON-RPC，直接处理
    if (detected_protocol == UNIFIED_PROTOCOL_HTTP || 
        detected_protocol == UNIFIED_PROTOCOL_WEBSOCKET) {
        // 尝试解析为JSON
        cJSON* request = cJSON_ParseWithLength(request_data, request_size);
        if (!request) {
            return gateway_rpc_create_error(-1, "Invalid JSON format");
        }
        
        // 使用原有的JSON-RPC处理逻辑
        rpc_result_t result = gateway_rpc_handle_request(request, custom_handler, handler_data);
        cJSON_Delete(request);
        return result;
    }
    
    // 非JSON-RPC协议，需要转换
    char* jsonrpc_request = NULL;
    int convert_result = gateway_protocol_convert_to_jsonrpc(
        handler, request_data, request_size, detected_protocol, &jsonrpc_request);
    
    if (convert_result != 0 || !jsonrpc_request) {
        h->conversion_errors++;
        return gateway_rpc_create_error(-1, "Protocol conversion failed");
    }
    
    // 解析转换后的JSON-RPC请求
    cJSON* request = cJSON_Parse(jsonrpc_request);
    free(jsonrpc_request);
    
    if (!request) {
        return gateway_rpc_create_error(-1, "Converted request is invalid JSON");
    }
    
    // 处理JSON-RPC请求
    rpc_result_t result = gateway_rpc_handle_request(request, custom_handler, handler_data);
    cJSON_Delete(request);
    
    // 如果需要转换响应回原始协议
    // TODO: 根据配置决定是否转换响应
    
    return result;
}

void gateway_protocol_handler_get_default_config(
    gateway_protocol_config_t* config)
{
    if (!config) return;
    
    init_default_config(config);
}

int gateway_protocol_handler_load_config_from_json(
    gateway_protocol_config_t* config,
    const char* json_config)
{
    if (!config || !json_config) {
        return -1;
    }
    
    // TODO: 实现JSON配置解析
    // 目前使用默认配置
    init_default_config(config);
    
    return 0;
}

int gateway_protocol_handler_get_stats(
    gateway_protocol_handler_t handler,
    char** stats_json)
{
    if (!handler || !stats_json) {
        return -1;
    }
    
    struct gateway_protocol_handler_s* h = 
        (struct gateway_protocol_handler_s*)handler;
    
    const char* fmt = 
        "{"
        "\"total_requests\": %llu,"
        "\"jsonrpc_requests\": %llu,"
        "\"mcp_requests\": %llu,"
        "\"a2a_requests\": %llu,"
        "\"openai_requests\": %llu,"
        "\"conversion_errors\": %llu,"
        "\"conversion_success_rate\": %.2f"
        "}";
    
    uint64_t total_conversions = h->total_requests - h->jsonrpc_requests;
    float success_rate = 0.0f;
    if (total_conversions > 0) {
        uint64_t successful_conversions = total_conversions - h->conversion_errors;
        success_rate = (float)successful_conversions / total_conversions * 100.0f;
    }
    
    size_t buf_size = snprintf(NULL, 0, fmt,
                               h->total_requests,
                               h->jsonrpc_requests,
                               h->mcp_requests,
                               h->a2a_requests,
                               h->openai_requests,
                               h->conversion_errors,
                               success_rate) + 1;
    
    char* buf = (char*)malloc(buf_size);
    if (!buf) {
        return -1;
    }
    
    snprintf(buf, buf_size, fmt,
             h->total_requests,
             h->jsonrpc_requests,
             h->mcp_requests,
             h->a2a_requests,
             h->openai_requests,
             h->conversion_errors,
             success_rate);
    
    *stats_json = buf;
    return 0;
}

// ============================================================================
// 协议检测函数实现
// ============================================================================

unified_protocol_type_t gateway_protocol_detect(
    const char* request_data,
    size_t request_size)
{
    if (!request_data || request_size == 0) {
        return UNIFIED_PROTOCOL_UNKNOWN;
    }
    
    // 简单的基于内容特征的协议检测
    int result = detect_protocol_internal(request_data, request_size);
    
    switch (result) {
        case 1: return UNIFIED_PROTOCOL_HTTP;      // JSON-RPC over HTTP
        case 2: return UNIFIED_PROTOCOL_MCP;
        case 3: return UNIFIED_PROTOCOL_A2A;
        case 4: return UNIFIED_PROTOCOL_OPENAI;
        default: return UNIFIED_PROTOCOL_HTTP;     // 默认作为JSON-RPC处理
    }
}

int gateway_protocol_is_jsonrpc(
    const char* request_data,
    size_t request_size)
{
    return (detect_protocol_internal(request_data, request_size) == 1);
}

int gateway_protocol_is_mcp(
    const char* request_data,
    size_t request_size)
{
    return (detect_protocol_internal(request_data, request_size) == 2);
}

int gateway_protocol_is_a2a(
    const char* request_data,
    size_t request_size)
{
    return (detect_protocol_internal(request_data, request_size) == 3);
}

int gateway_protocol_is_openai(
    const char* request_data,
    size_t request_size)
{
    return (detect_protocol_internal(request_data, request_size) == 4);
}

// ============================================================================
// 协议转换函数实现
// ============================================================================

int gateway_protocol_convert_to_jsonrpc(
    gateway_protocol_handler_t handler,
    const char* request_data,
    size_t request_size,
    unified_protocol_type_t protocol_type,
    char** jsonrpc_out)
{
    if (!handler || !request_data || !jsonrpc_out) {
        return -1;
    }
    
    struct gateway_protocol_handler_s* h = 
        (struct gateway_protocol_handler_s*)handler;
    
    // 创建统一消息
    unified_message_t source_msg = create_unified_message(
        request_data, request_size, protocol_type);
    
    unified_message_t target_msg;
    memset(&target_msg, 0, sizeof(target_msg));
    
    // 使用协议路由器进行转换
    int result = protocol_router_route(h->router, &source_msg, &target_msg);
    if (result != 0) {
        return -1;
    }
    
    // 提取转换后的数据
    if (target_msg.data && target_msg.data_size > 0) {
        *jsonrpc_out = (char*)malloc(target_msg.data_size + 1);
        if (!*jsonrpc_out) {
            return -1;
        }
        memcpy(*jsonrpc_out, target_msg.data, target_msg.data_size);
        (*jsonrpc_out)[target_msg.data_size] = '\0';
        
        // 清理目标消息资源（假设数据是动态分配的）
        if (target_msg.data) {
            free((void*)target_msg.data);
        }
        if (target_msg.endpoint) {
            free((void*)target_msg.endpoint);
        }
        if (target_msg.metadata) {
            free((void*)target_msg.metadata);
        }
        
        return 0;
    }
    
    return -1;
}

int gateway_protocol_convert_from_jsonrpc(
    gateway_protocol_handler_t handler,
    const char* jsonrpc_response,
    unified_protocol_type_t target_protocol,
    char** target_response)
{
    if (!handler || !jsonrpc_response || !target_response) {
        return -1;
    }
    
    // TODO: 实现JSON-RPC到其他协议的转换
    // 目前简单返回原JSON-RPC响应
    size_t len = strlen(jsonrpc_response);
    *target_response = (char*)malloc(len + 1);
    if (!*target_response) {
        return -1;
    }
    strcpy(*target_response, jsonrpc_response);
    
    return 0;
}

// ============================================================================
// 向后兼容接口实现
// ============================================================================

rpc_result_t gateway_protocol_handle_jsonrpc(
    const cJSON* request,
    int (*handler)(const char*, char**, void*),
    void* handler_data)
{
    // 直接委托给原有的gateway_rpc_handle_request
    return gateway_rpc_handle_request(request, handler, handler_data);
}

// ============================================================================
// 静态函数实现
// ============================================================================

static void init_default_config(gateway_protocol_config_t* config)
{
    if (!config) return;
    
    memset(config, 0, sizeof(*config));
    
    config->enable_mcp_protocol = true;
    config->enable_a2a_protocol = true;
    config->enable_openai_protocol = true;
    config->default_protocol = "jsonrpc";
    config->max_request_size = 10 * 1024 * 1024; // 10MB
    config->enable_protocol_detection = true;
}

static unified_message_t create_unified_message(
    const char* data,
    size_t size,
    unified_protocol_type_t protocol_type)
{
    unified_message_t msg;
    memset(&msg, 0, sizeof(msg));
    
    msg.protocol = protocol_type;
    msg.timestamp = time(NULL);
    
    // 复制数据
    if (data && size > 0) {
        msg.data = (uint8_t*)malloc(size);
        if (msg.data) {
            memcpy((void*)msg.data, data, size);
            msg.data_size = size;
        }
    }
    
    return msg;
}

static int jsonrpc_handler_adapter(const char* request_json,
                                   char** response_json,
                                   void* handler_data)
{
    // 适配器函数，将自定义handler转换为统一接口
    // 这里假设handler_data是一个函数指针
    int (*handler)(const char*, char**, void*) = 
        (int (*)(const char*, char**, void*))handler_data;
    
    if (handler) {
        return handler(request_json, response_json, NULL);
    }
    
    return -1;
}

static int detect_protocol_internal(const char* data, size_t size)
{
    if (!data || size == 0) {
        return 0;
    }
    
    // 简单的协议检测逻辑
    // 1. 检查是否为有效JSON
    // 2. 检查是否包含JSON-RPC特定字段
    // 3. 检查MCP/A2A/OpenAI API特征
    
    // 首先检查是否为JSON
    const char* trimmed_data = data;
    size_t trimmed_size = size;
    
    // 跳过空白字符
    while (trimmed_size > 0 && isspace((unsigned char)*trimmed_data)) {
        trimmed_data++;
        trimmed_size--;
    }
    
    if (trimmed_size == 0) {
        return 0;
    }
    
    // 检查JSON对象开始
    if (*trimmed_data == '{') {
        // 可能是JSON-RPC
        // 简单检查是否包含jsonrpc字段
        const char* jsonrpc_str = "\"jsonrpc\":";
        const char* method_str = "\"method\":";
        
        // 转换为小写进行比较（简化实现）
        char* lower_data = (char*)malloc(trimmed_size + 1);
        if (!lower_data) return 0;
        
        for (size_t i = 0; i < trimmed_size; i++) {
            lower_data[i] = tolower((unsigned char)trimmed_data[i]);
        }
        lower_data[trimmed_size] = '\0';
        
        int has_jsonrpc = (strstr(lower_data, jsonrpc_str) != NULL);
        int has_method = (strstr(lower_data, method_str) != NULL);
        
        free(lower_data);
        
        if (has_jsonrpc && has_method) {
            return 1; // JSON-RPC
        }
        
        // 检查OpenAI API特征
        const char* messages_str = "\"messages\":";
        const char* model_str = "\"model\":";
        
        if (strstr(trimmed_data, messages_str) && strstr(trimmed_data, model_str)) {
            return 4; // OpenAI API
        }
    }
    
    // 检查MCP特征（示例：检查特定的MCP头）
    const char* mcp_magic = "@mcp";
    if (size >= 4 && memcmp(data, mcp_magic, 4) == 0) {
        return 2; // MCP
    }
    
    // 检查A2A特征（示例：检查特定的A2A模式）
    const char* a2a_pattern = "a2a://";
    if (size >= 6 && memcmp(data, a2a_pattern, 6) == 0) {
        return 3; // A2A
    }
    
    return 0; // 未知协议
}