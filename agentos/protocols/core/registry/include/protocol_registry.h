// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file protocol_registry.h
 * @brief Protocol Registry Center - Unified Protocol Discovery & Management
 *
 * 协议注册中心是AgentOS协议系统的核心组件，提供：
 * 1. 协议适配器注册/注销/发现
 * 2. 协议能力查询与匹配
 * 3. 协议版本管理
 * 4. 协议依赖关系追踪
 * 5. 运行时统计与监控
 * 6. 热加载/卸载支持
 *
 * 支持的协议（v2.1.0）:
 * - JSON-RPC 2.0 (原生)
 * - MCP v1.0 (Model Context Protocol)
 * - A2A v0.3 (Agent-to-Agent)
 * - OpenAI API (兼容)
 * - OpenJiuwen (自定义二进制)
 * - OpenClaw (九问平台)
 * - Claude API (Anthropic)
 *
 * @since 2.1.0
 */

#ifndef AGENTOS_PROTOCOL_REGISTRY_H
#define AGENTOS_PROTOCOL_REGISTRY_H

#include "agentos_protocol_interface.h"
#include "unified_protocol.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PROTO_REGISTRY_VERSION       "2.1.0"
#define PROTO_REGISTRY_MAX_ADAPTERS   32
#define PROTO_REGISTRY_MAX_DEPS       16
#define PROTO_REGISTRY_NAME_MAX_LEN   64
#define PROTO_REGISTRY_DESC_MAX_LEN   256

typedef enum {
    PROTO_CAT_CORE = 0,
    PROTO_CAT_STANDARD,
    PROTO_CAT_INTEGRATION,
    PROTO_CAT_FRAMEWORK,
    PROTO_CAT_CUSTOM,
    PROTO_CATEGORY_COUNT
} proto_category_t;

typedef enum {
    PROTO_STATE_UNREGISTERED = 0,
    PROTO_STATE_REGISTERED,
    PROTO_STATE_INITIALIZING,
    PROTO_STATE_READY,
    PROTO_STATE_ACTIVE,
    PROTO_STATE_DEGRADED,
    PROTO_STATE_ERROR,
    PROTO_STATE_SHUTDOWN
} proto_state_t;

typedef struct {
    char name[PROTO_REGISTRY_NAME_MAX_LEN];
    proto_state_t state;
} proto_dependency_t;

typedef struct {
    char name[PROTO_REGISTRY_NAME_MAX_LEN];
    char version[32];
    char description[PROTO_REGISTRY_DESC_MAX_LEN];
    proto_category_t category;
    proto_type_t type;
    uint32_t capabilities;
    proto_state_t state;
    const protocol_adapter_t* adapter;
    void* context;
    proto_dependency_t dependencies[PROTO_REGISTRY_MAX_DEPS];
    size_t dependency_count;
    uint64_t registered_at;
    uint64_t activated_at;
    uint64_t last_heartbeat;
    uint64_t request_count;
    uint64_t error_count;
    double avg_latency_ms;
    bool is_builtin;
} proto_registry_entry_t;

typedef struct {
    size_t total_entries;
    size_t active_entries;
    size_t builtin_entries;
    size_t custom_entries;
    uint64_t total_requests;
    uint64_t total_errors;
    double total_uptime_sec;
    double avg_system_latency_ms;
} proto_registry_stats_t;

typedef struct protocol_registry_s protocol_registry_t;

typedef int (*proto_registry_event_fn)(const char* event_name,
                                        const char* entry_name,
                                        proto_state_t old_state,
                                        proto_state_t new_state,
                                        void* user_data);

protocol_registry_t* proto_registry_create(void);
void proto_registry_destroy(protocol_registry_t* registry);

const char* proto_registry_version(void);

int proto_registry_register(protocol_registry_t* registry,
                            const char* name,
                            const char* version,
                            const char* description,
                            proto_category_t category,
                            proto_type_t type,
                            uint32_t capabilities,
                            const protocol_adapter_t* adapter,
                            void* context);

int proto_registry_unregister(protocol_registry_t* registry,
                              const char* name);

proto_registry_entry_t* proto_registry_find(protocol_registry_t* registry,
                                            const char* name);

proto_registry_entry_t* proto_registry_find_by_type(protocol_registry_t* registry,
                                                    proto_type_t type);

proto_registry_entry_t* proto_registry_find_by_capability(protocol_registry_t* registry,
                                                          uint32_t required_caps);

size_t proto_registry_list_all(protocol_registry_t* registry,
                               proto_registry_entry_t** entries);

size_t proto_registry_list_by_category(protocol_registry_t* registry,
                                       proto_category_t category,
                                       proto_registry_entry_t** entries);

size_t proto_registry_list_active(protocol_registry_t* registry,
                                  proto_registry_entry_t** entries);

int proto_registry_set_state(protocol_registry_t* registry,
                             const char* name,
                             proto_state_t state);

int proto_registry_add_dependency(protocol_registry_t* registry,
                                  const char* name,
                                  const char* dep_name);

bool proto_registry_check_dependencies(const proto_registry_entry_t* entry);

int proto_registry_activate(protocol_registry_t* registry,
                            const char* name);

int proto_registry_deactivate(protocol_registry_t* registry,
                              const char* name);

int proto_registry_heartbeat(protocol_registry_t* registry,
                             const char* name);

int proto_registry_record_request(protocol_registry_t* registry,
                                  const char* name,
                                  bool success,
                                  double latency_ms);

int proto_registry_get_statistics(protocol_registry_t* registry,
                                  proto_registry_stats_t* stats);

int proto_registry_export_json(protocol_registry_t* registry,
                                char* json_buffer,
                                size_t buffer_size);

int proto_registry_set_event_callback(protocol_registry_t* registry,
                                      proto_registry_event_fn callback,
                                      void* user_data);

int proto_registry_initialize_builtins(protocol_registry_t* registry);

const char* proto_category_to_string(proto_category_t cat);
const char* proto_state_to_string(proto_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_PROTOCOL_REGISTRY_H */
