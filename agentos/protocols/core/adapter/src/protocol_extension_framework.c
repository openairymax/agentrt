// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file protocol_extension_framework.c
 * @brief Protocol Extension Framework Implementation
 */

#include "protocol_extension_framework.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

typedef struct {
    agentos_protocol_type_t type;
    const char* name;
    int (*init)(void* context);
    int (*destroy)(void* context);
    int (*encode)(void* context, const void* msg, void** out_data, size_t* out_size);
    int (*decode)(void* context, const void* data, size_t size, void* out_msg);
    int (*connect)(void* context, const char* endpoint);
    int (*disconnect)(void* context);
    int (*is_connected)(void* context);
    int (*send)(void* context, const void* data, size_t size);
    int (*receive)(void* context, void** data, size_t* size, uint32_t timeout_ms);
    int (*get_stats)(void* context, char* stats_json, size_t max_size);
} local_adapter_t;

typedef struct {
    proto_ext_descriptor_t descriptor;
    proto_ext_callbacks_t callbacks;
    void* adapter_context;
    proto_ext_state_t state;
    uint32_t error_count;
    uint64_t last_activity_ms;
    uint64_t messages_processed;
    bool registered;
} proto_ext_adapter_entry_t;

struct proto_ext_framework_s {
    proto_ext_adapter_entry_t* adapters;
    size_t adapter_count;
    size_t adapter_capacity;

    proto_middleware_t* middlewares;
    size_t middleware_count;
    size_t middleware_capacity;

    uint64_t total_messages;
};

static uint64_t current_time_ms(void) {
    return (uint64_t)time(NULL) * 1000;
}

proto_ext_framework_t* proto_ext_framework_create(void) {
    proto_ext_framework_t* fw = calloc(1, sizeof(proto_ext_framework_t));
    if (!fw) return NULL;

    fw->adapter_capacity = 16;
    fw->adapters = calloc(fw->adapter_capacity, sizeof(proto_ext_adapter_entry_t));
    fw->adapter_count = 0;

    fw->middleware_capacity = 16;
    fw->middlewares = calloc(fw->middleware_capacity, sizeof(proto_middleware_t));
    fw->middleware_count = 0;

    fw->total_messages = 0;

    return fw;
}

void proto_ext_framework_destroy(proto_ext_framework_t* fw) {
    if (!fw) return;

    for (size_t i = 0; i < fw->adapter_count; i++) {
        if (fw->adapters[i].state >= PROTO_EXT_STATE_INITIALIZED && fw->adapters[i].callbacks.on_unload) {
            fw->adapters[i].callbacks.on_unload(fw->adapters[i].adapter_context);
        }
    }
    free(fw->adapters);
    free(fw->middlewares);
    free(fw);
}

int proto_ext_register(proto_ext_framework_t* fw,
                         const proto_ext_descriptor_t* descriptor,
                         const proto_ext_callbacks_t* callbacks) {
    if (!fw || !descriptor || !callbacks) return -1;
    if (fw->adapter_count >= PROTO_EXT_MAX_ADAPTERS) return -2;

    for (size_t i = 0; i < fw->adapter_count; i++) {
        if (strcmp(fw->adapters[i].descriptor.name, descriptor->name) == 0) {
            return -3;
        }
    }

    if (fw->adapter_count >= fw->adapter_capacity) {
        size_t new_cap = fw->adapter_capacity * 2;
        proto_ext_adapter_entry_t* new_adapters = realloc(fw->adapters, new_cap * sizeof(proto_ext_adapter_entry_t));
        if (!new_adapters) return -4;
        fw->adapters = new_adapters;
        fw->adapter_capacity = new_cap;
    }

    proto_ext_adapter_entry_t* entry = &fw->adapters[fw->adapter_count];
    memset(entry, 0, sizeof(*entry));
    memcpy(&entry->descriptor, descriptor, sizeof(proto_ext_descriptor_t));
    memcpy(&entry->callbacks, callbacks, sizeof(proto_ext_callbacks_t));
    entry->adapter_context = NULL;
    entry->state = PROTO_EXT_STATE_UNLOADED;
    entry->error_count = 0;
    entry->last_activity_ms = 0;
    entry->messages_processed = 0;
    entry->registered = true;
    fw->adapter_count++;

    return 0;
}

