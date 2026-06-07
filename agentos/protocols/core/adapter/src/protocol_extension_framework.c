// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: Apache-2.0
/**
 * @file protocol_extension_framework.c
 * @brief Protocol Extension Framework Implementation
 */

#include "protocol_extension_framework.h"

#include "memory_compat.h"
#include "types.h"

#include <stdio.h>
#include "error.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
    proto_ext_descriptor_t descriptor;
    proto_ext_callbacks_t callbacks;
    void *adapter_context;
    proto_ext_state_t state;
    uint32_t error_count;
    uint64_t last_activity_ms;
    uint64_t messages_processed;
    bool registered;
} proto_ext_adapter_entry_t;

struct proto_ext_framework_s {
    proto_ext_adapter_entry_t *adapters;
    size_t adapter_count;
    size_t adapter_capacity;

    proto_middleware_t *middlewares;
    size_t middleware_count;
    size_t middleware_capacity;

    uint64_t total_messages;
};

static uint64_t current_time_ms(void)
{
    return (uint64_t)time(NULL) * 1000;
}

proto_ext_framework_t *proto_ext_framework_create(void)
{
    proto_ext_framework_t *fw = AGENTOS_CALLOC(1, sizeof(proto_ext_framework_t));
    if (!fw)
        return NULL;

    fw->adapter_capacity = 16;
    fw->adapters = AGENTOS_CALLOC(fw->adapter_capacity, sizeof(proto_ext_adapter_entry_t));
    fw->adapter_count = 0;

    fw->middleware_capacity = 16;
    fw->middlewares = AGENTOS_CALLOC(fw->middleware_capacity, sizeof(proto_middleware_t));
    fw->middleware_count = 0;

    fw->total_messages = 0;

    return fw;
}

void proto_ext_framework_destroy(proto_ext_framework_t *fw)
{
    if (!fw)
        return;

    for (size_t i = 0; i < fw->adapter_count; i++) {
        if (fw->adapters[i].state >= PROTO_EXT_STATE_INITIALIZED &&
            fw->adapters[i].callbacks.on_unload) {
            fw->adapters[i].callbacks.on_unload(fw->adapters[i].adapter_context);
        }
    }
    AGENTOS_FREE(fw->adapters);
    AGENTOS_FREE(fw->middlewares);
    AGENTOS_FREE(fw);
}

int proto_ext_register(proto_ext_framework_t *fw, const proto_ext_descriptor_t *descriptor,
                       const proto_ext_callbacks_t *callbacks)
{
    if (!fw || !descriptor || !callbacks)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "proto_ext_register: failed");
        return AGENTOS_ERR_UNKNOWN;
        }
    if (fw->adapter_count >= PROTO_EXT_MAX_ADAPTERS)
        AGENTOS_ERROR(AGENTOS_ERR_INVALID_PARAM, "invalid parameter");

    for (size_t i = 0; i < fw->adapter_count; i++) {
        if (strcmp(fw->adapters[i].descriptor.name, descriptor->name) == 0) {
            AGENTOS_ERROR(AGENTOS_ERR_NULL_POINTER, "null pointer");
        }
    }

    if (fw->adapter_count >= fw->adapter_capacity) {
        size_t new_cap = fw->adapter_capacity * 2;
        proto_ext_adapter_entry_t *new_adapters =
            AGENTOS_REALLOC(fw->adapters, new_cap * sizeof(proto_ext_adapter_entry_t));
        if (!new_adapters)
            AGENTOS_ERROR(AGENTOS_ERR_OUT_OF_MEMORY, "out of memory");
        fw->adapters = new_adapters;
        fw->adapter_capacity = new_cap;
    }

    proto_ext_adapter_entry_t *entry = &fw->adapters[fw->adapter_count];
    AGENTOS_MEMSET(entry, 0, sizeof(*entry));
    __builtin_memcpy(&entry->descriptor, descriptor, sizeof(proto_ext_descriptor_t));
    __builtin_memcpy(&entry->callbacks, callbacks, sizeof(proto_ext_callbacks_t));
    entry->adapter_context = NULL;
    entry->state = PROTO_EXT_STATE_UNLOADED;
    entry->error_count = 0;
    entry->last_activity_ms = 0;
    entry->messages_processed = 0;
    entry->registered = true;
    fw->adapter_count++;

    return 0;
}

int proto_ext_unregister(proto_ext_framework_t *fw, const char *name)
{
    if (!fw || !name)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "proto_ext_unregister: failed");
        return AGENTOS_ERR_UNKNOWN;
        }
    for (size_t i = 0; i < fw->adapter_count; i++) {
        if (strcmp(fw->adapters[i].descriptor.name, name) == 0) {
            if (fw->adapters[i].state >= PROTO_EXT_STATE_RUNNING) {
                proto_ext_stop(fw, name);
            }
            if (fw->adapters[i].state >= PROTO_EXT_STATE_LOADED) {
                proto_ext_unload(fw, name);
            }
            __builtin_memmove(&fw->adapters[i], &fw->adapters[i + 1],
                    (fw->adapter_count - i - 1) * sizeof(proto_ext_adapter_entry_t));
            fw->adapter_count--;
            return 0;
        }
    }
    AGENTOS_ERROR(AGENTOS_ERR_INVALID_PARAM, "invalid parameter");
}

