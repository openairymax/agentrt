#ifndef AGENTOS_CONTEXT_PROCESSOR_H
#define AGENTOS_CONTEXT_PROCESSOR_H

#include "agentos.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct agentos_context_processor agentos_context_processor_t;
typedef struct agentos_context_engine agentos_context_engine_t;

typedef struct {
    size_t max_tokens;
    size_t target_tokens;
    float compression_ratio;
    int preserve_recent;
    const char* summarization_prompt;
} agentos_context_processor_config_t;

typedef struct {
    char* content;
    size_t content_len;
    char* metadata;
    size_t metadata_len;
    uint64_t timestamp_ns;
    uint32_t priority;
} agentos_context_entry_t;

typedef struct {
    agentos_context_entry_t* entries;
    size_t entry_count;
    size_t capacity;
    size_t total_content_len;
    size_t token_budget;
} agentos_model_context_t;

typedef agentos_error_t (*agentos_context_process_func_t)(
    agentos_context_processor_t* self,
    agentos_model_context_t* context,
    const agentos_context_processor_config_t* config);

typedef void (*agentos_context_processor_destroy_t)(agentos_context_processor_t* self);

struct agentos_context_processor {
    char* name;
    char* type;
    void* user_data;
    agentos_context_process_func_t process;
    agentos_context_processor_destroy_t destroy;
};

struct agentos_context_engine {
    agentos_context_processor_t** processors;
    size_t processor_count;
    size_t processor_capacity;
};

AGENTOS_API agentos_context_engine_t* agentos_context_engine_create(void);
AGENTOS_API void agentos_context_engine_destroy(agentos_context_engine_t* engine);
AGENTOS_API agentos_error_t agentos_context_engine_register_processor(
    agentos_context_engine_t* engine,
    agentos_context_processor_t* processor);
AGENTOS_API agentos_error_t agentos_context_engine_process(
    agentos_context_engine_t* engine,
    agentos_model_context_t* context,
    const agentos_context_processor_config_t* config);

AGENTOS_API agentos_model_context_t* agentos_model_context_create(size_t capacity);
AGENTOS_API void agentos_model_context_destroy(agentos_model_context_t* ctx);
AGENTOS_API agentos_error_t agentos_model_context_add_entry(
    agentos_model_context_t* ctx,
    const char* content,
    size_t content_len,
    const char* metadata,
    uint32_t priority);

AGENTOS_API agentos_context_processor_t* agentos_context_processor_window_trimmer(void);
AGENTOS_API agentos_context_processor_t* agentos_context_processor_compressor(void);
AGENTOS_API agentos_context_processor_t* agentos_context_processor_summarizer(void);
AGENTOS_API agentos_context_processor_t* agentos_context_processor_memory_augmenter(void);

#ifdef __cplusplus
}
#endif

#endif
