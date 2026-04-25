#include "gateway_a2a_handler.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define GW_A2A_MAX_TASK_TYPES 64

typedef struct {
    char task_type[128];
    gw_a2a_task_exec_fn exec_fn;
    void* user_data;
} gw_a2a_task_type_entry_t;

struct gw_a2a_handler {
    gw_a2a_handler_config_t config;
    gw_a2a_task_type_entry_t task_types[GW_A2A_MAX_TASK_TYPES];
    size_t task_type_count;
    bool initialized;
    bool healthy;
    uint64_t request_count;
    uint64_t error_count;
};

static int handle_a2a_request(const char* method,
                               const char* path,
                               const char* body_json,
                               char** response_json,
                               void* user_data);

gw_a2a_handler_t* gw_a2a_handler_create(const gw_a2a_handler_config_t* config)
{
    gw_a2a_handler_t* handler = (gw_a2a_handler_t*)calloc(1, sizeof(gw_a2a_handler_t));
    if (!handler) return NULL;
    if (config) {
        handler->config = *config;
    } else {
        gw_a2a_handler_config_t defaults = GW_A2A_HANDLER_CONFIG_DEFAULTS;
        handler->config = defaults;
    }
    return handler;
}

void gw_a2a_handler_destroy(gw_a2a_handler_t* handler)
{
    if (!handler) return;
    if (handler->initialized) {
        gw_a2a_handler_shutdown(handler);
    }
    free(handler);
}

int gw_a2a_handler_init(gw_a2a_handler_t* handler)
{
    if (!handler) return -1;
    if (handler->initialized) return 0;
    handler->initialized = true;
    handler->healthy = true;
    handler->request_count = 0;
    handler->error_count = 0;
    return 0;
}

int gw_a2a_handler_shutdown(gw_a2a_handler_t* handler)
{
    if (!handler || !handler->initialized) return -1;
    handler->task_type_count = 0;
    handler->initialized = false;
    handler->healthy = false;
    return 0;
}

int gw_a2a_handler_register_task_type(gw_a2a_handler_t* handler,
                                       const char* task_type,
                                       gw_a2a_task_exec_fn exec_fn,
                                       void* user_data)
{
    if (!handler || !task_type || !exec_fn) return -1;
    if (handler->task_type_count >= GW_A2A_MAX_TASK_TYPES) return -2;

    gw_a2a_task_type_entry_t* entry = &handler->task_types[handler->task_type_count];
    strncpy(entry->task_type, task_type, sizeof(entry->task_type) - 1);
    entry->task_type[sizeof(entry->task_type) - 1] = '\0';
    entry->exec_fn = exec_fn;
    entry->user_data = user_data;
    handler->task_type_count++;
    return 0;
}

int gw_a2a_handler_get_agent_card(gw_a2a_handler_t* handler,
                                   char** card_json)
{
    if (!handler || !card_json) return -1;

    const char* fmt = "{"
        "\"name\":\"%s\","
        "\"version\":\"%s\","
        "\"url\":\"%s\","
        "\"capabilities\":{\"taskExecution\":%s,\"streaming\":%s,"
        "\"pushNotifications\":%s,\"negotiation\":%s,"
        "\"multiTurn\":%s,\"stateTransition\":%s},"
        "\"protocolVersion\":\"0.3.0\""
        "}";

    uint32_t caps = handler->config.capabilities;
    const char* fmt_bool = "true";
    size_t len = snprintf(NULL, 0, fmt,
        handler->config.agent_name,
        handler->config.agent_version,
        handler->config.agent_url,
        (caps & 0x01) ? fmt_bool : "false",
        (caps & 0x02) ? fmt_bool : "false",
        (caps & 0x04) ? fmt_bool : "false",
        (caps & 0x08) ? fmt_bool : "false",
        (caps & 0x10) ? fmt_bool : "false",
        (caps & 0x20) ? fmt_bool : "false");

    char* buf = (char*)malloc(len + 1);
    if (!buf) return -1;
    snprintf(buf, len + 1, fmt,
        handler->config.agent_name,
        handler->config.agent_version,
        handler->config.agent_url,
        (caps & 0x01) ? fmt_bool : "false",
        (caps & 0x02) ? fmt_bool : "false",
        (caps & 0x04) ? fmt_bool : "false",
        (caps & 0x08) ? fmt_bool : "false",
        (caps & 0x10) ? fmt_bool : "false",
        (caps & 0x20) ? fmt_bool : "false");

    *card_json = buf;
    return 0;
}