int proto_ext_load(proto_ext_framework_t *fw, const char *name, const char *config_json)
{
    if (!fw || !name)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "proto_ext_load: failed");
        return AGENTOS_ERR_UNKNOWN;
        }
    for (size_t i = 0; i < fw->adapter_count; i++) {
        if (strcmp(fw->adapters[i].descriptor.name, name) == 0) {
            if (fw->adapters[i].state != PROTO_EXT_STATE_UNLOADED)
                AGENTOS_ERROR(AGENTOS_ERR_NULL_POINTER, "null pointer");

            if (fw->adapters[i].callbacks.on_load) {
                int rc = fw->adapters[i].callbacks.on_load(&fw->adapters[i].adapter_context);
                if (rc != 0) {
                    fw->adapters[i].state = PROTO_EXT_STATE_ERROR;
                    fw->adapters[i].error_count++;
                    return rc;
                }
            }

            if (fw->adapters[i].callbacks.on_init && config_json) {
                int rc =
                    fw->adapters[i].callbacks.on_init(fw->adapters[i].adapter_context, config_json);
                if (rc != 0) {
                    fw->adapters[i].state = PROTO_EXT_STATE_ERROR;
                    fw->adapters[i].error_count++;
                    return rc;
                }
            }

            fw->adapters[i].state = PROTO_EXT_STATE_LOADED;
            return 0;
        }
    }
    AGENTOS_ERROR(AGENTOS_ERR_INVALID_PARAM, "invalid parameter");
}

int proto_ext_unload(proto_ext_framework_t *fw, const char *name)
{
    if (!fw || !name)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "proto_ext_unload: failed");
        return AGENTOS_ERR_UNKNOWN;
        }
    for (size_t i = 0; i < fw->adapter_count; i++) {
        if (strcmp(fw->adapters[i].descriptor.name, name) == 0) {
            if (fw->adapters[i].callbacks.on_unload) {
                fw->adapters[i].callbacks.on_unload(fw->adapters[i].adapter_context);
            }
            fw->adapters[i].adapter_context = NULL;
            fw->adapters[i].state = PROTO_EXT_STATE_UNLOADED;
            return 0;
        }
    }
    AGENTOS_ERROR(AGENTOS_ERR_INVALID_PARAM, "invalid parameter");
}

int proto_ext_start(proto_ext_framework_t *fw, const char *name)
{
    if (!fw || !name)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "proto_ext_start: failed");
        return AGENTOS_ERR_UNKNOWN;
        }
    for (size_t i = 0; i < fw->adapter_count; i++) {
        if (strcmp(fw->adapters[i].descriptor.name, name) == 0) {
            if (fw->adapters[i].state < PROTO_EXT_STATE_LOADED) {
                int rc = proto_ext_load(fw, name, NULL);
                if (rc != 0)
                    return rc;
            }
            if (fw->adapters[i].callbacks.on_start) {
                int rc = fw->adapters[i].callbacks.on_start(fw->adapters[i].adapter_context);
                if (rc != 0) {
                    fw->adapters[i].state = PROTO_EXT_STATE_ERROR;
                    return rc;
                }
            }
            fw->adapters[i].state = PROTO_EXT_STATE_RUNNING;
            return 0;
        }
    }
    AGENTOS_ERROR(AGENTOS_ERR_INVALID_PARAM, "invalid parameter");
}

int proto_ext_stop(proto_ext_framework_t *fw, const char *name)
{
    if (!fw || !name)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "proto_ext_stop: failed");
        return AGENTOS_ERR_UNKNOWN;
        }
    for (size_t i = 0; i < fw->adapter_count; i++) {
        if (strcmp(fw->adapters[i].descriptor.name, name) == 0) {
            if (fw->adapters[i].callbacks.on_stop) {
                int rc = fw->adapters[i].callbacks.on_stop(fw->adapters[i].adapter_context);
                if (rc != 0)
                    return rc;
            }
            fw->adapters[i].state = PROTO_EXT_STATE_LOADED;
            return 0;
        }
    }
    AGENTOS_ERROR(AGENTOS_ERR_INVALID_PARAM, "invalid parameter");
}

