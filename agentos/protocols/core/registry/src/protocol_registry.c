// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file protocol_registry.c
 * @brief Protocol Registry Center Implementation
 */

#define LOG_TAG "protocol_registry"

#include "protocol_registry.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

struct protocol_registry_s {
    proto_registry_entry_t entries[PROTO_REGISTRY_MAX_ADAPTERS];
    size_t entry_count;
    proto_registry_event_fn event_callback;
    void* event_callback_data;
    bool initialized;
};

static protocol_registry_t* g_registry = NULL;

const char* proto_category_to_string(proto_category_t cat) {
    static const char* names[] = {
        "core", "standard", "integration", "framework", "custom"
    };
    if (cat >= 0 && cat < PROTO_CATEGORY_COUNT) return names[cat];
    return "unknown";
}

const char* proto_state_to_string(proto_state_t state) {
    static const char* names[] = {
        "unregistered", "registered", "initializing",
        "ready", "active", "degraded", "error", "shutdown"
    };
    if (state >= 0 && state <= PROTO_STATE_SHUTDOWN) return names[state];
    return "unknown";
}

protocol_registry_t* proto_registry_create(void) {
    protocol_registry_t* registry = (protocol_registry_t*)calloc(1, sizeof(protocol_registry_t));
    if (!registry) return NULL;

    registry->entry_count = 0;
    registry->event_callback = NULL;
    registry->event_callback_data = NULL;
    registry->initialized = true;

    g_registry = registry;
    return registry;
}

void proto_registry_destroy(protocol_registry_t* registry) {
    if (!registry) return;

    for (size_t i = 0; i < registry->entry_count; i++) {
        memset(&registry->entries[i], 0, sizeof(proto_registry_entry_t));
    }
    registry->entry_count = 0;

    if (registry == g_registry)
        g_registry = NULL;

    free(registry);
}

const char* proto_registry_version(void) {
    return PROTO_REGISTRY_VERSION;
}

int proto_registry_register(protocol_registry_t* registry,
                            const char* name,
                            const char* version,
                            const char* description,
                            proto_category_t category,
                            proto_type_t type,
                            uint32_t capabilities,
                            const protocol_adapter_t* adapter,
                            void* context) {
    if (!registry || !name) return -1;
    if (registry->entry_count >= PROTO_REGISTRY_MAX_ADAPTERS) return -2;

    for (size_t i = 0; i < registry->entry_count; i++) {
        if (strncmp(registry->entries[i].name, name, PROTO_REGISTRY_NAME_MAX_LEN - 1) == 0)
            return -3;
    }

    proto_registry_entry_t* entry = &registry->entries[registry->entry_count];
    memset(entry, 0, sizeof(proto_registry_entry_t));

    snprintf(entry->name, PROTO_REGISTRY_NAME_MAX_LEN, "%s", name);
    snprintf(entry->version, sizeof(entry->version), "%s", version ? version : "1.0.0");
    if (description)
        snprintf(entry->description, PROTO_REGISTRY_DESC_MAX_LEN, "%s", description);
    entry->category = category;
    entry->type = type;
    entry->capabilities = capabilities;
    entry->state = PROTO_STATE_REGISTERED;
    entry->adapter = adapter;
    entry->context = context;
    entry->dependency_count = 0;
    entry->registered_at = (uint64_t)(time(NULL));
    entry->request_count = 0;
    entry->error_count = 0;
    entry->avg_latency_ms = 0.0;
    entry->is_builtin = false;

    registry->entry_count++;

    if (registry->event_callback) {
        registry->event_callback("register", name,
                                PROTO_STATE_UNREGISTERED, PROTO_STATE_REGISTERED,
                                registry->event_callback_data);
    }

    return 0;
}

int proto_registry_unregister(protocol_registry_t* registry, const char* name) {
    if (!registry || !name) return -1;

    for (size_t i = 0; i < registry->entry_count; i++) {
        if (strncmp(registry->entries[i].name, name, PROTO_REGISTRY_NAME_MAX_LEN - 1) == 0) {
            proto_state_t old_state = registry->entries[i].state;

            if (i < registry->entry_count - 1) {
                memmove(&registry->entries[i],
                        &registry->entries[i + 1],
                        (registry->entry_count - i - 1) * sizeof(proto_registry_entry_t));
            }
            registry->entry_count--;

            if (registry->event_callback) {
                registry->event_callback("unregister", name, old_state, PROTO_STATE_UNREGISTERED,
                                        registry->event_callback_data);
            }

            return 0;
        }
    }
    return -4;
}

