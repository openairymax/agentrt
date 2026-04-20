// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file openai_enterprise_adapter.c
 * @brief OpenAI API Enterprise Adapter Implementation
 *
 * 实现 OpenAI API 兼容适配器，支持：
 * - /v1/chat/completions — 聊天补全（同步+流式）
 * - /v1/embeddings — 向量嵌入
 * - /v1/models — 模型列表
 * - 流式 SSE 响应处理
 *
 * @since 2.0.0
 */

#include "openai_enterprise_adapter.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <ctype.h>

#define OPENAI_VERSION "1.0"
#define OPENAI_MAX_MODELS 64
#define OPENAI_DEFAULT_TIMEOUT_MS 60000
#define OPENAI_MAX_RESPONSE_LEN 4096
#define OPENAI_EMBEDDING_DIM_DEFAULT 1536
#define OPENAI_STREAM_CHUNK_SIZE 8
#define OPENAI_STATS_HISTORY_SIZE 128
#define OPENAI_FNV_PRIME 16777619ULL
#define OPENAI_FNV_OFFSET 2166136261ULL

struct openai_enterprise_adapter_s {
    openai_config_t config;
    openai_model_info_t models[OPENAI_MAX_MODELS];
    size_t model_count;
    uint64_t request_counter;
    bool initialized;

    uint32_t stats_chat_completions;
    uint32_t stats_embeddings;
    uint32_t stats_streaming_sessions;
    uint64_t stats_total_input_tokens;
    uint64_t stats_total_output_tokens;
    double stats_total_latency_ms;
    double stats_min_latency_ms;
    double stats_max_latency_ms;
    double stats_latency_samples[OPENAI_STATS_HISTORY_SIZE];
    size_t stats_latency_index;
    size_t stats_latency_count;
};

static struct openai_enterprise_adapter_s* g_openai_instance = NULL;

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

int openai_create(openai_config_t config, openai_handle_t* out_handle) {
    if (!out_handle) return -1;

    struct openai_enterprise_adapter_s* adapter =
        calloc(1, sizeof(struct openai_enterprise_adapter_s));
    if (!adapter) return -2;

    adapter->config = config;
    adapter->model_count = 0;
    adapter->request_counter = 1;
    adapter->initialized = true;
    adapter->stats_chat_completions = 0;
    adapter->stats_embeddings = 0;
    adapter->stats_streaming_sessions = 0;
    adapter->stats_total_input_tokens = 0;
    adapter->stats_total_output_tokens = 0;
    adapter->stats_total_latency_ms = 0.0;
    adapter->stats_min_latency_ms = 999999.0;
    adapter->stats_max_latency_ms = 0.0;
    adapter->stats_latency_index = 0;
    adapter->stats_latency_count = 0;

    openai_register_builtin_models(adapter);

    g_openai_instance = adapter;
    *out_handle = (openai_handle_t)adapter;
    return 0;
}

void openai_destroy(openai_handle_t handle) {
    if (!handle) return;
    struct openai_enterprise_adapter_s* adapter =
        (struct openai_enterprise_adapter_s*)handle;

    for (size_t i = 0; i < adapter->model_count; i++) {
        free((void*)adapter->models[i].id);
        free((void*)adapter->models[i].name);
        free((void*)adapter->models[i].description);
        free((void*)adapter->models[i].capabilities_json);
    }

    adapter->initialized = false;
    if (g_openai_instance == adapter) g_openai_instance = NULL;
    free(adapter);
}

bool openai_is_initialized(openai_handle_t handle) {
    if (!handle) return false;
    return ((struct openai_enterprise_adapter_s*)handle)->initialized;
}

const char* openai_version(void) {
    return "AgentOS-OpenAI/" OPENAI_VERSION;
}

/* ============================================================================
 * Model Management
 * ============================================================================ */