int proto_ext_send_message(proto_ext_framework_t *fw, const char *adapter_name,
                           const unified_message_t *message)
{
    if (!fw || !adapter_name || !message)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "proto_ext_send_message: IO error");
        return AGENTOS_ERR_UNKNOWN;
        }
    for (size_t i = 0; i < fw->adapter_count; i++) {
        if (strcmp(fw->adapters[i].descriptor.name, adapter_name) == 0) {
            if (fw->adapters[i].state != PROTO_EXT_STATE_RUNNING)
                AGENTOS_ERROR(AGENTOS_ERR_NULL_POINTER, "null pointer");

            if (fw->adapters[i].callbacks.encode_message) {
                void *encoded = NULL;
                size_t encoded_size = 0;
                int rc = fw->adapters[i].callbacks.encode_message(fw->adapters[i].adapter_context,
                                                                  message, &encoded, &encoded_size);
                if (rc != 0) {
                    AGENTOS_FREE(encoded);
                    fw->adapters[i].error_count++;
                    return rc;
                }

                if (encoded && encoded_size > 0 && fw->adapters[i].callbacks.handle_request) {
                    char params_json[64];
                    snprintf(params_json, sizeof(params_json), "{\"size\":%zu}", encoded_size);
                    char *response = NULL;
                    int send_rc = fw->adapters[i].callbacks.handle_request(
                        fw->adapters[i].adapter_context, "send", params_json, &response);
                    AGENTOS_FREE(response);
                    if (send_rc != 0) {
                        fw->adapters[i].error_count++;
                    }
                }

                AGENTOS_FREE(encoded);
            }

            fw->adapters[i].messages_processed++;
            fw->adapters[i].last_activity_ms = current_time_ms();
            fw->total_messages++;
            return 0;
        }
    }
    AGENTOS_ERROR(AGENTOS_ERR_INVALID_PARAM, "invalid parameter");
}

int proto_ext_handle_request(proto_ext_framework_t *fw, const char *adapter_name,
                             const char *method, const char *params_json, char **response_json)
{
    if (!fw || !adapter_name || !response_json)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "proto_ext_handle_request: failed");
        return AGENTOS_ERR_UNKNOWN;
        }
    for (size_t i = 0; i < fw->adapter_count; i++) {
        if (strcmp(fw->adapters[i].descriptor.name, adapter_name) == 0) {
            if (fw->adapters[i].state != PROTO_EXT_STATE_RUNNING)
                AGENTOS_ERROR(AGENTOS_ERR_NULL_POINTER, "null pointer");
            if (!fw->adapters[i].callbacks.handle_request)
                AGENTOS_ERROR(AGENTOS_ERR_OUT_OF_MEMORY, "out of memory");

            int rc = fw->adapters[i].callbacks.handle_request(fw->adapters[i].adapter_context,
                                                              method, params_json, response_json);

            fw->adapters[i].messages_processed++;
            fw->adapters[i].last_activity_ms = current_time_ms();
            fw->total_messages++;

            if (rc != 0)
                fw->adapters[i].error_count++;
            return rc;
        }
    }
    AGENTOS_ERROR(AGENTOS_ERR_INVALID_PARAM, "invalid parameter");
}

int proto_ext_auto_route(proto_ext_framework_t *fw, const unified_message_t *message,
                         char **adapter_name)
{
    if (!fw || !message || !adapter_name)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "proto_ext_auto_route: failed");
        return AGENTOS_ERR_UNKNOWN;
        }

    for (size_t i = 0; i < fw->adapter_count; i++) {
        if (fw->adapters[i].state != PROTO_EXT_STATE_RUNNING)
            continue;
        if (fw->adapters[i].descriptor.protocol_type == message->protocol) {
            *adapter_name = AGENTOS_STRDUP(fw->adapters[i].descriptor.name);
            return 0;
        }
    }

    for (size_t i = 0; i < fw->adapter_count; i++) {
        if (fw->adapters[i].state != PROTO_EXT_STATE_RUNNING)
            continue;
        if (fw->adapters[i].descriptor.protocol_type == PROTOCOL_CUSTOM) {
            *adapter_name = AGENTOS_STRDUP(fw->adapters[i].descriptor.name);
            return 0;
        }
    }

    AGENTOS_ERROR(AGENTOS_ERR_INVALID_PARAM, "invalid parameter");
}

int proto_ext_negotiate(proto_ext_framework_t *fw, const char *adapter_name,
                        const char *client_version, char **agreed_version)
{
    if (!fw || !adapter_name)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "proto_ext_negotiate: failed");
        return AGENTOS_ERR_UNKNOWN;
        }
    for (size_t i = 0; i < fw->adapter_count; i++) {
        if (strcmp(fw->adapters[i].descriptor.name, adapter_name) == 0) {
            if (!fw->adapters[i].callbacks.negotiate_version) {
                *agreed_version = AGENTOS_STRDUP(fw->adapters[i].descriptor.version);
                return 0;
            }
            return fw->adapters[i].callbacks.negotiate_version(fw->adapters[i].adapter_context,
                                                               client_version, agreed_version);
        }
    }
    AGENTOS_ERROR(AGENTOS_ERR_INVALID_PARAM, "invalid parameter");
}

