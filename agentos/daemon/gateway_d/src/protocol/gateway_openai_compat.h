#ifndef AGENTOS_GATEWAY_OPENAI_COMPAT_H
#define AGENTOS_GATEWAY_OPENAI_COMPAT_H

#include "gateway_protocol_router.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gw_openai_compat gw_openai_compat_t;

typedef struct {
    char default_model[128];
    uint32_t max_tokens_default;
    double temperature_default;
    double top_p_default;
    uint32_t rate_limit_rpm;
    uint32_t retry_max;
    uint32_t retry_base_ms;
} gw_openai_compat_config_t;

#define GW_OPENAI_COMPAT_CONFIG_DEFAULTS { \
    .default_model = "gpt-4", \
    .max_tokens_default = 4096, \
    .temperature_default = 0.7, \
    .top_p_default = 1.0, \
    .rate_limit_rpm = 60, \
    .retry_max = 3, \
    .retry_base_ms = 1000 }

typedef int (*gw_openai_llm_call_fn)(const char* model,
                                      const char* messages_json,
                                      const char* functions_json,
                                      double temperature,
                                      int max_tokens,
                                      char** response_json,
                                      void* user_data);

typedef int (*gw_openai_embed_fn)(const char* model,
                                   const char* input_json,
                                   char** response_json,
                                   void* user_data);

gw_openai_compat_t* gw_openai_compat_create(const gw_openai_compat_config_t* config);
void gw_openai_compat_destroy(gw_openai_compat_t* compat);

int gw_openai_compat_init(gw_openai_compat_t* compat);
int gw_openai_compat_shutdown(gw_openai_compat_t* compat);

int gw_openai_compat_set_llm_call(gw_openai_compat_t* compat,
                                   gw_openai_llm_call_fn fn,
                                   void* user_data);

int gw_openai_compat_set_embed_fn(gw_openai_compat_t* compat,
                                   gw_openai_embed_fn fn,
                                   void* user_data);

int gw_openai_compat_handle_request(gw_openai_compat_t* compat,
                                     const char* method,
                                     const char* path,
                                     const char* body_json,
                                     char** response_json);

gw_proto_request_handler_t gw_openai_compat_get_handler(gw_openai_compat_t* compat);
void* gw_openai_compat_get_handler_data(gw_openai_compat_t* compat);

bool gw_openai_compat_is_healthy(gw_openai_compat_t* compat);

#ifdef __cplusplus
}
#endif

#endif