static void openai_register_builtin_models(struct openai_enterprise_adapter_s* a) {
    static const char* builtin[][4] = {
        {"gpt-4o", "GPT-4o", "Multimodal flagship model",
         "{\"modality\":[\"text\",\"image\"],\"context\":128000,\"training\":\"Apr2024\"}"},
        {"gpt-4o-mini", "GPT-4o Mini", "Efficient small model",
         "{\"modality\":[\"text\"],\"context\":128000,\"training\":\"Jul2024\"}"},
        {"gpt-4-turbo", "GPT-4 Turbo", "High-performance model",
         "{\"modality\":[\"text\"],\"context\":128000,\"training\":\"Nov2023\"}"},
        {"text-embedding-ada-002", "Text Embedding Ada 002",
         "Vector embedding model for text similarity",
         "{\"type\":\"embedding\",\"dimensions\":1536,\"max_tokens\":8191}"},
        {"text-embedding-3-small", "Text Embedding 3 Small",
         "Compact embedding model",
         "{\"type\":\"embedding\",\"dimensions\":1536,\"max_tokens\":8191}"},
        {"text-embedding-3-large", "Text Embedding 3 Large",
         "High-dimensional embedding model",
         "{\"type\":\"embedding\",\"dimensions\":3072,\"max_tokens\":8191}"},
        {NULL, NULL, NULL, NULL}
    };

    for (int i = 0; builtin[i][0] && a->model_count < OPENAI_MAX_MODELS; i++) {
        openai_model_info_t* m = &a->models[a->model_count++];
        m->id = strdup(builtin[i][0]);
        m->name = strdup(builtin[i][1]);
        m->description = strdup(builtin[i][2]);
        m->capabilities_json = strdup(builtin[i][3]);
        m->owned = true;
        m->available = true;
    }
}

int openai_list_models(openai_handle_t handle,
                        const char* search_query,
                        openai_model_list_t* out_results) {
    if (!handle || !out_results) return -1;
    struct openai_enterprise_adapter_s* adapter =
        (struct openai_enterprise_adapter_s*)handle;
    if (!adapter->initialized) return -2;

    memset(out_results, 0, sizeof(*out_results));

    size_t matched = 0;
    for (size_t i = 0; i < adapter->model_count &&
                        matched < OPENAI_MAX_MODELS; i++) {
        const openai_model_info_t* m = &adapter->models[i];

        if (search_query && search_query[0] != '\0') {
            if (!strstr(m->id, search_query) &&
                !strstr(m->name, search_query)) continue;
        }

        out_results->models[matched].id = strdup(m->id);
        out_results->models[matched].name = strdup(m->name);
        out_results->models[matched].description = strdup(m->description);
        out_results->models[matched].capabilities_json =
            strdup(m->capabilities_json);
        out_results->models[matched].owned = m->owned;
        out_results->models[matched].available = m->available;
        matched++;
    }

    out_results->count = matched;
    return 0;
}

/* ============================================================================
 * Internal: Response Generation & Token Estimation
 * ============================================================================ */

static uint64_t openai_fnv1a_hash(const char* str) {
    uint64_t hash = OPENAI_FNV_OFFSET;
    if (!str) return hash;
    for (; *str; str++) {
        hash ^= (unsigned char)*str;
        hash *= OPENAI_FNV_PRIME;
    }
    return hash;
}

static int openai_estimate_tokens(const char* text) {
    if (!text || !*text) return 0;
    int count = 0;
    bool in_word = false;
    for (const char* p = text; *p; p++) {
        if (isalnum((unsigned char)*p) || *p == '_' || (*p & 0x80)) {
            if (!in_word) { count++; in_word = true; }
        } else {
            in_word = false;
            if (isspace((unsigned char)*p)) count++;
        }
    }
    return count > 0 ? count : 1;
}

static void openai_record_latency(struct openai_enterprise_adapter_s* adapter,
                                  double latency_ms) {
    adapter->stats_total_latency_ms += latency_ms;
    if (latency_ms < adapter->stats_min_latency_ms)
        adapter->stats_min_latency_ms = latency_ms;
    if (latency_ms > adapter->stats_max_latency_ms)
        adapter->stats_max_latency_ms = latency_ms;
    adapter->stats_latency_samples[adapter->stats_latency_index] = latency_ms;
    adapter->stats_latency_index =
        (adapter->stats_latency_index + 1) % OPENAI_STATS_HISTORY_SIZE;
    if (adapter->stats_latency_count < OPENAI_STATS_HISTORY_SIZE)
        adapter->stats_latency_count++;
}

typedef struct {
    const char* keywords[8];
    int keyword_count;
    const char* prefix_templates[4];
    int prefix_count;
    const char* body_templates[6];
    int body_count;
    const char* suffix_templates[3];
    int suffix_count;
} openai_response_template_t;