int proto_ext_add_middleware(proto_ext_framework_t *fw, const char *name,
                             proto_middleware_fn middleware, proto_ext_priority_t priority,
                             void *user_data)
{
    if (!fw || !name || !middleware)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "proto_ext_add_middleware: failed");
        return AGENTOS_ERR_UNKNOWN;
        }
    if (fw->middleware_count >= PROTO_EXT_MAX_MIDDLEWARE)
        AGENTOS_ERROR(AGENTOS_ERR_INVALID_PARAM, "invalid parameter");

    if (fw->middleware_count >= fw->middleware_capacity) {
        size_t new_cap = fw->middleware_capacity * 2;
        proto_middleware_t *new_mw =
            AGENTOS_REALLOC(fw->middlewares, new_cap * sizeof(proto_middleware_t));
        if (!new_mw)
            AGENTOS_ERROR(AGENTOS_ERR_NULL_POINTER, "null pointer");
        fw->middlewares = new_mw;
        fw->middleware_capacity = new_cap;
    }

    proto_middleware_t *mw = &fw->middlewares[fw->middleware_count];
    AGENTOS_STRNCPY_TERM(mw->name, name, PROTO_EXT_MAX_NAME_LEN);
    mw->name[PROTO_EXT_MAX_NAME_LEN - 1] = '\0';
    mw->process = middleware;
    mw->priority = priority;
    mw->user_data = user_data;
    mw->enabled = true;
    fw->middleware_count++;

    for (size_t i = fw->middleware_count - 1; i > 0; i--) {
        if (fw->middlewares[i].priority > fw->middlewares[i - 1].priority) {
            proto_middleware_t tmp = fw->middlewares[i];
            fw->middlewares[i] = fw->middlewares[i - 1];
            fw->middlewares[i - 1] = tmp;
        }
    }

    return 0;
}

int proto_ext_remove_middleware(proto_ext_framework_t *fw, const char *name)
{
    if (!fw || !name)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "proto_ext_remove_middleware: failed");
        return AGENTOS_ERR_UNKNOWN;
        }
    for (size_t i = 0; i < fw->middleware_count; i++) {
        if (strcmp(fw->middlewares[i].name, name) == 0) {
            __builtin_memmove(&fw->middlewares[i], &fw->middlewares[i + 1],
                    (fw->middleware_count - i - 1) * sizeof(proto_middleware_t));
            fw->middleware_count--;
            return 0;
        }
    }
    AGENTOS_ERROR(AGENTOS_ERR_INVALID_PARAM, "invalid parameter");
}

int proto_ext_enable_middleware(proto_ext_framework_t *fw, const char *name)
{
    if (!fw || !name)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "proto_ext_enable_middleware: failed");
        return AGENTOS_ERR_UNKNOWN;
        }
    for (size_t i = 0; i < fw->middleware_count; i++) {
        if (strcmp(fw->middlewares[i].name, name) == 0) {
            fw->middlewares[i].enabled = true;
            return 0;
        }
    }
    AGENTOS_ERROR(AGENTOS_ERR_INVALID_PARAM, "invalid parameter");
}

int proto_ext_disable_middleware(proto_ext_framework_t *fw, const char *name)
{
    if (!fw || !name)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "proto_ext_disable_middleware: failed");
        return AGENTOS_ERR_UNKNOWN;
        }
    for (size_t i = 0; i < fw->middleware_count; i++) {
        if (strcmp(fw->middlewares[i].name, name) == 0) {
            fw->middlewares[i].enabled = false;
            return 0;
        }
    }
    AGENTOS_ERROR(AGENTOS_ERR_INVALID_PARAM, "invalid parameter");
}

int proto_ext_process_middleware_chain(proto_ext_framework_t *fw, const unified_message_t *request,
                                       unified_message_t *response)
{
    if (!fw || !request || !response)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "proto_ext_process_middleware_chain: failed");
        return AGENTOS_ERR_UNKNOWN;
        }

    for (size_t i = 0; i < fw->middleware_count; i++) {
        if (!fw->middlewares[i].enabled)
            continue;
        int rc = fw->middlewares[i].process(request, response, fw->middlewares[i].user_data);
        if (rc != 0)
            return rc;
    }
    return 0;
}

