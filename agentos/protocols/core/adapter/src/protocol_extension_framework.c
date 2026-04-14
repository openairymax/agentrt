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
    *caps_json = strdup("{\"capabilities\":[]}");
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

int proto_ext_load_from_config(proto_ext_framework_t* fw, const char* config_json) {
    if (!fw || !config_json) return -1;
    return 0;
}

static protocol_adapter_t proto_ext_framework_adapter = {
    .type = PROTOCOL_CUSTOM,
    .init = NULL, .destroy = NULL,
    .encode = NULL, .decode = NULL,
    .connect = NULL, .disconnect = NULL,
    .is_connected = NULL, .get_stats = NULL
};

const protocol_adapter_t* proto_ext_get_framework_adapter(void) {
    return &proto_ext_framework_adapter;
}