proto_registry_entry_t* proto_registry_find(protocol_registry_t* registry, const char* name) {
    if (!registry || !name) return NULL;

    for (size_t i = 0; i < registry->entry_count; i++) {
        if (strncmp(registry->entries[i].name, name, PROTO_REGISTRY_NAME_MAX_LEN - 1) == 0)
            return &registry->entries[i];
    }
    return NULL;
}

proto_registry_entry_t* proto_registry_find_by_type(protocol_registry_t* registry,
                                                    proto_type_t type) {
    if (!registry) return NULL;

    for (size_t i = 0; i < registry->entry_count; i++) {
        if (registry->entries[i].type == type &&
            (registry->entries[i].state == PROTO_STATE_ACTIVE ||
             registry->entries[i].state == PROTO_STATE_READY))
            return &registry->entries[i];
    }
    return NULL;
}

proto_registry_entry_t* proto_registry_find_by_capability(protocol_registry_t* registry,
                                                          uint32_t required_caps) {
    if (!registry) return NULL;

    for (size_t i = 0; i < registry->entry_count; i++) {
        if ((registry->entries[i].capabilities & required_caps) == required_caps &&
            (registry->entries[i].state == PROTO_STATE_ACTIVE ||
             registry->entries[i].state == PROTO_STATE_READY))
            return &registry->entries[i];
    }
    return NULL;
}

size_t proto_registry_list_all(protocol_registry_t* registry,
                               proto_registry_entry_t** entries) {
    if (!registry || !entries) return 0;

    *entries = (proto_registry_entry_t*)calloc(registry->entry_count, sizeof(proto_registry_entry_t));
    if (!*entries && registry->entry_count > 0) return 0;

    memcpy(*entries, registry->entries, registry->entry_count * sizeof(proto_registry_entry_t));
    return registry->entry_count;
}

size_t proto_registry_list_by_category(protocol_registry_t* registry,
                                       proto_category_t category,
                                       proto_registry_entry_t** entries) {
    if (!registry || !entries) return 0;

    size_t count = 0;
    for (size_t i = 0; i < registry->entry_count; i++) {
        if (registry->entries[i].category == category) count++;
    }

    if (count == 0) { *entries = NULL; return 0; }

    *entries = (proto_registry_entry_t*)calloc(count, sizeof(proto_registry_entry_t));
    if (!*entries) return 0;

    size_t idx = 0;
    for (size_t i = 0; i < registry->entry_count && idx < count; i++) {
        if (registry->entries[i].category == category) {
            (*entries)[idx++] = registry->entries[i];
        }
    }
    return idx;
}

size_t proto_registry_list_active(protocol_registry_t* registry,
                                  proto_registry_entry_t** entries) {
    if (!registry || !entries) return 0;

    size_t count = 0;
    for (size_t i = 0; i < registry->entry_count; i++) {
        if (registry->entries[i].state == PROTO_STATE_ACTIVE ||
            registry->entries[i].state == PROTO_STATE_READY)
            count++;
    }

    if (count == 0) { *entries = NULL; return 0; }

    *entries = (proto_registry_entry_t*)calloc(count, sizeof(proto_registry_entry_t));
    if (!*entries) return 0;

    size_t idx = 0;
    for (size_t i = 0; i < registry->entry_count && idx < count; i++) {
        if (registry->entries[i].state == PROTO_STATE_ACTIVE ||
            registry->entries[i].state == PROTO_STATE_READY) {
            (*entries)[idx++] = registry->entries[i];
        }
    }
    return idx;
}

int proto_registry_set_state(protocol_registry_t* registry,
                             const char* name,
                             proto_state_t state) {
    if (!registry || !name) return -1;

    proto_registry_entry_t* entry = proto_registry_find(registry, name);
    if (!entry) return -4;

    proto_state_t old_state = entry->state;
    entry->state = state;

    if (state == PROTO_STATE_ACTIVE)
        entry->activated_at = (uint64_t)(time(NULL));

    if (registry->event_callback) {
        registry->event_callback("state_change", name, old_state, state,
                                registry->event_callback_data);
    }

    return 0;
}