int proto_ext_get_adapter_stats(proto_ext_framework_t *fw, const char *name,
                                proto_ext_stats_t *stats)
{
    if (!fw || !name || !stats)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "proto_ext_get_adapter_stats: failed");
        return AGENTOS_ERR_UNKNOWN;
        }
    for (size_t i = 0; i < fw->adapter_count; i++) {
        if (strcmp(fw->adapters[i].descriptor.name, name) == 0) {
            AGENTOS_STRNCPY_TERM(stats->name, fw->adapters[i].descriptor.name, PROTO_EXT_MAX_NAME_LEN);
            stats->state = fw->adapters[i].state;
            stats->error_count = fw->adapters[i].error_count;
            stats->last_activity_ms = fw->adapters[i].last_activity_ms;
            stats->messages_processed = fw->adapters[i].messages_processed;
            return 0;
        }
    }
    AGENTOS_ERROR(AGENTOS_ERR_INVALID_PARAM, "invalid parameter");
}

int proto_ext_list_adapters(proto_ext_framework_t *fw, char **names_json)
{
    if (!fw || !names_json)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "proto_ext_list_adapters: failed");
        return AGENTOS_ERR_UNKNOWN;
        }
    size_t buf_size = 4096 + fw->adapter_count * 128;
    char *buf = AGENTOS_MALLOC(buf_size);
    if (!buf)
        AGENTOS_ERROR(AGENTOS_ERR_NULL_POINTER, "null pointer");

    size_t offset = snprintf(buf, buf_size, "{\"adapters\":[");
    for (size_t i = 0; i < fw->adapter_count; i++) {
        if (i > 0)
            offset += snprintf(buf + offset, buf_size - offset, ",");
        const char *state_str = "unknown";
        switch (fw->adapters[i].state) {
        case PROTO_EXT_STATE_UNLOADED:
            state_str = "unloaded";
            break;
        case PROTO_EXT_STATE_LOADED:
            state_str = "loaded";
            break;
        case PROTO_EXT_STATE_INITIALIZED:
            state_str = "initialized";
            break;
        case PROTO_EXT_STATE_RUNNING:
            state_str = "running";
            break;
        case PROTO_EXT_STATE_ERROR:
            state_str = "error";
            break;
        case PROTO_EXT_STATE_DISABLED:
            state_str = "disabled";
            break;
        }
        offset += snprintf(buf + offset, buf_size - offset,
                           "{\"name\":\"%s\",\"version\":\"%s\",\"state\":\"%s\",\"type\":%d}",
                           fw->adapters[i].descriptor.name, fw->adapters[i].descriptor.version,
                           state_str, fw->adapters[i].descriptor.protocol_type);
    }
    offset += snprintf(buf + offset, buf_size - offset, "]}");
    *names_json = buf;
    return 0;
}

int proto_ext_list_capabilities(proto_ext_framework_t *fw, char **caps_json)
{
    if (!fw || !caps_json)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "proto_ext_list_capabilities: failed");
        return AGENTOS_ERR_UNKNOWN;
        }

    uint32_t all_caps = 0;
    for (size_t i = 0; i < fw->adapter_count; i++) {
        if (fw->adapters[i].state >= PROTO_EXT_STATE_LOADED) {
            all_caps |= fw->adapters[i].descriptor.capabilities;
        }
    }

    size_t buf_size = 2048;
    char *buf = AGENTOS_MALLOC(buf_size);
    if (!buf)
        AGENTOS_ERROR(AGENTOS_ERR_NULL_POINTER, "null pointer");

    size_t offset = snprintf(buf, buf_size, "{\"capabilities\":[");

    bool first = true;
    const struct {
        uint32_t bit;
        const char *name;
    } cap_names[] = {
        {PROTO_CAP_REQUEST_RESPONSE, "request_response"},
        {PROTO_CAP_STREAMING, "streaming"},
        {PROTO_CAP_BIDIRECTIONAL, "bidirectional"},
        {0x0008, "binary"},
        {0x0010, "compression"},
        {0x0020, "encryption"},
        {PROTO_CAP_AUTHENTICATION, "authentication"},
        {PROTO_CAP_AGENT_DISCOVERY, "discovery"},
        {0x0080, "delegation"},
        {PROTO_CAP_NEGOTIATION, "negotiation"},
        {0x2000, "consensus"},
        {PROTO_CAP_FUNCTION_CALLING, "tool_use"},
        {0x4000, "vision"},
        {0x8000, "extended_thinking"},
        {0x10000, "code_execution"},
        {0x20000, "human_in_loop"},
        {0x40000, "memory"},
        {0x80000, "rag"},
        {PROTO_CAP_CUSTOM, "multi_agent"},
        {0x100000, "cluster"},
        {0x0100, "embedding"},
        {0x0200, "token_counting"},
        {0x0400, "prompt_caching"},
        {0x10000, "safety_filter"},
    };

    for (size_t c = 0; c < sizeof(cap_names) / sizeof(cap_names[0]); c++) {
        if (all_caps & cap_names[c].bit) {
            if (!first)
                offset += snprintf(buf + offset, buf_size - offset, ",");
            offset += snprintf(buf + offset, buf_size - offset, "\"%s\"", cap_names[c].name);
            first = false;
        }
    }

    snprintf(buf + offset, buf_size - offset, "],\"raw_flags\":%u}", all_caps);
    *caps_json = buf;
    return 0;
}

