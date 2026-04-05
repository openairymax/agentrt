/**
 * @file response.c
 * @brief 响应序列化实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "response.h"
#include <stdlib.h>
#include <string.h>

char* response_to_json(const llm_response_t* resp) {
    if (!resp) return NULL;
    cJSON* root = cJSON_CreateObject();
    if (!root) return NULL;

    if (resp->id) cJSON_AddStringToObject(root, "id", resp->id);
    if (resp->model) cJSON_AddStringToObject(root, "model", resp->model);
    cJSON_AddNumberToObject(root, "created", (double)resp->created);
    cJSON_AddNumberToObject(root, "prompt_tokens", resp->prompt_tokens);
    cJSON_AddNumberToObject(root, "completion_tokens", resp->completion_tokens);
    cJSON_AddNumberToObject(root, "total_tokens", resp->total_tokens);
    if (resp->finish_reason) cJSON_AddStringToObject(root, "finish_reason", resp->finish_reason);

    cJSON* choices = cJSON_CreateArray();
    for (size_t i = 0; i < resp->choice_count; ++i) {
        cJSON* choice = cJSON_CreateObject();
        cJSON_AddStringToObject(choice, "role", resp->choices[i].role);
        cJSON_AddStringToObject(choice, "content", resp->choices[i].content);
        cJSON_AddItemToArray(choices, choice);
    }
    cJSON_AddItemToObject(root, "choices", choices);

    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}

llm_response_t* response_from_json(const char* json) {
    if (!json) return NULL;
    cJSON* root = cJSON_Parse(json);
    if (!root) return NULL;

    llm_response_t* resp = calloc(1, sizeof(llm_response_t));
    if (!resp) {
        cJSON_Delete(root);
        return NULL;
    }

    cJSON* id = cJSON_GetObjectItem(root, "id");
    if (cJSON_IsString(id)) resp->id = strdup(id->valuestring);

    cJSON* model = cJSON_GetObjectItem(root, "model");
    if (cJSON_IsString(model)) resp->model = strdup(model->valuestring);

    cJSON* created = cJSON_GetObjectItem(root, "created");
    if (cJSON_IsNumber(created)) resp->created = (uint64_t)created->valuedouble;

    cJSON* prompt_tokens = cJSON_GetObjectItem(root, "prompt_tokens");
    if (cJSON_IsNumber(prompt_tokens)) resp->prompt_tokens = (uint32_t)prompt_tokens->valuedouble;

    cJSON* completion_tokens = cJSON_GetObjectItem(root, "completion_tokens");
    if (cJSON_IsNumber(completion_tokens)) resp->completion_tokens = (uint32_t)completion_tokens->valuedouble;

    cJSON* total_tokens = cJSON_GetObjectItem(root, "total_tokens");
    if (cJSON_IsNumber(total_tokens)) resp->total_tokens = (uint32_t)total_tokens->valuedouble;

    cJSON* finish_reason = cJSON_GetObjectItem(root, "finish_reason");
    if (cJSON_IsString(finish_reason)) resp->finish_reason = strdup(finish_reason->valuestring);

    cJSON* choices = cJSON_GetObjectItem(root, "choices");
    if (cJSON_IsArray(choices)) {
        resp->choice_count = cJSON_GetArraySize(choices);
        resp->choices = calloc(resp->choice_count, sizeof(llm_message_t));
        for (size_t i = 0; i < resp->choice_count; ++i) {
            cJSON* choice = cJSON_GetArrayItem(choices, i);
            cJSON* role = cJSON_GetObjectItem(choice, "role");
            cJSON* content = cJSON_GetObjectItem(choice, "content");
            if (cJSON_IsString(role)) resp->choices[i].role = strdup(role->valuestring);
            if (cJSON_IsString(content)) resp->choices[i].content = strdup(content->valuestring);
        }
    }

    cJSON_Delete(root);
    return resp;
}