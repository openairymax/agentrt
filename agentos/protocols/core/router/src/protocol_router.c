// SPDX-FileCopyrightText: 2026 SPHARX.
// SPDX-License-Identifier: Apache-2.0
/**
 * @file protocol_router.c
 * @brief Protocol Routing and Transformation Engine Implementation
 * 
 * 协议路由与转换引擎实现，支持MCP/A2A/OpenAI API等协议的自适应路由和转换。
 */

#include "protocol_router.h"
#include "protocol_transformers.h"
#include <agentos/safe_string_utils.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// ============================================================================
// 内部数据结构
// ============================================================================

typedef struct rule_node_s {
    protocol_rule_t rule;
    message_transformer_t transformer;
    struct rule_node_s* next;
} rule_node_t;

struct protocol_router_s {
    protocol_type_t default_protocol;
    rule_node_t* rules;
    size_t rule_count;
    route_decision_func_t decision_func;
    
    // 统计信息
    uint64_t total_messages_routed;
    uint64_t messages_routed_success;
    uint64_t messages_routed_failed;
    uint64_t total_conversion_time_ns;
};

// ============================================================================
// 静态函数声明
// ============================================================================

static rule_node_t* create_rule_node(const protocol_rule_t* rule,
                                     message_transformer_t transformer);
static void destroy_rule_node(rule_node_t* node);
static int match_endpoint(const char* pattern, const char* endpoint);
static int default_decision_func(const unified_message_t* message,
                                 const protocol_rule_t* rules,
                                 size_t rule_count);

// ============================================================================
// 核心API实现
// ============================================================================

protocol_router_handle_t protocol_router_create(protocol_type_t default_protocol)
{
    struct protocol_router_s* router = (struct protocol_router_s*)calloc(1, sizeof(struct protocol_router_s));
    if (!router) {
        return NULL;
    }
    
    router->default_protocol = default_protocol;
    router->rules = NULL;
    router->rule_count = 0;
    router->decision_func = default_decision_func;
    
    router->total_messages_routed = 0;
    router->messages_routed_success = 0;
    router->messages_routed_failed = 0;
    router->total_conversion_time_ns = 0;
    
    return router;
}

void protocol_router_destroy(protocol_router_handle_t router)
{
    if (!router) return;
    
    struct protocol_router_s* r = (struct protocol_router_s*)router;
    
    // 销毁所有规则节点
    rule_node_t* node = r->rules;
    while (node) {
        rule_node_t* next = node->next;
        destroy_rule_node(node);
        node = next;
    }
    
    free(r);
}

int protocol_router_add_rule(protocol_router_handle_t router,
                             const protocol_rule_t* rule,
                             message_transformer_t transformer)
{
    if (!router || !rule) {
        return -1;
    }
    
    struct protocol_router_s* r = (struct protocol_router_s*)router;
    
    // 创建规则节点
    rule_node_t* node = create_rule_node(rule, transformer);
    if (!node) {
        return -1;
    }
    
    // 添加到链表（按优先级排序）
    rule_node_t** pp = &r->rules;
    while (*pp && (*pp)->rule.priority <= rule->priority) {
        pp = &(*pp)->next;
    }
    
    node->next = *pp;
    *pp = node;
    r->rule_count++;
    
    return 0;
}