int proto_ext_find_by_capability(proto_ext_framework_t *fw, uint32_t capability,
                                 char ***adapter_names, size_t *count)
{
    if (!fw || !adapter_names || !count)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "proto_ext_find_by_capability: failed");
        return AGENTOS_ERR_UNKNOWN;
        }

    size_t found = 0;
    char **results = AGENTOS_CALLOC(fw->adapter_count, sizeof(char *));
    if (!results)
        AGENTOS_ERROR(AGENTOS_ERR_NULL_POINTER, "null pointer");

    for (size_t i = 0; i < fw->adapter_count; i++) {
        if (fw->adapters[i].descriptor.capabilities & capability) {
            results[found++] = AGENTOS_STRDUP(fw->adapters[i].descriptor.name);
        }
    }

    *adapter_names = results;
    *count = found;
    return 0;
}

proto_ext_state_t proto_ext_get_state(proto_ext_framework_t *fw, const char *name)
{
    if (!fw || !name)
        return PROTO_EXT_STATE_UNLOADED;
    for (size_t i = 0; i < fw->adapter_count; i++) {
        if (strcmp(fw->adapters[i].descriptor.name, name) == 0) {
            return fw->adapters[i].state;
        }
    }
    return PROTO_EXT_STATE_UNLOADED;
}

static char *json_extract_string(const char *json, const char *key)
{
    if (!json || !key)
        return NULL;
    char search[128];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *p = strstr(json, search);
    if (!p)
        return NULL;
    p += strlen(search);
    while (*p && (*p == ' ' || *p == ':'))
        p++;
    if (*p != '"')
        return NULL;
    p++;
    const char *end = strchr(p, '"');
    if (!end)
        return NULL;
    size_t len = end - p;
    char *result = AGENTOS_MALLOC(len + 1);
    __builtin_memcpy(result, p, len);
    result[len] = '\0';
    return result;
}

static int json_extract_int(const char *json, const char *key, int default_val)
{
    char *s = json_extract_string(json, key);
    if (s) {
        int v = (int)strtol(s, NULL, 10);
        AGENTOS_FREE(s);
        return v;
    }
    return default_val;
}

int proto_ext_load_from_config(proto_ext_framework_t *fw, const char *config_json)
{
    if (!fw || !config_json)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "proto_ext_load_from_config: failed");
        return AGENTOS_ERR_UNKNOWN;
        }

    const char *adapters_start = strstr(config_json, "\"adapters\"");
    if (!adapters_start) {
        adapters_start = strstr(config_json, "\"extensions\"");
        if (!adapters_start)
            AGENTOS_ERROR(AGENTOS_ERR_INVALID_PARAM, "invalid parameter");
    }

    const char *array_start = strchr(adapters_start, '[');
    if (!array_start)
        AGENTOS_ERROR(AGENTOS_ERR_NULL_POINTER, "null pointer");

    int loaded_count = 0;
    const char *p = array_start + 1;
    while (*p && *p != ']') {
        const char *obj_start = strchr(p, '{');
        if (!obj_start)
            break;
        const char *obj_end = strchr(obj_start, '}');
        if (!obj_end)
            break;

        size_t obj_len = obj_end - obj_start + 1;
        char *obj_buf = AGENTOS_MALLOC(obj_len + 1);
        __builtin_memcpy(obj_buf, obj_start, obj_len);
        obj_buf[obj_len] = '\0';

        char *name = json_extract_string(obj_buf, "name");
        char *version = json_extract_string(obj_buf, "version");
        char *desc = json_extract_string(obj_buf, "description");
        char *author = json_extract_string(obj_buf, "author");
        int proto_type = json_extract_int(obj_buf, "protocol_type", PROTOCOL_CUSTOM);
        uint32_t caps = (uint32_t)json_extract_int(obj_buf, "capabilities", 0);
        int priority = json_extract_int(obj_buf, "priority", 50);

        if (name) {
            proto_ext_descriptor_t desc_struct = {.protocol_type = proto_type,
                                                  .capabilities = caps,
                                                  .priority = priority,
                                                  .hot_loadable = true};
            AGENTOS_STRNCPY_TERM(desc_struct.name, name, PROTO_EXT_MAX_NAME_LEN);
            if (version)
                AGENTOS_STRNCPY_TERM(desc_struct.version, version, PROTO_EXT_MAX_VERSION_LEN);
            else {
                AGENTOS_STRNCPY_TERM(desc_struct.version, "1.0.0", PROTO_EXT_MAX_VERSION_LEN);
            }
            if (desc)
                AGENTOS_STRNCPY_TERM(desc_struct.description, desc, sizeof(desc_struct.description));
            else {
                AGENTOS_STRNCPY_TERM(desc_struct.description, "Loaded from config", sizeof(desc_struct.description));
            }
            if (author)
                AGENTOS_STRNCPY_TERM(desc_struct.author, author, sizeof(desc_struct.author));

            proto_ext_callbacks_t empty_cbs = {0};

            int rc = proto_ext_register(fw, &desc_struct, &empty_cbs);
            if (rc == 0) {
                proto_ext_load(fw, name, obj_buf);
                loaded_count++;
            }
        }

        AGENTOS_FREE(name);
        AGENTOS_FREE(version);
        AGENTOS_FREE(desc);
        AGENTOS_FREE(author);
        AGENTOS_FREE(obj_buf);
        p = obj_end + 1;
        while (*p && (*p == ',' || *p == ' ' || *p == '\n' || *p == '\r'))
            p++;
    }

    return loaded_count;
}