int proto_ext_unregister(proto_ext_framework_t* fw, const char* name) {
    if (!fw || !name) return -1;
    for (size_t i = 0; i < fw->adapter_count; i++) {
        if (strcmp(fw->adapters[i].descriptor.name, name) == 0) {
            if (fw->adapters[i].state >= PROTO_EXT_STATE_RUNNING) {
                proto_ext_stop(fw, name);
            }
            if (fw->adapters[i].state >= PROTO_EXT_STATE_LOADED) {
                proto_ext_unload(fw, name);
            }
            memmove(&fw->adapters[i], &fw->adapters[i + 1],
                    (fw->adapter_count - i - 1) * sizeof(proto_ext_adapter_entry_t));
            fw->adapter_count--;
            return 0;
        }
    }
    return -2;
}

int proto_ext_load(proto_ext_framework_t* fw, const char* name, const char* config_json) {
    if (!fw || !name) return -1;
    for (size_t i = 0; i < fw->adapter_count; i++) {
        if (strcmp(fw->adapters[i].descriptor.name, name) == 0) {
            if (fw->adapters[i].state != PROTO_EXT_STATE_UNLOADED) return -3;

            if (fw->adapters[i].callbacks.on_load) {
                int rc = fw->adapters[i].callbacks.on_load(&fw->adapters[i].adapter_context);
                if (rc != 0) {
                    fw->adapters[i].state = PROTO_EXT_STATE_ERROR;
                    fw->adapters[i].error_count++;
                    return rc;
                }
            }

            if (fw->adapters[i].callbacks.on_init && config_json) {
                int rc = fw->adapters[i].callbacks.on_init(fw->adapters[i].adapter_context, config_json);
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
    return -2;
}

int proto_ext_unload(proto_ext_framework_t* fw, const char* name) {
    if (!fw || !name) return -1;
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
    return -2;
}

int proto_ext_start(proto_ext_framework_t* fw, const char* name) {
    if (!fw || !name) return -1;
    for (size_t i = 0; i < fw->adapter_count; i++) {
        if (strcmp(fw->adapters[i].descriptor.name, name) == 0) {
            if (fw->adapters[i].state < PROTO_EXT_STATE_LOADED) {
                int rc = proto_ext_load(fw, name, NULL);
                if (rc != 0) return rc;
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
    return -2;
}

int proto_ext_stop(proto_ext_framework_t* fw, const char* name) {
    if (!fw || !name) return -1;
    for (size_t i = 0; i < fw->adapter_count; i++) {
        if (strcmp(fw->adapters[i].descriptor.name, name) == 0) {
            if (fw->adapters[i].callbacks.on_stop) {
                int rc = fw->adapters[i].callbacks.on_stop(fw->adapters[i].adapter_context);
                if (rc != 0) return rc;
            }
            fw->adapters[i].state = PROTO_EXT_STATE_LOADED;
            return 0;
        }
    }
    return -2;
}

int proto_ext_send_message(proto_ext_framework_t* fw,
                             const char* adapter_name,
                             const unified_message_t* message) {
    if (!fw || !adapter_name || !message) return -1;
    for (size_t i = 0; i < fw->adapter_count; i++) {
        if (strcmp(fw->adapters[i].descriptor.name, adapter_name) == 0) {
            if (fw->adapters[i].state != PROTO_EXT_STATE_RUNNING) return -3;

            if (fw->adapters[i].callbacks.encode_message) {
                void* encoded = NULL;
                size_t encoded_size = 0;
                int rc = fw->adapters[i].callbacks.encode_message(
                    fw->adapters[i].adapter_context, message, &encoded, &encoded_size);
                free(encoded);
                if (rc != 0) {
                    fw->adapters[i].error_count++;
                    return rc;
                }
            }

            fw->adapters[i].messages_processed++;
            fw->adapters[i].last_activity_ms = current_time_ms();
            fw->total_messages++;
            return 0;
        }
    }
    return -2;
}

int proto_ext_handle_request(proto_ext_framework_t* fw,
                               const char* adapter_name,
                               const char* method,
                               const char* params_json,
                               char** response_json) {
    if (!fw || !adapter_name || !response_json) return -1;
    for (size_t i = 0; i < fw->adapter_count; i++) {
        if (strcmp(fw->adapters[i].descriptor.name, adapter_name) == 0) {
            if (fw->adapters[i].state != PROTO_EXT_STATE_RUNNING) return -3;
            if (!fw->adapters[i].callbacks.handle_request) return -4;

            int rc = fw->adapters[i].callbacks.handle_request(
                fw->adapters[i].adapter_context, method, params_json, response_json);

            fw->adapters[i].messages_processed++;
            fw->adapters[i].last_activity_ms = current_time_ms();
            fw->total_messages++;

            if (rc != 0) fw->adapters[i].error_count++;
            return rc;
        }
    }
    return -2;
}

int proto_ext_auto_route(proto_ext_framework_t* fw,
                           const unified_message_t* message,
                           char** adapter_name) {
    if (!fw || !message || !adapter_name) return -1;

    for (size_t i = 0; i < fw->adapter_count; i++) {
        if (fw->adapters[i].state != PROTO_EXT_STATE_RUNNING) continue;
        if (fw->adapters[i].descriptor.protocol_type == message->protocol) {
            *adapter_name = strdup(fw->adapters[i].descriptor.name);
            return 0;
        }
    }

    for (size_t i = 0; i < fw->adapter_count; i++) {
        if (fw->adapters[i].state != PROTO_EXT_STATE_RUNNING) continue;
        if (fw->adapters[i].descriptor.protocol_type == PROTOCOL_CUSTOM) {
            *adapter_name = strdup(fw->adapters[i].descriptor.name);
            return 0;
        }
    }

    return -2;
}

int proto_ext_negotiate(proto_ext_framework_t* fw,
                          const char* adapter_name,
                          const char* client_version,
                          char** agreed_version) {
    if (!fw || !adapter_name) return -1;
    for (size_t i = 0; i < fw->adapter_count; i++) {
        if (strcmp(fw->adapters[i].descriptor.name, adapter_name) == 0) {
            if (!fw->adapters[i].callbacks.negotiate_version) {
                *agreed_version = strdup(fw->adapters[i].descriptor.version);
                return 0;
            }
            return fw->adapters[i].callbacks.negotiate_version(
                fw->adapters[i].adapter_context, client_version, agreed_version);
        }
    }
    return -2;
}

int proto_ext_add_middleware(proto_ext_framework_t* fw,
                               const char* name,
                               proto_middleware_fn middleware,
                               proto_ext_priority_t priority,
                               void* user_data) {
    if (!fw || !name || !middleware) return -1;
    if (fw->middleware_count >= PROTO_EXT_MAX_MIDDLEWARE) return -2;

    if (fw->middleware_count >= fw->middleware_capacity) {
        size_t new_cap = fw->middleware_capacity * 2;
        proto_middleware_t* new_mw = realloc(fw->middlewares, new_cap * sizeof(proto_middleware_t));
        if (!new_mw) return -3;
        fw->middlewares = new_mw;
        fw->middleware_capacity = new_cap;
    }

    proto_middleware_t* mw = &fw->middlewares[fw->middleware_count];
    strncpy(mw->name, name, PROTO_EXT_MAX_NAME_LEN - 1);
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

int proto_ext_remove_middleware(proto_ext_framework_t* fw, const char* name) {
    if (!fw || !name) return -1;
    for (size_t i = 0; i < fw->middleware_count; i++) {
        if (strcmp(fw->middlewares[i].name, name) == 0) {
            memmove(&fw->middlewares[i], &fw->middlewares[i + 1],
                    (fw->middleware_count - i - 1) * sizeof(proto_middleware_t));
            fw->middleware_count--;
            return 0;
        }
    }
    return -2;
}

int proto_ext_enable_middleware(proto_ext_framework_t* fw, const char* name) {
    if (!fw || !name) return -1;
    for (size_t i = 0; i < fw->middleware_count; i++) {
        if (strcmp(fw->middlewares[i].name, name) == 0) {
            fw->middlewares[i].enabled = true;
            return 0;
        }
    }
    return -2;
}

int proto_ext_disable_middleware(proto_ext_framework_t* fw, const char* name) {
    if (!fw || !name) return -1;
    for (size_t i = 0; i < fw->middleware_count; i++) {
        if (strcmp(fw->middlewares[i].name, name) == 0) {
            fw->middlewares[i].enabled = false;
            return 0;
        }
    }
    return -2;
}

int proto_ext_process_middleware_chain(proto_ext_framework_t* fw,
                                        const unified_message_t* request,
                                        unified_message_t* response) {
    if (!fw || !request || !response) return -1;

    for (size_t i = 0; i < fw->middleware_count; i++) {
        if (!fw->middlewares[i].enabled) continue;
        int rc = fw->middlewares[i].process(request, response, fw->middlewares[i].user_data);
        if (rc != 0) return rc;
    }
    return 0;
}

int proto_ext_get_adapter_stats(proto_ext_framework_t* fw,
                                  const char* name,
                                  proto_ext_stats_t* stats) {
    if (!fw || !name || !stats) return -1;
    for (size_t i = 0; i < fw->adapter_count; i++) {
        if (strcmp(fw->adapters[i].descriptor.name, name) == 0) {
            strncpy(stats->name, fw->adapters[i].descriptor.name, PROTO_EXT_MAX_NAME_LEN - 1);
            stats->state = fw->adapters[i].state;
            stats->error_count = fw->adapters[i].error_count;
            stats->last_activity_ms = fw->adapters[i].last_activity_ms;
            stats->messages_processed = fw->adapters[i].messages_processed;
            return 0;
        }
    }
    return -2;
}

int proto_ext_list_adapters(proto_ext_framework_t* fw, char** names_json) {
    if (!fw || !names_json) return -1;
    size_t buf_size = 4096 + fw->adapter_count * 128;
    char* buf = malloc(buf_size);
    if (!buf) return -3;

    size_t offset = snprintf(buf, buf_size, "{\"adapters\":[");
    for (size_t i = 0; i < fw->adapter_count; i++) {
        if (i > 0) offset += snprintf(buf + offset, buf_size - offset, ",");
        const char* state_str = "unknown";
        switch (fw->adapters[i].state) {
            case PROTO_EXT_STATE_UNLOADED: state_str = "unloaded"; break;
            case PROTO_EXT_STATE_LOADED: state_str = "loaded"; break;
            case PROTO_EXT_STATE_INITIALIZED: state_str = "initialized"; break;
            case PROTO_EXT_STATE_RUNNING: state_str = "running"; break;
            case PROTO_EXT_STATE_ERROR: state_str = "error"; break;
            case PROTO_EXT_STATE_DISABLED: state_str = "disabled"; break;
        }
        offset += snprintf(buf + offset, buf_size - offset,
            "{\"name\":\"%s\",\"version\":\"%s\",\"state\":\"%s\",\"type\":%d}",
            fw->adapters[i].descriptor.name,
            fw->adapters[i].descriptor.version,
            state_str,
            fw->adapters[i].descriptor.protocol_type);
    }
    offset += snprintf(buf + offset, buf_size - offset, "]}");
    *names_json = buf;
    return 0;
}

int proto_ext_list_capabilities(proto_ext_framework_t* fw, char** caps_json) {
    if (!fw || !caps_json) return -1;

    uint32_t all_caps = 0;
    for (size_t i = 0; i < fw->adapter_count; i++) {
        if (fw->adapters[i].state >= PROTO_EXT_STATE_LOADED) {
            all_caps |= fw->adapters[i].descriptor.capabilities;
        }
    }

    size_t buf_size = 2048;
    char* buf = malloc(buf_size);
    if (!buf) return -3;

    size_t offset = snprintf(buf, buf_size, "{\"capabilities\":[");

    bool first = true;
    const struct { uint32_t bit; const char* name; } cap_names[] = {
        { PROTO_CAP_REQUEST_RESPONSE, "request_response" },
        { PROTO_CAP_STREAMING, "streaming" },
        { PROTO_CAP_BIDIRECTIONAL, "bidirectional" },
        { 0x0008, "binary" },
        { 0x0010, "compression" },
        { 0x0020, "encryption" },
        { PROTO_CAP_AUTHENTICATION, "authentication" },
        { PROTO_CAP_AGENT_DISCOVERY, "discovery" },
        { 0x0080, "delegation" },
        { PROTO_CAP_NEGOTIATION, "negotiation" },
        { 0x2000, "consensus" },
        { PROTO_CAP_FUNCTION_CALLING, "tool_use" },
        { 0x4000, "vision" },
        { 0x8000, "extended_thinking" },
        { 0x10000, "code_execution" },
        { 0x20000, "human_in_loop" },
        { 0x40000, "memory" },
        { 0x80000, "rag" },
        { PROTO_CAP_CUSTOM, "multi_agent" },
        { 0x100000, "cluster" },
        { 0x0100, "embedding" },
        { 0x0200, "token_counting" },
        { 0x0400, "prompt_caching" },
        { 0x10000, "safety_filter" },
    };

    for (size_t c = 0; c < sizeof(cap_names)/sizeof(cap_names[0]); c++) {
        if (all_caps & cap_names[c].bit) {
            if (!first) offset += snprintf(buf + offset, buf_size - offset, ",");
            offset += snprintf(buf + offset, buf_size - offset, "\"%s\"", cap_names[c].name);
            first = false;
        }
    }

    snprintf(buf + offset, buf_size - offset, "],\"raw_flags\":%u}", all_caps);
    *caps_json = buf;
    return 0;
}

int proto_ext_find_by_capability(proto_ext_framework_t* fw,
                                   uint32_t capability,
                                   char*** adapter_names,
                                   size_t* count) {
    if (!fw || !adapter_names || !count) return -1;

    size_t found = 0;
    char** results = calloc(fw->adapter_count, sizeof(char*));
    if (!results) return -3;

    for (size_t i = 0; i < fw->adapter_count; i++) {
        if (fw->adapters[i].descriptor.capabilities & capability) {
            results[found++] = strdup(fw->adapters[i].descriptor.name);
        }
    }

    *adapter_names = results;
    *count = found;
    return 0;
}

proto_ext_state_t proto_ext_get_state(proto_ext_framework_t* fw, const char* name) {
    if (!fw || !name) return PROTO_EXT_STATE_UNLOADED;
    for (size_t i = 0; i < fw->adapter_count; i++) {
        if (strcmp(fw->adapters[i].descriptor.name, name) == 0) {
            return fw->adapters[i].state;
        }
    }
    return PROTO_EXT_STATE_UNLOADED;
}

static char* json_extract_string(const char* json, const char* key) {
    if (!json || !key) return NULL;
    char search[128];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char* p = strstr(json, search);
    if (!p) return NULL;
    p += strlen(search);
    while (*p && (*p == ' ' || *p == ':')) p++;
    if (*p != '"') return NULL;
    p++;
    const char* end = strchr(p, '"');
    if (!end) return NULL;
    size_t len = end - p;
    char* result = malloc(len + 1);
    memcpy(result, p, len);
    result[len] = '\0';
    return result;
}

static int json_extract_int(const char* json, const char* key, int default_val) {
    char* s = json_extract_string(json, key);
    if (s) { int v = atoi(s); free(s); return v; }
    return default_val;
}

int proto_ext_load_from_config(proto_ext_framework_t* fw, const char* config_json) {
    if (!fw || !config_json) return -1;

    const char* adapters_start = strstr(config_json, "\"adapters\"");
    if (!adapters_start) {
        adapters_start = strstr(config_json, "\"extensions\"");
        if (!adapters_start) return -2;
    }

    const char* array_start = strchr(adapters_start, '[');
    if (!array_start) return -3;

    int loaded_count = 0;
    const char* p = array_start + 1;
    while (*p && *p != ']') {
        const char* obj_start = strchr(p, '{');
        if (!obj_start) break;
        const char* obj_end = strchr(obj_start, '}');
        if (!obj_end) break;

        size_t obj_len = obj_end - obj_start + 1;
        char* obj_buf = malloc(obj_len + 1);
        memcpy(obj_buf, obj_start, obj_len);
        obj_buf[obj_len] = '\0';

        char* name = json_extract_string(obj_buf, "name");
        char* version = json_extract_string(obj_buf, "version");
        char* desc = json_extract_string(obj_buf, "description");
        char* author = json_extract_string(obj_buf, "author");
        int proto_type = json_extract_int(obj_buf, "protocol_type", PROTOCOL_CUSTOM);
        uint32_t caps = (uint32_t)json_extract_int(obj_buf, "capabilities", 0);
        int priority = json_extract_int(obj_buf, "priority", 50);

        if (name) {
            proto_ext_descriptor_t desc_struct = { .protocol_type = proto_type,
                .capabilities = caps, .priority = priority, .hot_loadable = true };
            strncpy(desc_struct.name, name, PROTO_EXT_MAX_NAME_LEN - 1);
            if (version) strncpy(desc_struct.version, version, PROTO_EXT_MAX_VERSION_LEN - 1);
            else { strncpy(desc_struct.version, "1.0.0", PROTO_EXT_MAX_VERSION_LEN - 1); desc_struct.version[PROTO_EXT_MAX_VERSION_LEN - 1] = '\0'; }
            if (desc) strncpy(desc_struct.description, desc, 255);
            else { strncpy(desc_struct.description, "Loaded from config", 255); desc_struct.description[254] = '\0'; }
            if (author) strncpy(desc_struct.author, author, 127);

            proto_ext_callbacks_t empty_cbs = { 0 };

            int rc = proto_ext_register(fw, &desc_struct, &empty_cbs);
            if (rc == 0) {
                proto_ext_load(fw, name, obj_buf);
                loaded_count++;
            }
        }

        free(name); free(version); free(desc); free(author); free(obj_buf);
        p = obj_end + 1;
        while (*p && (*p == ',' || *p == ' ' || *p == '\n' || *p == '\r')) p++;
    }

    return loaded_count;
}

static proto_ext_framework_t* g_framework_instance = NULL;

static int fw_adapter_init(void* ctx, const char* config) {
    (void)ctx;
    if (!g_framework_instance) {
        g_framework_instance = proto_ext_framework_create();
        if (config && config[0] != '\0') {
            proto_ext_load_from_config(g_framework_instance, config);
        }
    }
    return 0;
}

static void fw_adapter_destroy(void* ctx) {
    (void)ctx;
    if (g_framework_instance) {
        proto_ext_framework_destroy(g_framework_instance);
        g_framework_instance = NULL;
    }
}

static int fw_adapter_encode(void* ctx, const void* in, size_t in_len, void** out, size_t* out_len) {
    (void)ctx;
    if (!in || !out || !out_len) return -1;
    if (in_len == 0) { *out = NULL; *out_len = 0; return -2; }

    *out = malloc(in_len);
    if (!*out) return -3;
    memcpy(*out, in, in_len);
    *out_len = in_len;
    return 0;
}

static int fw_adapter_decode(void* ctx, const void* in, size_t in_len, void** out, size_t* out_len) {
    (void)ctx;
    if (!in || !out || !out_len) return -1;
    if (in_len == 0) { *out = NULL; *out_len = 0; return -2; }

    *out = malloc(in_len);
    if (!*out) return -3;
    memcpy(*out, in, in_len);
    *out_len = in_len;
    return 0;
}

static bool fw_adapter_is_connected(void* ctx) {
    (void)ctx;
    return g_framework_instance != NULL;
}

static int fw_adapter_get_stats(void* ctx, char* stats_json, size_t max_size) {
    (void)ctx;
    (void)stats_json;
    (void)max_size;
    return 0;
}

static local_adapter_t proto_ext_framework_adapter_internal = {
    .type = PROTOCOL_CUSTOM,
    .init = fw_adapter_init,
    .destroy = fw_adapter_destroy,
    .encode = fw_adapter_encode,
    .decode = fw_adapter_decode,
    .connect = NULL,
    .disconnect = NULL,
    .is_connected = fw_adapter_is_connected,
    .get_stats = fw_adapter_get_stats
};

static protocol_adapter_t proto_ext_framework_adapter = NULL;

void proto_ext_framework_init_adapter(void) {
    proto_ext_framework_adapter = (protocol_adapter_t)&proto_ext_framework_adapter_internal;
}

const protocol_adapter_t* proto_ext_get_framework_adapter(void) {
    if (!proto_ext_framework_adapter) {
        proto_ext_framework_init_adapter();
    }
    return &proto_ext_framework_adapter;
}

proto_ext_framework_t* proto_ext_get_global_instance(void) {
    if (!g_framework_instance) {
        g_framework_instance = proto_ext_framework_create();
    }
    return g_framework_instance;
}