int proto_registry_add_dependency(protocol_registry_t* registry,
                                  const char* name,
                                  const char* dep_name) {
    if (!registry || !name || !dep_name) return -1;

    proto_registry_entry_t* entry = proto_registry_find(registry, name);
    if (!entry) return -4;

    if (entry->dependency_count >= PROTO_REGISTRY_MAX_DEPS) return -5;

    snprintf(entry->dependencies[entry->dependency_count].name,
             PROTO_REGISTRY_NAME_MAX_LEN, "%s", dep_name);
    entry->dependencies[entry->dependency_count].state = PROTO_STATE_UNREGISTERED;
    entry->dependency_count++;

    return 0;
}

bool proto_registry_check_dependencies(const proto_registry_entry_t* entry) {
    if (!entry) return false;

    for (size_t i = 0; i < entry->dependency_count; i++) {
        if (entry->dependencies[i].state != PROTO_STATE_ACTIVE &&
            entry->dependencies[i].state != PROTO_STATE_READY)
            return false;
    }
    return true;
}

int proto_registry_activate(protocol_registry_t* registry, const char* name) {
    if (!registry || !name) return -1;

    proto_registry_entry_t* entry = proto_registry_find(registry, name);
    if (!entry) return -4;

    if (!proto_registry_check_dependencies(entry)) {
        proto_registry_set_state(registry, name, PROTO_STATE_ERROR);
        return -6;
    }

    return proto_registry_set_state(registry, name, PROTO_STATE_ACTIVE);
}

int proto_registry_deactivate(protocol_registry_t* registry, const char* name) {
    if (!registry || !name) return -1;

    proto_registry_entry_t* entry = proto_registry_find(registry, name);
    if (!entry) return -4;

    return proto_registry_set_state(registry, name, PROTO_STATE_READY);
}

int proto_registry_heartbeat(protocol_registry_t* registry, const char* name) {
    if (!registry || !name) return -1;

    proto_registry_entry_t* entry = proto_registry_find(registry, name);
    if (!entry) return -4;

    entry->last_heartbeat = (uint64_t)(time(NULL));

    for (size_t i = 0; i < registry->entry_count; i++) {
        for (size_t j = 0; j < registry->entries[i].dependency_count; j++) {
            if (strncmp(registry->entries[i].dependencies[j].name, name,
                       PROTO_REGISTRY_NAME_MAX_LEN - 1) == 0) {
                registry->entries[i].dependencies[j].state = entry->state;
            }
        }
    }

    return 0;
}

int proto_registry_record_request(protocol_registry_t* registry,
                                  const char* name,
                                  bool success,
                                  double latency_ms) {
    if (!registry || !name) return -1;

    proto_registry_entry_t* entry = proto_registry_find(registry, name);
    if (!entry) return -4;

    entry->request_count++;
    if (!success) entry->error_count++;

    double total_latency = entry->avg_latency_ms * (double)(entry->request_count - 1) + latency_ms;
    entry->avg_latency_ms = total_latency / (double)entry->request_count;

    return 0;
}

int proto_registry_get_statistics(protocol_registry_t* registry,
                                  proto_registry_stats_t* stats) {
    if (!registry || !stats) return -1;

    memset(stats, 0, sizeof(proto_registry_stats_t));

    stats->total_entries = registry->entry_count;
    stats->total_uptime_sec = 0.0;

    for (size_t i = 0; i < registry->entry_count; i++) {
        const proto_registry_entry_t* e = &registry->entries[i];
        if (e->state == PROTO_STATE_ACTIVE || e->state == PROTO_STATE_READY)
            stats->active_entries++;
        if (e->is_builtin) stats->builtin_entries++;
        else stats->custom_entries++;
        stats->total_requests += e->request_count;
        stats->total_errors += e->error_count;
        if (e->activated_at > 0) {
            double uptime = difftime(time(NULL), (time_t)e->activated_at);
            if (uptime > stats->total_uptime_sec)
                stats->total_uptime_sec = uptime;
        }
        stats->avg_system_latency_ms += e->avg_latency_ms;
    }

    if (stats->active_entries > 0)
        stats->avg_system_latency_ms /= (double)stats->active_entries;

    return 0;
}

