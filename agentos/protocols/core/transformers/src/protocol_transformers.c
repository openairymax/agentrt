// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file protocol_transformers.c
 * @brief Protocol Message Transformers — Complete Implementation
 *
 * 实现所有协议间的双向消息格式转换，替换 protocol_router.c 中的 TODO 存根。
 */

#include "protocol_transformers.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Transform Context
 * ============================================================================ */

transform_context_t* transform_context_create(const char* src_proto,
                                                const char* tgt_proto) {
    transform_context_t* ctx = calloc(1, sizeof(transform_context_t));
    if (!ctx) return NULL;
    if (src_proto) strncpy(ctx->source_protocol, src_proto, sizeof(ctx->source_protocol) - 1);
    if (tgt_proto) strncpy(ctx->target_protocol, tgt_proto, sizeof(ctx->target_protocol) - 1);
    return ctx;
}

void transform_context_destroy(transform_context_t* ctx) {
    free(ctx);
}

/* ============================================================================
 * JSON-RPC → MCP 转换器
 * ============================================================================ */

int transformer_jsonrpc_to_mcp_request(const unified_message_t* source,
                                       unified_message_t* target,
                                       void* context) {
    if (!source || !target) return -1;

    memset(target, 0, sizeof(*target));
    target->protocol = PROTOCOL_CUSTOM;
    strncpy(target->protocol_name, "mcp", sizeof(target->protocol_name) - 1);
    target->direction = MSG_TYPE_REQUEST;
    strncpy(target->method, "tools/call", sizeof(target->method) - 1);

    transform_context_t* ctx = (transform_context_t*)context;
    if (ctx && ctx->trace_id[0]) {
        strncpy(target->metadata.trace_id, ctx->trace_id, sizeof(target->metadata.trace_id) - 1);
    }

    char mcp_params[4096] = {0};
    const char* name = NULL;
    const char* arguments = "{}";

    if (source->payload) {
        const char* p = strstr(source->payload, "\"name\"");
        if (p) {
            const char* colon = strchr(p, ':');
            if (colon) {
                const char* start = strchr(colon + 1, '"');
                if (start) {
                    start++;
                    const char* end = strchr(start, '"');
                    if (end) {
                        size_t len = end - start;
                        name = strndup(start, len);
                    }
                }
            }
        }
        const char* a = strstr(source->payload, "\"arguments\"");
        if (a) {
            const char* colon2 = strchr(a, ':');
            if (colon2) {
                arguments = colon2 + 1;
            }
        }
    }

    snprintf(mcp_params, sizeof(mcp_params),
        "{\"name\":\"%s\",\"arguments\":%s}",
        name ? name : "unknown",
        arguments);

    free((void*)name);

    target->payload_size = strlen(mcp_params) + 1;
    target->payload = strdup(mcp_params);
    target = ENCODING_UTF8_JSON;

    return 0;
}

int transformer_mcp_to_jsonrpc_response(const unified_message_t* source,
                                        unified_message_t* target,
                                        void* context) {
    if (!source || !target) return -1;

    memset(target, 0, sizeof(*target));
    target->protocol = PROTOCOL_HTTP;
    strncpy(target->protocol_name, "jsonrpc", sizeof(target->protocol_name) - 1);
    target->direction = source->is_error ? MSG_TYPE_ERROR : MSG_TYPE_RESPONSE;

    transform_context_t* ctx = (transform_context_t*)context;
    int jsonrpc_id = ctx ? ctx->jsonrpc_id_counter : 1;

    if (source->is_error) {
        snprintf(target->error_code, sizeof(target->error_code), "%d", source->error_code);
        snprintf(target->error_msg, sizeof(target->error_msg),
            "%s", source->payload ? source->payload : "MCP error");
    } else {
        char output_buf[8192] = {0};
        const char* content_text = "";
        if (source->payload) {
            const char* text_key = strstr(source->payload, "\"text\"");
            if (text_key) {
                const char* val_start = strchr(text_key + 5, '"');
                if (val_start) {
                    content_text = val_start + 1;
                    const char* val_end = strchr(content_text, '"');
                    if (val_end) {
                        size_t len = val_end - content_text;
                        char* tmp = malloc(len + 1);
                        memcpy(tmp, content_text, len);
                        tmp[len] = '\0';
                        snprintf(output_buf, sizeof(output_buf),
                            "{\"output\":%s,\"status\":\"success\"}", tmp);
                        free(tmp);
                        content_text = output_buf;
                    } else {
                        snprintf(output_buf, sizeof(output_buf),
                            "{\"output\":%s,\"status\":\"success\"}", source->payload);
                        content_text = output_buf;
                    }
                }
            } else {
                snprintf(output_buf, sizeof(output_buf),
                    "{\"output\":%s,\"status\":\"success\"}", source->payload);
                content_text = output_buf;
            }
        } else {
            content_text = "{\"output\":\"\",\"status\":\"success\"}";
        }

        target->payload_size = strlen(content_text) + 1;
        target->payload = strdup(content_text);
        target = ENCODING_UTF8_JSON;
    }

    return 0;
}