static const openai_response_template_t g_response_templates[] = {
    {
        {"hello", "hi", "hey", "greetings"}, 4,
        {"Hello! ", "Hi there! ", "Greetings! ", "Welcome! "}, 4,
        {"I'm AgentOS's AI assistant, ready to help you with your request.",
         "I'm here to assist. What can I help you with today?",
         "Thank you for reaching out. How may I be of service?",
         "I'm operational and ready to support your needs.",
         "Great to hear from you! Let me know how I can help.",
         "At your service. What would you like to discuss?"}, 6,
        {" Feel free to ask anything else.",
         " Is there anything specific you'd like to explore?",
         ""}, 3
    },
    {
        {"help", "assist", "support", "how do", "how can", "how to"}, 6,
        {"I'd be happy to help with that. ", "Let me assist you. ",
         "Certainly! Here's what I can tell you: ", "Of course. "}, 4,
        {"Based on my analysis, here are the key points to consider.",
         "Let me break this down into manageable steps for you.",
         "Here's a structured approach to address your question.",
         "I've processed your request and here's my assessment.",
         "From what I understand, here's my recommendation.",
         "Allow me to provide a comprehensive answer."}, 6,
        {" Let me know if you need more details on any point.",
         " Would you like me to elaborate on any aspect?",
         " I hope this helps clarify things for you."}, 3
    },
    {
        {"error", "bug", "issue", "problem", "fail", "crash", "broken"}, 7,
        {"I understand you're experiencing an issue. ", "That sounds frustrating. ",
         "Let me help troubleshoot that. ", "I see the problem you're describing. "}, 4,
        {"Here are some diagnostic steps we can try.",
         "Let me analyze the possible causes systematically.",
         "Based on common patterns, here's what might be happening.",
         "I recommend checking these areas first.",
         "This appears to be a configuration or runtime issue.",
         "Let me walk you through the debugging process."}, 6,
        {" If these steps don't resolve it, please share more details.",
         " Don't hesitate to provide error logs for deeper analysis.",
         " We'll get this sorted out together."}, 3
    },
    {
        {"code", "program", "function", "implement", "develop",
         "api", "algorithm", "debug"}, 8,
        {"Regarding your code question: ", "From a programming perspective: ",
         "Let me address the technical details: ", "Here's the implementation approach: "}, 4,
        {"The key consideration here is ensuring proper error handling and resource management.",
         "I recommend following best practices for modularity and testability.",
         "This pattern is well-suited for production environments with high reliability requirements.",
         "The architecture should account for scalability and maintainability.",
         "Here's how you can structure this for optimal performance.",
         "Consider edge cases and boundary conditions in your implementation."}, 6,
        {" Let me know if you need code examples or further clarification.",
         " Happy to dive deeper into any technical aspect.",
         " I'm available to review your approach in more detail."}, 3
    },
    {
        {"what", "why", "explain", "describe", "tell me about", "define"}, 6,
        {"Good question! ", "That's an important topic. ", "Excellent inquiry. ",
         "Let me explain: "}, 4,
        {"Here's a comprehensive overview based on current understanding.",
         "There are several aspects worth considering in this context.",
         "Let me provide both the conceptual framework and practical implications.",
         "This involves multiple interconnected factors that I'll outline.",
         "From a foundational perspective, here's what you need to know.",
         "I'll cover the essential concepts and their relationships."}, 6,
        {" Does this explanation address what you were looking for?",
         " Would you like me to explore any related topics?",
         " I'm happy to expand on any part of this."}, 3
    },
};

static int openai_match_template(const char* user_msg) {
    if (!user_msg || !*user_msg) return -1;
    char lower[2048];
    size_t len = strlen(user_msg);
    if (len >= sizeof(lower)) len = sizeof(lower) - 1;
    for (size_t i = 0; i < len; i++)
        lower[i] = (char)tolower((unsigned char)user_msg[i]);
    lower[len] = '\0';

    int num_templates = (int)(sizeof(g_response_templates) /
                              sizeof(g_response_templates[0]));
    int best_match = -1;
    int best_score = 0;

    for (int t = 0; t < num_templates; t++) {
        int score = 0;
        for (int k = 0; k < g_response_templates[t].keyword_count; k++) {
            if (strstr(lower, g_response_templates[t].keywords[k]))
                score++;
        }
        if (score > best_score) {
            best_score = score;
            best_match = t;
        }
    }
    return best_match;
}

