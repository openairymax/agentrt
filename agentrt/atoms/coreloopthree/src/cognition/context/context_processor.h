#ifndef AGENTRT_CONTEXT_PROCESSOR_H
#define AGENTRT_CONTEXT_PROCESSOR_H

#include "agentrt.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct agentrt_context_processor agentrt_context_processor_t;
typedef struct agentrt_context_engine agentrt_context_engine_t;

typedef struct {
    size_t max_tokens;
    size_t target_tokens;
    float compression_ratio;
    int preserve_recent;
    const char *summarization_prompt;
} agentrt_context_processor_config_t;

typedef struct {
    char *content;
    size_t content_len;
    char *metadata;
    size_t metadata_len;
    uint64_t timestamp_ns;
    uint32_t priority;
} agentrt_context_entry_t;

typedef struct {
    agentrt_context_entry_t *entries;
    size_t entry_count;
    size_t capacity;
    size_t total_content_len;
    size_t token_budget;
} agentrt_model_context_t;

typedef agentrt_error_t (*agentrt_context_process_func_t)(
    agentrt_context_processor_t *self, agentrt_model_context_t *context,
    const agentrt_context_processor_config_t *config);

typedef void (*agentrt_context_processor_destroy_t)(agentrt_context_processor_t *self);

struct agentrt_context_processor {
    char *name;
    char *type;
    void *user_data;
    agentrt_context_process_func_t process;
    agentrt_context_processor_destroy_t destroy;
};

struct agentrt_context_engine {
    agentrt_context_processor_t **processors;
    size_t processor_count;
    size_t processor_capacity;
};

AGENTRT_API agentrt_context_engine_t *agentrt_context_engine_create(void);
AGENTRT_API void agentrt_context_engine_destroy(agentrt_context_engine_t *engine);
AGENTRT_API agentrt_error_t agentrt_context_engine_register_processor(
    agentrt_context_engine_t *engine, agentrt_context_processor_t *processor);
AGENTRT_API agentrt_error_t
agentrt_context_engine_process(agentrt_context_engine_t *engine, agentrt_model_context_t *context,
                               const agentrt_context_processor_config_t *config);

AGENTRT_API agentrt_model_context_t *agentrt_model_context_create(size_t capacity);
AGENTRT_API void agentrt_model_context_destroy(agentrt_model_context_t *ctx);
AGENTRT_API agentrt_error_t agentrt_model_context_add_entry(agentrt_model_context_t *ctx,
                                                            const char *content, size_t content_len,
                                                            const char *metadata,
                                                            uint32_t priority);

AGENTRT_API agentrt_context_processor_t *agentrt_context_processor_window_trimmer(void);
AGENTRT_API agentrt_context_processor_t *agentrt_context_processor_compressor(void);
AGENTRT_API agentrt_context_processor_t *agentrt_context_processor_summarizer(void);
AGENTRT_API agentrt_context_processor_t *agentrt_context_processor_memory_augmenter(void);

#ifdef __cplusplus
}
#endif

#endif