static proto_ext_framework_t *g_framework_instance = NULL;

static int fw_adapter_init(void *ctx)
{
    if (!g_framework_instance) {
        g_framework_instance = proto_ext_framework_create();
    }
    return 0;
}

static int fw_adapter_destroy(void *ctx)
{
    if (g_framework_instance) {
        proto_ext_framework_destroy(g_framework_instance);
        g_framework_instance = NULL;
    }
    return 0;
}

static int fw_adapter_encode(void *ctx, const void *msg, void **out_data, size_t *out_size)
{
    if (!msg || !out_data || !out_size)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "fw_adapter_encode: failed");
        return AGENTOS_ERR_UNKNOWN;
        }
    unified_message_t *umsg = (unified_message_t *)msg;
    size_t in_len =
        umsg->payload_size ? umsg->payload_size : (umsg->payload ? strlen(umsg->payload) : 0);
    if (in_len == 0) {
        *out_data = NULL;
        *out_size = 0;
        AGENTOS_ERROR(AGENTOS_ERR_INVALID_PARAM, "invalid parameter");
    }

    *out_data = AGENTOS_MALLOC(in_len);
    if (!*out_data)
        AGENTOS_ERROR(AGENTOS_ERR_NULL_POINTER, "null pointer");
    __builtin_memcpy(*out_data, umsg->payload ? umsg->payload : "", in_len);
    *out_size = in_len;
    return 0;
}

static int fw_adapter_decode(void *ctx, const void *data, size_t size, void *out_msg)
{
    if (!data || !out_msg || size == 0)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "fw_adapter_decode: failed");
        return AGENTOS_ERR_UNKNOWN;
        }

    unified_message_t *msg = (unified_message_t *)out_msg;
    AGENTOS_MEMSET(msg, 0, sizeof(*msg));
    msg->payload = AGENTOS_MALLOC(size + 1);
    if (!msg->payload)
        AGENTOS_ERROR(AGENTOS_ERR_NULL_POINTER, "null pointer");
    __builtin_memcpy((void *)msg->payload, data, size);
    ((char *)msg->payload)[size] = '\0';
    msg->payload_size = size;
    return 0;
}

static int fw_adapter_is_connected(void *ctx)
{
    return g_framework_instance != NULL ? 1 : 0;
}

static int fw_adapter_get_stats(void *ctx, char *stats_json, size_t max_size)
{
    if (!stats_json || max_size < 64)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "fw_adapter_get_stats: failed");
        return AGENTOS_ERR_UNKNOWN;
        }
    int written = snprintf(stats_json, max_size,
                           "{\"adapter\":\"protocol_extension_framework\",\"status\":\"active\"}");
    return (written >= 0 && (size_t)written < max_size) ? 0 : -2;
}

static int fw_adapter_connect(void *ctx, const char *endpoint)
{
    if (!endpoint)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "fw_adapter_connect: IO error");
        return AGENTOS_ERR_UNKNOWN;
        }
    if (!g_framework_instance)
        AGENTOS_ERROR(AGENTOS_ERR_INVALID_PARAM, "invalid parameter");
    for (size_t i = 0; i < g_framework_instance->adapter_count; i++) {
        if (g_framework_instance->adapters[i].state == PROTO_EXT_STATE_INITIALIZED) {
            g_framework_instance->adapters[i].state = PROTO_EXT_STATE_RUNNING;
            g_framework_instance->adapters[i].last_activity_ms = current_time_ms();
            return 0;
        }
    }
    AGENTOS_ERROR(AGENTOS_ERR_NULL_POINTER, "null pointer");
}

static int fw_adapter_disconnect(void *ctx)
{
    if (!g_framework_instance)
        AGENTOS_ERROR(AGENTOS_ERR_INVALID_PARAM, "invalid parameter");
    for (size_t i = 0; i < g_framework_instance->adapter_count; i++) {
        if (g_framework_instance->adapters[i].state == PROTO_EXT_STATE_RUNNING) {
            g_framework_instance->adapters[i].state = PROTO_EXT_STATE_LOADED;
        }
    }
    return 0;
}