static gw_a2a_task_type_entry_t* find_task_type(gw_a2a_handler_t* handler,
                                                  const char* task_type)
{
    for (size_t i = 0; i < handler->task_type_count; i++) {
        if (strcmp(handler->task_types[i].task_type, task_type) == 0) {
            return &handler->task_types[i];
        }
    }
    return NULL;
}

static char* extract_a2a_field(const char* json, const char* field_name)
{
    if (!json || !field_name) return NULL;
    size_t flen = strlen(field_name) + 4;
    char* key = (char*)malloc(flen);
    if (!key) return NULL;
    snprintf(key, flen, "\"%s\"", field_name);
    const char* p = strstr(json, key);
    free(key);
    if (!p) return NULL;
    p += strlen(field_name) + 3;
    while (*p && (*p == ' ' || *p == ':' || *p == '\t')) p++;
    if (*p != '"') return NULL;
    p++;
    const char* end = strchr(p, '"');
    if (!end) return NULL;
    size_t len = (size_t)(end - p);
    char* val = (char*)malloc(len + 1);
    if (!val) return NULL;
    memcpy(val, p, len);
    val[len] = '\0';
    return val;
}

int gw_a2a_handler_handle_request(gw_a2a_handler_t* handler,
                                   const char* method,
                                   const char* path,
                                   const char* body_json,
                                   char** response_json)
{
    if (!handler || !body_json || !response_json) return -1;
    handler->request_count++;

    if (path && strcmp(path, "/a2a/agent-card") == 0) {
        return gw_a2a_handler_get_agent_card(handler, response_json);
    }

    if (path && strcmp(path, "/a2a/task") == 0) {
        char* task_type = extract_a2a_field(body_json, "type");
        char* task_id = extract_a2a_field(body_json, "id");
        char* input_json = extract_a2a_field(body_json, "input");

        if (!task_type) {
            free(task_id);
            free(input_json);
            handler->error_count++;
            return -1;
        }

        gw_a2a_task_type_entry_t* entry = find_task_type(handler, task_type);
        if (!entry) {
            const char* err = "{\"error\":{\"code\":-32601,\"message\":\"Unknown task type: %s\"}}";
            size_t elen = snprintf(NULL, 0, err, task_type);
            char* ebuf = (char*)malloc(elen + 1);
            if (ebuf) snprintf(ebuf, elen + 1, err, task_type);
            *response_json = ebuf;
            free(task_type);
            free(task_id);
            free(input_json);
            handler->error_count++;
            return -2;
        }

        char* output = NULL;
        int rc = entry->exec_fn(
            task_id ? task_id : "unknown",
            task_type,
            input_json ? input_json : "{}",
            &output,
            entry->user_data);

        free(task_type);
        free(task_id);
        free(input_json);

        if (rc != 0 || !output) {
            free(output);
            handler->error_count++;
            return -3;
        }

        const char* resp_fmt = "{\"result\":{\"status\":\"completed\",\"output\":%s}}";
        size_t rlen = snprintf(NULL, 0, resp_fmt, output);
        char* buf = (char*)malloc(rlen + 1);
        if (buf) snprintf(buf, rlen + 1, resp_fmt, output);
        *response_json = buf;
        free(output);
        return 0;
    }

    if (strstr(body_json, "\"agentCard\"") || strstr(body_json, "\"agent-card\"")) {
        return gw_a2a_handler_get_agent_card(handler, response_json);
    }

    handler->error_count++;
    return -1;
}

static int handle_a2a_request(const char* method,
                               const char* path,
                               const char* body_json,
                               char** response_json,
                               void* user_data)
{
    gw_a2a_handler_t* handler = (gw_a2a_handler_t*)user_data;
    if (!handler) return -1;
    return gw_a2a_handler_handle_request(handler, method, path, body_json, response_json);
}

gw_proto_request_handler_t gw_a2a_handler_get_handler(gw_a2a_handler_t* handler)
{
    if (!handler) return NULL;
    return handle_a2a_request;
}

void* gw_a2a_handler_get_handler_data(gw_a2a_handler_t* handler)
{
    return (void*)handler;
}

bool gw_a2a_handler_is_healthy(gw_a2a_handler_t* handler)
{
    if (!handler) return false;
    return handler->healthy;
}