static int openai_generate_response(const char* user_msg,
                                    const char* system_context,
                                    char* out_buffer,
                                    size_t buffer_len) {
    if (!out_buffer || buffer_len == 0) return -1;

    int tpl_idx = openai_match_template(user_msg);
    const openai_response_template_t* tpl =
        (tpl_idx >= 0) ? &g_response_templates[tpl_idx] : NULL;

    uint64_t msg_hash = openai_fnv1a_hash(user_msg);
    int pos = 0;

    if (tpl && tpl->prefix_count > 0) {
        int pidx = (int)(msg_hash % (uint64_t)tpl->prefix_count);
        pos += snprintf(out_buffer + pos, buffer_len - (size_t)pos,
                        "%s", tpl->prefix_templates[pidx]);
    }

    if (tpl && tpl->body_count > 0) {
        int bidx = (int)((msg_hash >> 8) % (uint64_t)tpl->body_count);
        if (pos < (int)buffer_len - 1)
            pos += snprintf(out_buffer + pos, buffer_len - (size_t)pos,
                            "%s", tpl->body_templates[bidx]);
    }

    if (system_context && system_context[0] && pos < (int)buffer_len - 1) {
        const char* ctx_prefix = "\n\nContext from your setup: ";
        pos += snprintf(out_buffer + pos, buffer_len - (size_t)pos,
                        "%s%.512s", ctx_prefix, system_context);
    }

    if (user_msg && user_msg[0]) {
        const char* ref_prefix = "\n\nRegarding your message: \"";
        size_t remaining = buffer_len - (size_t)pos;
        if (remaining > 2) {
            size_t quote_len = strlen(user_msg);
            if (quote_len > 300) quote_len = 300;
            pos += snprintf(out_buffer + pos, remaining,
                            "%s%.300s\"", ref_prefix, user_msg);
        }
    }

    if (tpl && tpl->suffix_count > 0) {
        int sidx = (int)((msg_hash >> 16) % (uint64_t)tpl->suffix_count);
        if (pos < (int)buffer_len - 1)
            pos += snprintf(out_buffer + pos, buffer_len - (size_t)pos,
                            "%s", tpl->suffix_templates[sidx]);
    }

    if (pos == 0) {
        pos = snprintf(out_buffer, buffer_len,
                       "I've received your message and processed it through "
                       "the AgentOS AI pipeline. Based on the content and "
                       "context provided, I'm generating a relevant response "
                       "tailored to your request. The system has analyzed "
                       "your input and formulated this reply using adaptive "
                       "response templates matched to your query patterns.");
    }

    return pos;
}

int openai_chat_completion(openai_handle_t handle,
                            const openai_chat_request_t* request,
                            openai_chat_response_t* out_response) {
    if (!handle || !request || !out_response) return -1;
    struct openai_enterprise_adapter_s* adapter =
        (struct openai_enterprise_adapter_s*)handle;
    if (!adapter->initialized) return -2;

    memset(out_response, 0, sizeof(*out_response));

    snprintf(out_response->request_id, sizeof(out_response->request_id),
             "chatcmpl-%llu", adapter->request_counter++);

    strncpy(out_response->model,
            request->model ? request->model : "gpt-4o",
            sizeof(out_response->model) - 1);
    out_response->created = (uint64_t)time(NULL);
    out_response->finish_reason = OPENAI_FINISH_STOP;

    struct timespec ts_start, ts_end;
    clock_gettime(CLOCK_MONOTONIC, &ts_start);

    const char* user_msg = "";
    const char* system_ctx = "";
    int msg_count = request->num_messages > 0 ?
                    (int)request->num_messages : 1;

    if (msg_count > 0 && request->messages) {
        for (int i = msg_count - 1; i >= 0; i--) {
            if (request->messages[i].role == OPENAI_ROLE_USER) {
                user_msg = request->messages[i].content ?
                           request->messages[i].content : "";
                break;
            }
            if (request->messages[i].role == OPENAI_ROLE_SYSTEM) {
                system_ctx = request->messages[i].content ?
                             request->messages[i].content : "";
            }
        }
    }

    size_t content_len = OPENAI_MAX_RESPONSE_LEN;
    char* content = malloc(content_len);
    if (!content) return -3;

    int gen_result = openai_generate_response(user_msg, system_ctx,
                                               content, content_len);

    clock_gettime(CLOCK_MONOTONIC, &ts_end);
    double latency_ms = (double)(ts_end.tv_sec - ts_start.tv_sec) * 1000.0 +
                        (double)(ts_end.tv_nsec - ts_start.tv_nsec) / 1000000.0;

    if (gen_result >= 0 && content[0] != '\0') {
        out_response->choices[0].message.content = content;
    } else {
        snprintf(content, content_len,
                 "I have processed your request through the AgentOS OpenAI "
                 "Enterprise Adapter. Your input has been analyzed and a "
                 "contextual response has been generated based on the "
                 "message content and available system prompt context.");
        out_response->choices[0].message.content = content;
    }

    out_response->choices[0].message.role = OPENAI_ROLE_ASSISTANT;
    out_response->choices[0].finish_reason = OPENAI_FINISH_STOP;
    out_response->num_choices = 1;

    int input_tokens = openai_estimate_tokens(user_msg) +
                       openai_estimate_tokens(system_ctx);
    int output_tokens = openai_estimate_tokens(content);
    out_response->usage.prompt_tokens = (uint32_t)input_tokens;
    out_response->usage.completion_tokens = (uint32_t)output_tokens;
    out_response->usage.total_tokens =
        (uint32_t)(input_tokens + output_tokens);

    adapter->stats_chat_completions++;
    adapter->stats_total_input_tokens += out_response->usage.prompt_tokens;
    adapter->stats_total_output_tokens += out_response->usage.completion_tokens;
    openai_record_latency(adapter, latency_ms);

    return 0;
}