int protocol_router_route(protocol_router_handle_t router,
                          const unified_message_t* message,
                          unified_message_t* transformed)
{
    if (!router || !message || !transformed) {
        return -1;
    }
    
    struct protocol_router_s* r = (struct protocol_router_s*)router;
    r->total_messages_routed++;
    
    uint64_t start_time = 0;
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        start_time = (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
    }
    
    // 收集所有规则到临时数组供决策函数使用
    protocol_rule_t* rule_array = NULL;
    if (r->rule_count > 0) {
        rule_array = (protocol_rule_t*)malloc(r->rule_count * sizeof(protocol_rule_t));
        if (rule_array) {
            rule_node_t* node = r->rules;
            size_t i = 0;
            while (node && i < r->rule_count) {
                rule_array[i] = node->rule;
                node = node->next;
                i++;
            }
        }
    }
    
    // 使用决策函数选择规则
    int rule_index = -1;
    if (r->decision_func && rule_array) {
        rule_index = r->decision_func(message, rule_array, r->rule_count);
    }
    
    // 查找匹配的规则节点
    rule_node_t* matched_node = NULL;
    if (rule_index >= 0 && rule_index < (int)r->rule_count) {
        rule_node_t* node = r->rules;
        for (int i = 0; node && i < rule_index; i++) {
            node = node->next;
        }
        if (node) {
            matched_node = node;
        }
    }
    
    // 如果没有匹配规则，使用默认协议
    if (!matched_node) {
        // 直接复制消息，只修改协议类型为默认协议
        *transformed = *message;
        transformed->protocol = r->default_protocol;
        
        if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
            uint64_t end_time = (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
            if (start_time > 0) {
                r->total_conversion_time_ns += (end_time - start_time);
            }
        }
        
        r->messages_routed_success++;
        if (rule_array) free(rule_array);
        return 0;
    }
    
    // 应用转换器
    message_transformer_t transformer = matched_node->transformer;
    if (!transformer) {
        transformer = protocol_transformer_default;
    }
    
    int result = transformer(message, transformed, matched_node->rule.transformer_context);
    
    // 更新目标协议和端点（如果转换器未设置）
    if (result == 0) {
        if (transformed->protocol == PROTOCOL_CUSTOM) {
            transformed->protocol = matched_node->rule.target_protocol;
        }
        if (!transformed->endpoint && matched_node->rule.target_endpoint) {
            transformed->endpoint = matched_node->rule.target_endpoint;
        }
    }
    
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        uint64_t end_time = (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
        if (start_time > 0) {
            r->total_conversion_time_ns += (end_time - start_time);
        }
    }
    
    if (result == 0) {
        r->messages_routed_success++;
    } else {
        r->messages_routed_failed++;
    }
    
    if (rule_array) free(rule_array);
    return result;
}

int protocol_router_route_batch(protocol_router_handle_t router,
                                const unified_message_t* messages,
                                size_t count,
                                unified_message_t* transformed)
{
    if (!router || !messages || !transformed || count == 0) {
        return -1;
    }
    
    int success_count = 0;
    for (size_t i = 0; i < count; i++) {
        int result = protocol_router_route(router, &messages[i], &transformed[i]);
        if (result == 0) {
            success_count++;
        }
    }
    
    return success_count;
}

int protocol_router_set_decision_func(protocol_router_handle_t router,
                                      route_decision_func_t decision_func)
{
    if (!router) {
        return -1;
    }
    
    struct protocol_router_s* r = (struct protocol_router_s*)router;
    r->decision_func = decision_func ? decision_func : default_decision_func;
    
    return 0;
}

int protocol_router_get_stats(protocol_router_handle_t router,
                              char** stats_json)
{
    if (!router || !stats_json) {
        return -1;
    }
    
    struct protocol_router_s* r = (struct protocol_router_s*)router;
    
    // 构建简单的JSON统计信息
    const char* fmt = 
        "{"
        "\"total_messages_routed\": %llu,"
        "\"messages_routed_success\": %llu,"
        "\"messages_routed_failed\": %llu,"
        "\"average_conversion_time_ns\": %llu,"
        "\"rule_count\": %zu"
        "}";
    
    uint64_t avg_time = 0;
    if (r->total_messages_routed > 0) {
        avg_time = r->total_conversion_time_ns / r->total_messages_routed;
    }
    
    size_t buf_size = snprintf(NULL, 0, fmt,
                               r->total_messages_routed,
                               r->messages_routed_success,
                               r->messages_routed_failed,
                               avg_time,
                               r->rule_count) + 1;
    
    char* buf = (char*)malloc(buf_size);
    if (!buf) {
        return -1;
    }
    
    snprintf(buf, buf_size, fmt,
             r->total_messages_routed,
             r->messages_routed_success,
             r->messages_routed_failed,
             avg_time,
             r->rule_count);
    
    *stats_json = buf;
    return 0;
}