int proto_registry_export_json(protocol_registry_t* registry,
                                char* json_buffer,
                                size_t buffer_size) {
    if (!registry || !json_buffer || buffer_size < 128) return -1;

    int pos = 0;
    pos += snprintf(json_buffer + pos, buffer_size - pos,
        "{\"registry_version\":\"%s\",\"total\":%zu,\"entries\":[",
        PROTO_REGISTRY_VERSION, registry->entry_count);

    for (size_t i = 0; i < registry->entry_count && (size_t)pos < buffer_size - 200; i++) {
        const proto_registry_entry_t* e = &registry->entries[i];
        if (i > 0) pos += snprintf(json_buffer + pos, buffer_size - pos, ",");
        pos += snprintf(json_buffer + pos, buffer_size - pos,
            "{\"name\":\"%s\",\"ver\":\"%s\",\"cat\":\"%s\","
            "\"type\":%d,\"caps\":0x%08x,\"state\":\"%s\","
            "\"reqs\":%llu,\"errs\":%llu,\"latency\":%.2f}",
            e->name, e->version, proto_category_to_string(e->category),
            (int)e->type, e->capabilities, proto_state_to_string(e->state),
            (unsigned long long)e->request_count, (unsigned long long)e->error_count,
            e->avg_latency_ms);
    }

    pos += snprintf(json_buffer + pos, buffer_size - pos, "]}");

    return (pos >= 0 && (size_t)pos < buffer_size) ? 0 : -2;
}

int proto_registry_set_event_callback(protocol_registry_t* registry,
                                      proto_registry_event_fn callback,
                                      void* user_data) {
    if (!registry) return -1;
    registry->event_callback = callback;
    registry->event_callback_data = user_data;
    return 0;
}

int proto_registry_initialize_builtins(protocol_registry_t* registry) {
    if (!registry) return -1;

    struct builtin_def {
        const char* name;
        const char* version;
        const char* desc;
        proto_category_t cat;
        proto_type_t type;
        uint32_t caps;
    };

    static const struct builtin_def builtins[] = {
        {"JSON-RPC",   "2.0",  "原生JSON-RPC 2.0协议适配器",     PROTO_CAT_CORE,         PROTO_JSONRPC,  PROTO_CAP_STREAMING | PROTO_CAP_BATCH},
        {"MCP",        "1.0",  "Model Context Protocol v1.0",     PROTO_CAT_STANDARD,     PROTO_MCP,      PROTO_CAP_TOOL_CALLING | PROTO_CAP_STREAMING | PROTO_CAP_RESOURCE_ACCESS},
        {"A2A",        "0.3",  "Agent-to-Agent Protocol v0.3",     PROTO_CAT_STANDARD,     PROTO_A2A,      PROTO_CAP_AGENT_DISCOVERY | PROTO_CAP_STREAMING | PROTO_CAP_CONSENSUS},
        {"OpenAI",     "1.0",  "OpenAI API兼容适配器",              PROTO_CAT_INTEGRATION,  PROTO_OPENAI,    PROTO_CAP_STREAMING | PROTO_CAP_TOOL_CALLING | PROTO_CAP_EMBEDDINGS},
        {"OpenJiuwen", "1.0",  "OpenJiuwen自定义二进制协议",       PROTO_CAT_INTEGRATION,  PROTO_OPENJIUWEN,PROTO_CAP_BINARY | PROTO_CAP_LOW_LATENCY | PROTO_CAP_CRC_CHECKSUM},
        {"OpenClaw",   "1.0",  "OpenClaw九问平台集成适配器",       PROTO_CAT_INTEGRATION,  PROTO_OPENCLAW,  PROTO_CAP_MULTIMODAL | PROTO_CAP_STREAMING | PROTO_CAP_AGENT_DISCOVERY | PROTO_CAP_TOOL_CALLING},
        {"Claude",     "1.0",  "Anthropic Claude API适配器",        PROTO_CAT_INTEGRATION,  PROTO_CLAUDE,    PROTO_CAP_STREAMING | PROTO_CAP_TOOL_CALLING | PROTO_CAP_VISION | PROTO_CAP_EXTENDED_THINKING},
    };

    static const int builtin_count = (int)(sizeof(builtins) / sizeof(builtins[0]));

    for (int i = 0; i < builtin_count; i++) {
        int ret = proto_registry_register(
            registry,
            builtins[i].name,
            builtins[i].version,
            builtins[i].desc,
            builtins[i].cat,
            builtins[i].type,
            builtins[i].caps,
            NULL,
            NULL
        );

        if (ret == 0) {
            proto_registry_entry_t* entry = proto_registry_find(registry, builtins[i].name);
            if (entry) {
                entry->is_builtin = true;
                proto_registry_activate(registry, builtins[i].name);
            }
        }
    }

    return builtin_count;
}