int transformer_mcp_tools_list_to_jsonrpc(const unified_message_t* source,
                                          unified_message_t* target,
                                          void* context) {
    (void)context;
    if (!source || !target) return -1;

    memset(target, 0, sizeof(*target));
    target->protocol = PROTOCOL_HTTP;
    strncpy(target->protocol_name, "jsonrpc", sizeof(target->protocol_name) - 1);
    target->direction = MSG_TYPE_RESPONSE;

    if (source->payload) {
        target->payload_size = strlen(source->payload) + 1;
        target->payload = strdup(source->payload);
        target = ENCODING_UTF8_JSON;
    } else {
        target->payload = strdup("{\"skills\":[]}");
        target->payload_size = strlen(target->payload) + 1;
        target = ENCODING_UTF8_JSON;
    }

    return 0;
}

/* ============================================================================
 * JSON-RPC → A2A 转换器
 * ============================================================================ */

int transformer_jsonrpc_to_a2a_task(const unified_message_t* source,
                                    unified_message_t* target,
                                    void* context) {
    if (!source || !target) return -1;

    memset(target, 0, sizeof(*target));
    target->protocol = PROTOCOL_CUSTOM;
    strncpy(target->protocol_name, "a2a", sizeof(target->protocol_name) - 1);
    target->direction = MSG_TYPE_REQUEST;
    strncpy(target->method, "task/delegate", sizeof(target->method) - 1);

    transform_context_t* ctx = (transform_context_t*)context;
    if (ctx && ctx->agent_id[0]) {
        strncpy(target->sender_id, ctx->agent_id, sizeof(target->sender_id) - 1);
    }
    if (ctx && ctx->trace_id[0]) {
        strncpy(target->metadata.trace_id, ctx->trace_id, sizeof(target->metadata.trace_id) - 1);
    }

    char a2a_payload[8192] = {0};
    const char* description = "";
    const char* input_data = "{}";

    if (source->payload) {
        const char* desc_key = strstr(source->payload, "\"description\"");
        if (desc_key) {
            const char* val_start = strchr(desc_key + 11, '"');
            if (val_start) {
                description = val_start + 1;
            }
        }
        const char* input_key = strstr(source->payload, "\"input\"");
        if (input_key) {
            const char* colon = strchr(input_key, ':');
            if (colon) input_data = colon + 1;
        }
    }

    snprintf(a2a_payload, sizeof(a2a_payload),
        "{\"description\":%s,\"input_data\":%s}",
        description ? description : "\"\"",
        input_data);

    target->payload_size = strlen(a2a_payload) + 1;
    target->payload = strdup(a2a_payload);
    target = ENCODING_UTF8_JSON;

    return 0;
}

int transformer_a2a_to_jsonrpc_response(const unified_message_t* source,
                                        unified_message_t* target,
                                        void* context) {
    (void)context;
    if (!source || !target) return -1;

    memset(target, 0, sizeof(*target));
    target->protocol = PROTOCOL_HTTP;
    target->endpoint = "jsonrpc";
    target->direction = DIRECTION_RESPONSE;

    if (source->payload) {
        target->payload_size = strlen((const char*)source->payload) + 1;
        target->payload = strdup((const char*)source->payload);
    } else {
        target->payload = strdup("{\"result\":{}}");
        target->payload_size = strlen((const char*)target->payload) + 1;
    }

    return 0;
}

int transformer_jsonrpc_to_a2a_discover(const unified_message_t* source,
                                        unified_message_t* target,
                                        void* context) {
    (void)source; (void)context;
    if (!target) return -1;

    memset(target, 0, sizeof(*target));
    target->protocol = PROTOCOL_CUSTOM;
    target->endpoint = "a2a/agent/discover";
    target->direction = DIRECTION_REQUEST;

    target->payload = strdup("{}");
    target->payload_size = 3;

    return 0;
}