static int fw_adapter_send(void *ctx, const void *data, size_t size)
{
    if (!data || size == 0)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "fw_adapter_send: IO error");
        return AGENTOS_ERR_UNKNOWN;
        }
    if (!g_framework_instance)
        AGENTOS_ERROR(AGENTOS_ERR_INVALID_PARAM, "invalid parameter");
    for (size_t i = 0; i < g_framework_instance->adapter_count; i++) {
        if (g_framework_instance->adapters[i].state == PROTO_EXT_STATE_RUNNING) {
            g_framework_instance->adapters[i].messages_processed++;
            g_framework_instance->adapters[i].last_activity_ms = current_time_ms();
            g_framework_instance->total_messages++;
            return 0;
        }
    }
    AGENTOS_ERROR(AGENTOS_ERR_NULL_POINTER, "null pointer");
}

static int fw_adapter_receive(void *ctx, void **data, size_t *size, uint32_t timeout_ms)
{
    if (!data || !size)
        {
        agentos_error_push_ex(AGENTOS_ERR_TIMEOUT, __FILE__, __LINE__, __func__, "fw_adapter_receive: timeout");
        return AGENTOS_ERR_TIMEOUT;
        }
    if (!g_framework_instance)
        AGENTOS_ERROR(AGENTOS_ERR_INVALID_PARAM, "invalid parameter");
    *data = NULL;
    *size = 0;
    for (size_t i = 0; i < g_framework_instance->adapter_count; i++) {
        if (g_framework_instance->adapters[i].state == PROTO_EXT_STATE_RUNNING) {
            return 0;
        }
    }
    AGENTOS_ERROR(AGENTOS_ERR_NULL_POINTER, "null pointer");
}

static int fw_adapter_handle_request(void *ctx, const void *req, void **resp)
{
    if (!req || !resp)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "fw_adapter_handle_request: failed");
        return AGENTOS_ERR_UNKNOWN;
        }
    if (!g_framework_instance)
        AGENTOS_ERROR(AGENTOS_ERR_INVALID_PARAM, "invalid parameter");
    size_t running = 0;
    for (size_t i = 0; i < g_framework_instance->adapter_count; i++) {
        if (g_framework_instance->adapters[i].state == PROTO_EXT_STATE_RUNNING) {
            g_framework_instance->adapters[i].messages_processed++;
            g_framework_instance->total_messages++;
            running++;
        }
    }

    char stats_json[512];
    snprintf(stats_json, sizeof(stats_json),
             "{\"framework\":\"extension\","
             "\"adapters_total\":%zu,"
             "\"adapters_running\":%zu,"
             "\"messages_processed\":%llu,"
             "\"active_connections\":0}",
             g_framework_instance->adapter_count, running,
             (unsigned long long)g_framework_instance->total_messages);

    *resp = AGENTOS_STRDUP(stats_json);
    return *resp ? 0 : -1;
}

static int fw_adapter_get_version(void *ctx, char *buf, size_t max_size)
{
    if (!buf || max_size == 0)
        return AGENTOS_ENOMEM;
    const char *ver = "1.0.0";
    size_t len = strlen(ver);
    if (len >= max_size)
        len = max_size - 1;
    __builtin_memcpy(buf, ver, len);
    buf[len] = '\0';
    return 0;
}

static uint32_t fw_adapter_capabilities(void *ctx)
{
    return (uint32_t)(PROTO_CAP_STREAMING | PROTO_CAP_TOOL_CALLING);
}

static protocol_adapter_t proto_ext_framework_adapter_internal = {
    .type = PROTOCOL_CUSTOM,
    .name = "Protocol Extension Framework Adapter",
    .version = "0.1.0",
    .description = "Built-in protocol extension framework adapter",
    .init = fw_adapter_init,
    .destroy = fw_adapter_destroy,
    .encode = fw_adapter_encode,
    .decode = fw_adapter_decode,
    .connect = fw_adapter_connect,
    .disconnect = fw_adapter_disconnect,
    .is_connected = fw_adapter_is_connected,
    .send = fw_adapter_send,
    .receive = fw_adapter_receive,
    .handle_request = fw_adapter_handle_request,
    .get_version = fw_adapter_get_version,
    .capabilities = fw_adapter_capabilities,
    .get_stats = fw_adapter_get_stats,
    .context = NULL,
    .user_data = NULL};

const protocol_adapter_t *proto_ext_get_framework_adapter(void)
{
    return &proto_ext_framework_adapter_internal;
}

proto_ext_framework_t *proto_ext_get_global_instance(void)
{
    if (!g_framework_instance) {
        g_framework_instance = proto_ext_framework_create();
    }
    return g_framework_instance;
}
