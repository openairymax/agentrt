// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file protocol_extension_framework.h
 * @brief Protocol Extension Framework for AgentOS
 *
 * 自定义协议扩展框架，支持第三方开发者创建和注册自定义协议适配器。
 * 提供协议生命周期管理、消息转换管道、能力声明与发现等扩展机制。
 *
 * 核心设计:
 * 1. 插件式协议注册 — 动态加载/卸载协议适配器
 * 2. 消息转换管道 — 可组合的消息处理中间件链
 * 3. 能力声明与发现 — 协议能力自动注册与查询
 * 4. 协议协商 — 客户端-服务端协议版本协商
 * 5. 热加载 — 运行时动态添加协议支持
 *
 * @since 2.0.0
 * @see unified_protocol.h
 */

#ifndef AGENTOS_PROTOCOL_EXTENSION_FRAMEWORK_H
#define AGENTOS_PROTOCOL_EXTENSION_FRAMEWORK_H

#include "unified_protocol.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PROTO_EXT_MAX_ADAPTERS       64
#define PROTO_EXT_MAX_MIDDLEWARE     32
#define PROTO_EXT_MAX_CAPABILITIES   128
#define PROTO_EXT_MAX_NAME_LEN       64
#define PROTO_EXT_MAX_VERSION_LEN    32

typedef enum {
    PROTO_EXT_STATE_UNLOADED = 0,
    PROTO_EXT_STATE_LOADED,
    PROTO_EXT_STATE_INITIALIZED,
    PROTO_EXT_STATE_RUNNING,
    PROTO_EXT_STATE_ERROR,
    PROTO_EXT_STATE_DISABLED
} proto_ext_state_t;

typedef enum {
    PROTO_EXT_PRIORITY_LOWEST = 0,
    PROTO_EXT_PRIORITY_LOW = 25,
    PROTO_EXT_PRIORITY_NORMAL = 50,
    PROTO_EXT_PRIORITY_HIGH = 75,
    PROTO_EXT_PRIORITY_HIGHEST = 100
} proto_ext_priority_t;

typedef struct {
    char name[PROTO_EXT_MAX_NAME_LEN];
    char version[PROTO_EXT_MAX_VERSION_LEN];
    char description[256];
    char author[128];
    protocol_type_t protocol_type;
    uint32_t capabilities;
    proto_ext_priority_t priority;
    bool hot_loadable;
    bool requires_auth;
} proto_ext_descriptor_t;

typedef struct {
    int (*on_load)(void** adapter_context);
    int (*on_init)(void* adapter_context, const char* config_json);
    int (*on_start)(void* adapter_context);
    int (*on_stop)(void* adapter_context);
    void (*on_unload)(void* adapter_context);

    int (*encode_message)(void* adapter_context,
                           const unified_message_t* in_msg,
                           void** out_data,
                           size_t* out_size);

    int (*decode_message)(void* adapter_context,
                           const void* in_data,
                           size_t in_size,
                           unified_message_t* out_msg);

    int (*handle_request)(void* adapter_context,
                           const char* method,
                           const char* params_json,
                           char** response_json);

    int (*negotiate_version)(void* adapter_context,
                              const char* client_version,
                              char** agreed_version);

    void (*get_capabilities)(void* adapter_context,
                              uint32_t* caps,
                              char** caps_json);

    void (*on_error)(void* adapter_context,
                      int error_code,
                      const char* error_message);
} proto_ext_callbacks_t;

typedef int (*proto_middleware_fn)(const unified_message_t* request,
                                   unified_message_t* response,
                                   void* user_data);

typedef struct {
    char name[PROTO_EXT_MAX_NAME_LEN];
    proto_middleware_fn process;
    proto_ext_priority_t priority;
    void* user_data;
    bool enabled;
} proto_middleware_t;

typedef struct proto_ext_framework_s proto_ext_framework_t;

typedef struct {
    char name[PROTO_EXT_MAX_NAME_LEN];
    proto_ext_state_t state;
    uint32_t error_count;
    uint64_t last_activity_ms;
    uint64_t messages_processed;
} proto_ext_stats_t;

proto_ext_framework_t* proto_ext_framework_create(void);
void proto_ext_framework_destroy(proto_ext_framework_t* fw);

int proto_ext_register(proto_ext_framework_t* fw,
                         const proto_ext_descriptor_t* descriptor,
                         const proto_ext_callbacks_t* callbacks);

int proto_ext_unregister(proto_ext_framework_t* fw, const char* name);

int proto_ext_load(proto_ext_framework_t* fw, const char* name, const char* config_json);
int proto_ext_unload(proto_ext_framework_t* fw, const char* name);

int proto_ext_start(proto_ext_framework_t* fw, const char* name);
int proto_ext_stop(proto_ext_framework_t* fw, const char* name);

int proto_ext_send_message(proto_ext_framework_t* fw,
                             const char* adapter_name,
                             const unified_message_t* message);

int proto_ext_handle_request(proto_ext_framework_t* fw,
                               const char* adapter_name,
                               const char* method,
                               const char* params_json,
                               char** response_json);

int proto_ext_auto_route(proto_ext_framework_t* fw,
                           const unified_message_t* message,
                           char** adapter_name);

int proto_ext_negotiate(proto_ext_framework_t* fw,
                          const char* adapter_name,
                          const char* client_version,
                          char** agreed_version);

int proto_ext_add_middleware(proto_ext_framework_t* fw,
                               const char* name,
                               proto_middleware_fn middleware,
                               proto_ext_priority_t priority,
                               void* user_data);

int proto_ext_remove_middleware(proto_ext_framework_t* fw, const char* name);

int proto_ext_enable_middleware(proto_ext_framework_t* fw, const char* name);
int proto_ext_disable_middleware(proto_ext_framework_t* fw, const char* name);

int proto_ext_process_middleware_chain(proto_ext_framework_t* fw,
                                        const unified_message_t* request,
                                        unified_message_t* response);

int proto_ext_get_adapter_stats(proto_ext_framework_t* fw,
                                  const char* name,
                                  proto_ext_stats_t* stats);

int proto_ext_list_adapters(proto_ext_framework_t* fw,
                              char** names_json);

int proto_ext_list_capabilities(proto_ext_framework_t* fw,
                                  char** caps_json);

int proto_ext_find_by_capability(proto_ext_framework_t* fw,
                                   uint32_t capability,
                                   char*** adapter_names,
                                   size_t* count);

proto_ext_state_t proto_ext_get_state(proto_ext_framework_t* fw, const char* name);

int proto_ext_load_from_config(proto_ext_framework_t* fw,
                                 const char* config_json);

const protocol_adapter_t* proto_ext_get_framework_adapter(void);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_PROTOCOL_EXTENSION_FRAMEWORK_H */