int transformer_a2a_agents_to_jsonrpc(const unified_message_t* source,
                                      unified_message_t* target,
                                      void* context) {
    (void)context;
    if (!source || !target) return -1;

    memset(target, 0, sizeof(*target));
    target->protocol = PROTOCOL_HTTP;
    target->endpoint = "jsonrpc";
    target->direction = DIRECTION_RESPONSE;

    if (source->payload) {
        target->payload_size = strlen((const char*)source->payload) + 1;
        target->payload = strdup((const char*)source->payload);
    } else {
        target->payload = strdup("{\"agents\":[]}");
        target->payload_size = strlen((const char*)target->payload) + 1;
    }

    return 0;
}

/* ============================================================================
 * JSON-RPC → OpenAI API 转换器
 * ============================================================================ */

int transformer_jsonrpc_to_openai_chat(const unified_message_t* source,
                                      unified_message_t* target,
                                      void* context) {
    if (!source || !target) return -1;

    memset(target, 0, sizeof(*target));
    target->protocol = PROTOCOL_CUSTOM;
    target->endpoint = "openai/chat/completions";
    target->direction = DIRECTION_REQUEST;

    char openai_payload[16384] = {0};
    const char* model = "gpt-4o";
    float temperature = 0.7f;
    int max_tokens = 2048;
    char messages_part[12000] = {0};

    if (source->payload) {
        const char* model_key = strstr((const char*)source->payload, "\"model\"");
        if (model_key) {
            const char* val_start = strchr(model_key + 6, '"');
            if (val_start) {
                model = val_start + 1;
            }
        }
        const char* temp_key = strstr((const char*)source->payload, "\"temperature\"");
        if (temp_key) {
            sscanf(temp_key + 13, "%f", &temperature);
        }
        const char* max_tok_key = strstr((const char*)source->payload, "\"max_tokens\"");
        if (max_tok_key) {
            sscanf(max_tok_key + 12, "%d", &max_tokens);
        }
        const char* msgs_key = strstr((const char*)source->payload, "\"messages\"");
        if (msgs_key) {
            const char* arr_start = strchr(msgs_key + 9, '[');
            if (arr_start) {
                const char* arr_end = strrchr(arr_start, ']');
                if (arr_end) {
                    size_t len = arr_end - arr_start + 1;
                    memcpy(messages_part, arr_start, len);
                    messages_part[len] = '\0';
                }
            }
        }
    }

    if (messages_part[0] == '\0') {
        snprintf(messages_part, sizeof(messages_part),
            "[{\"role\":\"user\",\"content\":\"%s\"}]",
            source->payload ? (const char*)source->payload : "");
    }

    snprintf(openai_payload, sizeof(openai_payload),
        "{\"model\":\"%s\",\"messages\":%s,\"temperature\":%.1f,"
        "\"max_tokens\":%d,\"stream\":false}",
        model, messages_part, temperature, max_tokens);

    target->payload = strdup(openai_payload);
    target->payload_size = strlen(openai_payload) + 1;

    return 0;
}

int transformer_openai_chat_to_jsonrpc(const unified_message_t* source,
                                      unified_message_t* target,
                                      void* context) {
    (void)context;
    if (!source || !target) return -1;

    memset(target, 0, sizeof(*target));
    target->protocol = PROTOCOL_HTTP;
    target->endpoint = "jsonrpc";
    target->direction = DIRECTION_RESPONSE;

    if (!source->payload) {
        target->payload = strdup("{\"content\":\"\",\"finish_reason\":\"stop\","
            "\"usage\":{\"prompt_tokens\":0,\"completion_tokens\":0,\"total_tokens\":0}}");
        target->payload_size = strlen((const char*)target->payload) + 1;
        return 0;
    }

    const char* content = "";
    const char* finish_reason = "stop";
    char usage_str[512] = "{}";

    const char* choices_key = strstr((const char*)source->payload, "\"choices\"");
    if (choices_key) {
        const char* message_key = strstr(choices_key, "\"message\"");
        if (message_key) {
            const char* content_key = strstr(message_key, "\"content\"");
            if (content_key) {
                const char* val_start = strchr(content_key + 8, '"');
                if (val_start) content = val_start + 1;
            }
        }
        const char* finish_key = strstr(choices_key, "\"finish_reason\"");
        if (finish_key) {
            const char* val_start = strchr(finish_key + 14, '"');
            if (val_start) finish_reason = val_start + 1;
        }
    }

    const char* usage_key = strstr((const char*)source->payload, "\"usage\"");
    if (usage_key) {
        const char* obj_start = strchr(usage_key, '{');
        if (obj_start) {
            const char* obj_end = strchr(obj_start, '}');
            if (obj_end) {
                size_t len = obj_end - obj_start + 1;
                memcpy(usage_str, obj_start, len);
                usage_str[len] = '\0';
            }
        }
    }

    char result_buf[16384];
    snprintf(result_buf, sizeof(result_buf),
        "{\"content\":\"%s\",\"finish_reason\":\"%s\",\"usage\":%s}",
        content, finish_reason, usage_str);

    target->payload = strdup(result_buf);
    target->payload_size = strlen(result_buf) + 1;

    return 0;
}