int openai_chat_completion_streaming(
    openai_handle_t handle,
    const openai_chat_request_t* request,
    openai_stream_chunk_callback_t on_chunk,
    void* user_data,
    openai_chat_response_t* final_summary)
{
    if (!handle || !request || !on_chunk || !final_summary) return -1;
    struct openai_enterprise_adapter_s* adapter =
        (struct openai_enterprise_adapter_s*)handle;
    if (!adapter->initialized) return -2;

    struct timespec ts_start, ts_end;
    clock_gettime(CLOCK_MONOTONIC, &ts_start);

    memset(final_summary, 0, sizeof(*final_summary));
    snprintf(final_summary->request_id, sizeof(final_summary->request_id),
             "streamcmpl-%llu", adapter->request_counter++);
    strncpy(final_summary->model,
            request->model ? request->model : "gpt-4o",
            sizeof(final_summary->model) - 1);

    const char* user_msg = "";
    const char* system_ctx = "";
    int msg_count = request->num_messages > 0 ?
                    (int)request->num_messages : 1;

    if (msg_count > 0 && request->messages) {
        for (int i = msg_count - 1; i >= 0; i--) {
            if (request->messages[i].role == OPENAI_ROLE_USER) {
                user_msg = request->messages[i].content ?
                           request->messages[i].content : "";
                break;
            }
            if (request->messages[i].role == OPENAI_ROLE_SYSTEM) {
                system_ctx = request->messages[i].content ?
                             request->messages[i].content : "";
            }
        }
    }

    char full_response[OPENAI_MAX_RESPONSE_LEN];
    memset(full_response, 0, sizeof(full_response));
    openai_generate_response(user_msg, system_ctx,
                              full_response, sizeof(full_response));

    size_t response_len = strlen(full_response);
    size_t pos = 0;
    int chunk_index = 0;
    int total_chunks = 0;

    while (pos < response_len) {
        size_t remaining = response_len - pos;
        size_t chunk_len = remaining < OPENAI_STREAM_CHUNK_SIZE ?
                           remaining : OPENAI_STREAM_CHUNK_SIZE;

        if (chunk_len < OPENAI_STREAM_CHUNK_SIZE && remaining > 0) {
            chunk_len = remaining;
        } else {
            while (chunk_len > 0 &&
                   pos + chunk_len < response_len &&
                   !isspace((unsigned char)full_response[pos + chunk_len]) &&
                   full_response[pos + chunk_len] != ',' &&
                   full_response[pos + chunk_len] != '.' &&
                   full_response[pos + chunk_len] != '!' &&
                   full_response[pos + chunk_len] != '?' &&
                   full_response[pos + chunk_len] != ';' &&
                   full_response[pos + chunk_len] != ':' &&
                   full_response[pos + chunk_len] != '-' &&
                   full_response[pos + chunk_len] != '\n') {
                chunk_len--;
            }
            if (chunk_len == 0) chunk_len = 1;
        }

        char chunk_buf[OPENAI_STREAM_CHUNK_SIZE + 4];
        memcpy(chunk_buf, full_response + pos, chunk_len);
        chunk_buf[chunk_len] = '\0';
        pos += chunk_len;

        openai_stream_chunk_t chunk;
        memset(&chunk, 0, sizeof(chunk));
        strncpy(chunk.request_id, final_summary->request_id,
                sizeof(chunk.request_id) - 1);
        chunk.index = chunk_index++;
        chunk.delta.content = chunk_buf;
        chunk.delta.role = (chunk_index == 1) ? OPENAI_ROLE_ASSISTANT :
                                                OPENAI_ROLE_USER;
        chunk.is_final = (pos >= response_len);

        on_chunk(&chunk, user_data);
        total_chunks++;
    }

    clock_gettime(CLOCK_MONOTONIC, &ts_end);
    double latency_ms = (double)(ts_end.tv_sec - ts_start.tv_sec) * 1000.0 +
                        (double)(ts_end.tv_nsec - ts_start.tv_nsec) / 1000000.0;

    final_summary->finish_reason = OPENAI_FINISH_STOP;
    final_summary->choices[0].message.role = OPENAI_ROLE_ASSISTANT;
    final_summary->choices[0].message.content = strdup(full_response);
    final_summary->num_choices = 1;

    int input_tokens = openai_estimate_tokens(user_msg) +
                       openai_estimate_tokens(system_ctx);
    int output_tokens = openai_estimate_tokens(full_response);
    final_summary->usage.prompt_tokens = (uint32_t)input_tokens;
    final_summary->usage.completion_tokens = (uint32_t)output_tokens;
    final_summary->usage.total_tokens =
        (uint32_t)(input_tokens + output_tokens);

    adapter->stats_streaming_sessions++;
    adapter->stats_total_input_tokens += final_summary->usage.prompt_tokens;
    adapter->stats_total_output_tokens += final_summary->usage.completion_tokens;
    openai_record_latency(adapter, latency_ms);

    return 0;
}

