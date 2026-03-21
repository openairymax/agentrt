/**
 * @file response.h
 * @brief 响应序列化接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef LLM_RESPONSE_H
#define LLM_RESPONSE_H

#include "llm_service.h"
#include <cjson/cJSON.h>

#ifdef __cplusplus
extern "C" {
#endif

char* response_to_json(const llm_response_t* resp);
llm_response_t* response_from_json(const char* json);

#ifdef __cplusplus
}
#endif

#endif /* LLM_RESPONSE_H */