int transformer_openai_stream_chunk_to_jsonrpc(const unified_message_t* source,
                                               unified_message_t* target,
                                               void* context) {
    (void)context;
    if (!source || !target) return -1;

    memset(target, 0, sizeof(*target));
    target->protocol = PROTOCOL_HTTP;
    target->endpoint = "jsonrpc/llm.stream.chunk";
    target->direction = DIRECTION_NOTIFICATION;

    if (source->payload) {
        target->payload_size = strlen((const char*)source->payload) + 1;
        target->payload = strdup((const char*)source->payload);
    } else {
        target->payload = strdup("{\"delta\":\"\"}");
        target->payload_size = strlen((const char*)target->payload) + 1;
    }

    return 0;
}

int transformer_jsonrpc_to_openai_embedding(const unified_message_t* source,
                                           unified_message_t* target,
                                           void* context) {
    if (!source || !target) return -1;
    (void)context;

    memset(target, 0, sizeof(*target));
    target->protocol = PROTOCOL_CUSTOM;
    target->endpoint = "openai/embeddings";
    target->direction = DIRECTION_REQUEST;

    const char* text = "";
    const char* model = "text-embedding-ada-002";

    if (source->payload) {
        const char* t = strstr((const char*)source->payload, "\"text\"");
        if (t) {
            const char* v = strchr(t + 5, '"');
            if (v) text = v + 1;
        }
        const char* m = strstr((const char*)source->payload, "\"model\"");
        if (m) {
            const char* v = strchr(m + 6, '"');
            if (v) model = v + 1;
        }
    }

    char payload_buf[8192];
    snprintf(payload_buf, sizeof(payload_buf),
        "{\"model\":\"%s\",\"input\":\"%s\"}", model, text);

    target->payload = strdup(payload_buf);
    target->payload_size = strlen(payload_buf) + 1;

    return 0;
}

/* ============================================================================
 * JSON-RPC → OpenJiuwen 转换器
 * ============================================================================ */

#define OPENJIUWEN_MAGIC      0x4F4A574DUL  /* "OJWM" */
#define OPENJIUWEN_VERSION     0x0001

#pragma pack(push, 1)
typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t flags;
    uint32_t msg_type;
    uint64_t request_id;
    uint32_t payload_length;
} openjiuwen_header_t;
#pragma pack(pop)

int transformer_jsonrpc_to_openjiuwen(const unified_message_t* source,
                                     unified_message_t* target,
                                     void* context) {
    if (!source || !target) return -1;

    memset(target, 0, sizeof(*target));
    target->protocol = PROTOCOL_CUSTOM;
    target->endpoint = "openjiuwen";
    target->direction = DIRECTION_REQUEST;

    openjiuwen_header_t header;
    memset(&header, 0, sizeof(header));
    header.magic = OPENJIUWEN_MAGIC;
    header.version = OPENJIUWEN_VERSION;
    header.msg_type = (uint32_t)(source->direction == DIRECTION_REQUEST ? 1 : 2);
    header.request_id = source->message_id;

    size_t payload_len = source->payload ? strlen((const char*)source->payload) : 0;
    header.payload_length = (uint32_t)payload_len;

    size_t total_size = sizeof(openjiuwen_header_t) + payload_len + 4;
    unsigned char* binary_data = calloc(1, total_size);
    if (!binary_data) return -2;

    memcpy(binary_data, &header, sizeof(openjiuwen_header_t));

    if (source->payload && payload_len > 0) {
        memcpy(binary_data + sizeof(openjiuwen_header_t),
               source->payload, payload_len);
    }

    uint32_t crc = 0;
    for (size_t i = 0; i < total_size - 4; i++) {
        crc ^= binary_data[i];
        crc = (crc << 1) | (crc >> 31);
    }
    memcpy(binary_data + total_size - 4, &crc, sizeof(crc));

    target->payload = binary_data;
    target->payload_size = total_size;

    return 0;
}

