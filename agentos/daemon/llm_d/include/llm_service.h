/**
 * @file llm_service.h
 * @brief LLM 服务对外接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_LLM_SERVICE_H
#define AGENTOS_LLM_SERVICE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- 公共类型定义 ---------- */

typedef struct llm_service llm_service_t;

typedef struct {
    const char* role;
    const char* content;
} llm_message_t;

typedef struct {
    const char* model;
    const llm_message_t* messages;
    size_t message_count;
    float temperature;
    float top_p;
    int max_tokens;
    int stream;
    const char** stop;
    size_t stop_count;
    double presence_penalty;
    double frequency_penalty;
    void* user_data;
} llm_request_config_t;

typedef struct {
    char* id;
    char* model;
    llm_message_t* choices;
    size_t choice_count;
    uint64_t created;
    uint32_t prompt_tokens;
    uint32_t completion_tokens;
    uint32_t total_tokens;
    char* finish_reason;
} llm_response_t;

typedef void (*llm_stream_callback_t)(const char* chunk, void* user_data);

/* ---------- 生命周期 ---------- */

llm_service_t* llm_service_create(const char* config_path);
void llm_service_destroy(llm_service_t* svc);

/* ---------- 请求接口 ---------- */

int llm_service_complete(llm_service_t* svc,
                         const llm_request_config_t* manager,
                         llm_response_t** out_response);

int llm_service_complete_stream(llm_service_t* svc,
                                const llm_request_config_t* manager,
                                llm_stream_callback_t callback,
                                llm_response_t** out_response);

void llm_response_free(llm_response_t* resp);

/* ---------- 统计 ---------- */

int llm_service_stats(llm_service_t* svc, char** out_json);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_LLM_SERVICE_H */