// ============================================================================
// 预定义转换器实现
// ============================================================================

int protocol_transformer_jsonrpc_to_mcp(const unified_message_t* source,
                                        unified_message_t* target,
                                        void* context)
{
    return transformer_jsonrpc_to_mcp_request(source, target, context);
}

int protocol_transformer_mcp_to_jsonrpc(const unified_message_t* source,
                                        unified_message_t* target,
                                        void* context)
{
    return transformer_mcp_to_jsonrpc_response(source, target, context);
}

int protocol_transformer_openai_to_jsonrpc(const unified_message_t* source,
                                           unified_message_t* target,
                                           void* context)
{
    return transformer_openai_chat_to_jsonrpc(source, target, context);
}

int protocol_transformer_a2a_to_jsonrpc(const unified_message_t* source,
                                        unified_message_t* target,
                                        void* context)
{
    return transformer_a2a_to_jsonrpc_response(source, target, context);
}

int protocol_transformer_default(const unified_message_t* source,
                                 unified_message_t* target,
                                 void* context)
{
    if (!source || !target) {
        return -1;
    }
    
    *target = *source;
    return 0;
}

// ============================================================================
// 静态函数实现
// ============================================================================

static rule_node_t* create_rule_node(const protocol_rule_t* rule,
                                     message_transformer_t transformer)
{
    rule_node_t* node = (rule_node_t*)calloc(1, sizeof(rule_node_t));
    if (!node) {
        return NULL;
    }
    
    node->rule = *rule;
    
    // 复制字符串字段
    if (rule->source_endpoint) {
        size_t len = strlen(rule->source_endpoint) + 1;
        char* copy = (char*)malloc(len);
        if (copy) {
            safe_strcpy(copy, rule->source_endpoint, len);
            node->rule.source_endpoint = copy;
        }
    }

    if (rule->target_endpoint) {
        size_t len = strlen(rule->target_endpoint) + 1;
        char* copy = (char*)malloc(len);
        if (copy) {
            safe_strcpy(copy, rule->target_endpoint, len);
            node->rule.target_endpoint = copy;
        }
    }
    
    node->transformer = transformer;
    node->next = NULL;
    
    return node;
}

static void destroy_rule_node(rule_node_t* node)
{
    if (!node) return;
    
    if (node->rule.source_endpoint) {
        free((void*)node->rule.source_endpoint);
    }
    
    if (node->rule.target_endpoint) {
        free((void*)node->rule.target_endpoint);
    }
    
    if (node->rule.transformer_context) {
        free(node->rule.transformer_context);
    }
    
    free(node);
}

static int match_endpoint(const char* pattern, const char* endpoint)
{
    if (!pattern || !endpoint) {
        return 0;
    }
    
    // 简单实现：支持通配符*和?
    // TODO: 实现更复杂的模式匹配
    return (strcmp(pattern, "*") == 0 || strcmp(pattern, endpoint) == 0);
}

static int default_decision_func(const unified_message_t* message,
                                 const protocol_rule_t* rules,
                                 size_t rule_count)
{
    if (!message || !rules || rule_count == 0) {
        return -1;
    }
    
    // 简单决策：按顺序匹配第一个符合的规则
    for (size_t i = 0; i < rule_count; i++) {
        const protocol_rule_t* rule = &rules[i];
        
        // 检查协议匹配
        if (rule->source_protocol != PROTOCOL_CUSTOM && 
            rule->source_protocol != message->protocol) {
            continue;
        }
        
        // 检查端点匹配
        if (rule->source_endpoint && message->endpoint) {
            if (!match_endpoint(rule->source_endpoint, message->endpoint)) {
                continue;
            }
        }
        
        return (int)i; // 匹配成功
    }
    
    return -1; // 无匹配
}