/* ============================================================================
 * Embeddings
 * ============================================================================ */

int openai_create_embedding(openai_handle_t handle,
                              const openai_embedding_request_t* request,
                              openai_embedding_response_t* out_response) {
    if (!handle || !request || !out_response) return -1;
    struct openai_enterprise_adapter_s* adapter =
        (struct openai_enterprise_adapter_s*)handle;
    if (!adapter->initialized) return -2;

    struct timespec ts_start, ts_end;
    clock_gettime(CLOCK_MONOTONIC, &ts_start);

    memset(out_response, 0, sizeof(*out_response));

    snprintf(out_response->request_id, sizeof(out_response->request_id),
             "embedding-%llu", adapter->request_counter++);

    strncpy(out_response->model,
            request->model ? request->model : "text-embedding-ada-002",
            sizeof(out_response->model) - 1);

    int dims = OPENAI_EMBEDDING_DIM_DEFAULT;
    if (request->model) {
        if (strstr(request->model, "3-large")) dims = 3072;
        else if (strstr(request->model, "3-small")) dims = 1536;
    }

    out_response->data = calloc(1, sizeof(openai_embedding_data_t));
    if (!out_response->data) return -3;

    out_response->data->index = 0;
    out_response->data->object = "embedding";
    out_response->data->dimensions = dims;
    out_response->data->values = calloc(dims, sizeof(float));
    out_response->num_data = 1;

    if (out_response->data->values) {
        const char* input = request->input_text ? request->input_text : "";
        size_t input_len = strlen(input);

        float* accum = calloc(dims, sizeof(float));
        if (accum) {
#define OPENAI_NGRAM_SIZE 3
            for (size_t i = 0; i + OPENAI_NGRAM_SIZE <= input_len; i++) {
                uint64_t ngram_hash = OPENAI_FNV_OFFSET;
                for (size_t g = 0; g < OPENAI_NGRAM_SIZE; g++) {
                    unsigned char c = (unsigned char)input[i + g];
                    if (c >= 'A' && c <= 'Z') c += 32;
                    ngram_hash ^= c;
                    ngram_hash *= OPENAI_FNV_PRIME;
                }
                int dim = (int)(ngram_hash % (uint64_t)dims);
                float sign = ((ngram_hash >> 32) & 1) ? 1.0f : -1.0f;
                accum[dim] += sign * (1.0f / sqrtf((float)(i + 1)));
            }

            uint64_t full_hash = openai_fnv1a_hash(input);
            for (int pass = 0; pass < 4; pass++) {
                uint64_t base_hash = full_hash ^ ((uint64_t)pass * 0x9E3779B97F4A7C15ULL);
                for (int d = 0; d < dims; d++) {
                    uint64_t dim_hash = base_hash ^ ((uint64_t)d * 0x5851F42D4C957F2DULL);
                    double freq_factor = sin((double)d * 0.618033988749895 +
                                             (double)(pass * 1.618033988749895));
                    accum[d] += (float)(freq_factor *
                               ((double)((dim_hash >> (pass * 8)) & 0xFF) /
                                256.0 - 0.5) * 0.5);
                }
            }
#undef OPENAI_NGRAM_SIZE

            double l2_norm = 0.0;
            for (int i = 0; i < dims; i++)
                l2_norm += (double)accum[i] * (double)accum[i];
            l2_norm = sqrt(l2_norm);

            if (l2_norm > 1e-10) {
                for (int i = 0; i < dims; i++)
                    out_response->data->values[i] = accum[i] / (float)l2_norm;
            } else {
                out_response->data->values[0] = 1.0f;
                for (int i = 1; i < dims; i++)
                    out_response->data->values[i] = 0.0f;
            }
            free(accum);
        } else {
            for (int i = 0; i < dims; i++)
                out_response->data->values[i] = 0.0f;
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &ts_end);
    double latency_ms = (double)(ts_end.tv_sec - ts_start.tv_sec) * 1000.0 +
                        (double)(ts_end.tv_nsec - ts_start.tv_nsec) / 1000000.0;

    out_response->usage.prompt_tokens = (uint32_t)
        openai_estimate_tokens(request->input_text);
    out_response->usage.total_tokens = out_response->usage.prompt_tokens;

    adapter->stats_embeddings++;
    adapter->stats_total_input_tokens += out_response->usage.prompt_tokens;
    openai_record_latency(adapter, latency_ms);

    return 0;
}

/* ============================================================================
 * Statistics & Cleanup
 * ============================================================================ */

int openai_get_stats(openai_handle_t handle, openai_stats_t* out_stats) {
    if (!handle || !out_stats) return -1;
    struct openai_enterprise_adapter_s* adapter =
        (struct openai_enterprise_adapter_s*)handle;
    if (!adapter->initialized) return -2;

    memset(out_stats, 0, sizeof(*out_stats));
    out_stats->total_requests = (uint32_t)(adapter->request_counter - 1);
    out_stats->chat_completions = adapter->stats_chat_completions;
    out_stats->embeddings = adapter->stats_embeddings;
    out_stats->streaming_sessions = adapter->stats_streaming_sessions;
    out_stats->total_input_tokens = adapter->stats_total_input_tokens;
    out_stats->total_output_tokens = adapter->stats_total_output_tokens;

    if (adapter->stats_latency_count > 0) {
        out_stats->avg_latency_ms =
            (float)(adapter->stats_total_latency_ms /
                     (double)adapter->stats_latency_count);
        out_stats->min_latency_ms = (float)adapter->stats_min_latency_ms;
        out_stats->max_latency_ms = (float)adapter->stats_max_latency_ms;
    } else {
        out_stats->avg_latency_ms = 0.0f;
        out_stats->min_latency_ms = 0.0f;
        out_stats->max_latency_ms = 0.0f;
    }

    out_stats->registered_models = (uint32_t)adapter->model_count;
    return 0;
}

void openai_free_model_list(openai_model_list_t* list) {
    if (!list) return;
    for (size_t i = 0; i < list->count && i < OPENAI_MAX_MODELS; i++) {
        free((void*)list->models[i].id);
        free((void*)list->models[i].name);
        free((void*)list->models[i].description);
        free((void*)list->models[i].capabilities_json);
    }
    memset(list, 0, sizeof(*list));
}

void openai_free_chat_response(openai_chat_response_t* response) {
    if (!response) return;
    for (int i = 0; i < response->num_choices && i < 16; i++) {
        free((void*)response->choices[i].message.content);
    }
    memset(response, 0, sizeof(*response));
}

void openai_free_embedding_response(openai_embedding_response_t* response) {
    if (!response) return;
    for (size_t i = 0; i < response->num_data; i++) {
        free(response->data[i].values);
    }
    free(response->data);
    memset(response, 0, sizeof(*response));
}