int transformer_openjiuwen_to_jsonrpc(const unified_message_t* source,
                                     unified_message_t* target,
                                     void* context) {
    (void)context;
    if (!source || !target || !source->payload ||
        source->payload_size < sizeof(openjiuwen_header_t)) {
        return -1;
    }

    memset(target, 0, sizeof(*target));
    target->protocol = PROTOCOL_HTTP;
    target->endpoint = "jsonrpc";

    const unsigned char* data = (const unsigned char*)source->payload;
    const openjiuwen_header_t* hdr = (const openjiuwen_header_t*)data;

    if (hdr->magic != OPENJIUWEN_MAGIC) {
        target->direction = DIRECTION_RESPONSE;
        target->payload = strdup("{\"error\":\"Invalid magic\"}");
        target->payload_size = 22;
        return 0;
    }

    target->message_id = hdr->request_id;
    target->direction = (hdr->msg_type == 2) ? DIRECTION_RESPONSE : DIRECTION_REQUEST;

    if (hdr->payload_length > 0 &&
        source->payload_size >= sizeof(openjiuwen_header_t) + hdr->payload_length) {

        const char* payload_ptr =
            (const char*)(data + sizeof(openjiuwen_header_t));
        size_t payload_len = hdr->payload_length;

        char* escaped = malloc(payload_len * 2 + 256);
        if (escaped) {
            size_t j = 0;
            j += snprintf(escaped + j, payload_len * 2 + 256 - j,
                         "{\"raw\":\"");
            for (size_t i = 0; i < payload_len && j < payload_len * 2 + 200; i++) {
                unsigned char c = (unsigned char)payload_ptr[i];
                if (c >= 32 && c < 127 && c != '"' && c != '\\') {
                    escaped[j++] = c;
                } else {
                    j += snprintf(escaped + j, 4, "\\x%02X", c);
                }
            }
            j += snprintf(escaped + j, 8, "\"}");
            target->payload = escaped;
            target->payload_size = j;
        }
    } else {
        target->payload = strdup("{}");
        target->payload_size = 3;
    }

    return 0;
}

/* ============================================================================
 * Auto-transform dispatcher
 * ============================================================================ */

typedef struct {
    const char* from_proto;
    const char* to_proto;
    int (*transform_fn)(const unified_message_t*, unified_message_t*, void*);
} transform_entry_t;

static const transform_entry_t g_transform_table[] = {
    { "jsonrpc", "mcp",       transformer_jsonrpc_to_mcp_request },
    { "mcp",     "jsonrpc",   transformer_mcp_to_jsonrpc_response },
    { "jsonrpc", "a2a",       transformer_jsonrpc_to_a2a_task },
    { "a2a",     "jsonrpc",   transformer_a2a_to_jsonrpc_response },
    { "jsonrpc", "openai",     transformer_jsonrpc_to_openai_chat },
    { "openai",   "jsonrpc",   transformer_openai_chat_to_jsonrpc },
    { "jsonrpc", "openjiuwen", transformer_jsonrpc_to_openjiuwen },
    { "openjiuwen","jsonrpc",  transformer_openjiuwen_to_jsonrpc },
    { NULL, NULL, NULL }
};

int protocol_auto_transform(const unified_message_t* source,
                           unified_message_t* target,
                           const char* target_protocol_name) {
    if (!source || !target || !target_protocol_name) return -1;

    const char* from = (source->endpoint && source->endpoint[0]) ? source->endpoint : "jsonrpc";

    for (int i = 0; g_transform_table[i].from_proto != NULL; i++) {
        if (strcasecmp(g_transform_table[i].from_proto, from) == 0 &&
            strcasecmp(g_transform_table[i].to_proto, target_protocol_name) == 0) {

            transform_context_t* ctx = transform_context_create(from, target_protocol_name);
            int ret = g_transform_table[i].transform_fn(source, target, ctx);
            transform_context_destroy(ctx);
            return ret;
        }
    }

    memcpy(target, source, sizeof(*target));
    return 0;
}

int protocol_validate_transformed(const unified_message_t* msg) {
    if (!msg) return -1;

    if (msg->endpoint && msg->endpoint[0] == '\0') return -2;
    if (msg->payload == NULL && msg->payload_size > 0) return -3;
    if (msg->payload != NULL && msg->payload_size < 4) return -4;

    return 0;
}

const char** protocol_list_transformers(size_t* count) {
    static const char* names[] = {
        "jsonrpc→mcp", "mcp→jsonrpc",
        "jsonrpc→a2a", "a2a→jsonrpc",
        "jsonrpc→openai", "openai→jsonrpc",
        "jsonrpc→openjiuwen", "openjiuwen→jsonrpc",
        NULL
    };
    if (count) *count = 8;
    return (const char**)names;
}
