#include "context_processor.h"
#include "agentos.h"
#include <stdlib.h>
#include <string.h>

agentos_model_context_t* agentos_model_context_create(size_t capacity) {
    agentos_model_context_t* ctx = (agentos_model_context_t*)calloc(1, sizeof(agentos_model_context_t));
    if (!ctx) return NULL;
    ctx->capacity = capacity > 0 ? capacity : 64;
    ctx->entries = (agentos_context_entry_t*)calloc(ctx->capacity, sizeof(agentos_context_entry_t));
    if (!ctx->entries) { free(ctx); return NULL; }
    ctx->entry_count = 0;
    ctx->total_content_len = 0;
    ctx->token_budget = 4096;
    return ctx;
}

void agentos_model_context_destroy(agentos_model_context_t* ctx) {
    if (!ctx) return;
    if (ctx->entries) {
        for (size_t i = 0; i < ctx->entry_count; i++) {
            if (ctx->entries[i].content) free(ctx->entries[i].content);
            if (ctx->entries[i].metadata) free(ctx->entries[i].metadata);
        }
        free(ctx->entries);
    }
    free(ctx);
}

agentos_error_t agentos_model_context_add_entry(
    agentos_model_context_t* ctx,
    const char* content,
    size_t content_len,
    const char* metadata,
    uint32_t priority) {
    if (!ctx || !content) return AGENTOS_EINVAL;
    if (ctx->entry_count >= ctx->capacity) {
        size_t new_cap = ctx->capacity * 2;
        agentos_context_entry_t* new_entries = (agentos_context_entry_t*)realloc(
            ctx->entries, new_cap * sizeof(agentos_context_entry_t));
        if (!new_entries) return AGENTOS_ENOMEM;
        memset(new_entries + ctx->capacity, 0, (new_cap - ctx->capacity) * sizeof(agentos_context_entry_t));
        ctx->entries = new_entries;
        ctx->capacity = new_cap;
    }
    agentos_context_entry_t* entry = &ctx->entries[ctx->entry_count];
    entry->content = (char*)malloc(content_len + 1);
    if (!entry->content) return AGENTOS_ENOMEM;
    memcpy(entry->content, content, content_len);
    entry->content[content_len] = '\0';
    entry->content_len = content_len;
    if (metadata) {
        size_t meta_len = strlen(metadata);
        entry->metadata = (char*)malloc(meta_len + 1);
        if (entry->metadata) {
            memcpy(entry->metadata, metadata, meta_len + 1);
            entry->metadata_len = meta_len;
        }
    }
    entry->priority = priority;
    entry->timestamp_ns = 0;
    ctx->total_content_len += content_len;
    ctx->entry_count++;
    return AGENTOS_SUCCESS;
}

static agentos_error_t window_trimmer_process(
    agentos_context_processor_t* self,
    agentos_model_context_t* context,
    const agentos_context_processor_config_t* config) {
    (void)self;
    if (!context || !config) return AGENTOS_EINVAL;

    size_t budget = config->target_tokens > 0 ? config->target_tokens : 2048;
    size_t approx_chars = budget * 4;
    if (context->total_content_len <= approx_chars) return AGENTOS_SUCCESS;

    size_t preserve_recent = config->preserve_recent > 0 ? (size_t)config->preserve_recent : 1;
    if (preserve_recent > context->entry_count) preserve_recent = context->entry_count;

    size_t recent_start = context->entry_count - preserve_recent;
    size_t recent_chars = 0;
    for (size_t i = recent_start; i < context->entry_count; i++) {
        recent_chars += context->entries[i].content_len;
    }

    size_t remaining_budget = approx_chars > recent_chars ? approx_chars - recent_chars : 0;
    size_t kept_count = 0;
    size_t kept_chars = 0;
    for (size_t i = 0; i < recent_start && kept_chars + context->entries[i].content_len <= remaining_budget; i++) {
        kept_chars += context->entries[i].content_len;
        kept_count++;
    }

    size_t new_count = kept_count + preserve_recent;
    if (kept_count < recent_start) {
        for (size_t i = kept_count; i < recent_start; i++) {
            if (context->entries[i].content) free(context->entries[i].content);
            if (context->entries[i].metadata) free(context->entries[i].metadata);
            memset(&context->entries[i], 0, sizeof(agentos_context_entry_t));
        }
        memmove(context->entries + kept_count, context->entries + recent_start,
                preserve_recent * sizeof(agentos_context_entry_t));
        context->entry_count = new_count;
        context->total_content_len = kept_chars + recent_chars;
    }

    return AGENTOS_SUCCESS;
}

static agentos_error_t compressor_process(
    agentos_context_processor_t* self,
    agentos_model_context_t* context,
    const agentos_context_processor_config_t* config) {
    (void)self;
    if (!context || !config) return AGENTOS_EINVAL;

    float ratio = config->compression_ratio > 0.0f && config->compression_ratio < 1.0f
        ? config->compression_ratio : 0.5f;

    for (size_t i = 0; i < context->entry_count; i++) {
        agentos_context_entry_t* entry = &context->entries[i];
        if (!entry->content || entry->content_len < 256) continue;

        size_t target_len = (size_t)(entry->content_len * ratio);
        if (target_len < 64) target_len = 64;
        if (target_len >= entry->content_len) continue;

        size_t head_len = target_len / 2;
        size_t tail_len = target_len - head_len - 3;
        if (tail_len > entry->content_len - head_len) tail_len = entry->content_len - head_len;

        char* compressed = (char*)malloc(target_len + 1);
        if (!compressed) continue;
        memcpy(compressed, entry->content, head_len);
        compressed[head_len] = '.';
        compressed[head_len + 1] = '.';
        compressed[head_len + 2] = '.';
        memcpy(compressed + head_len + 3, entry->content + entry->content_len - tail_len, tail_len);
        compressed[target_len] = '\0';

        size_t old_len = entry->content_len;
        free(entry->content);
        entry->content = compressed;
        entry->content_len = target_len;
        context->total_content_len -= (old_len - target_len);
    }

    return AGENTOS_SUCCESS;
}

static agentos_error_t summarizer_process(
    agentos_context_processor_t* self,
    agentos_model_context_t* context,
    const agentos_context_processor_config_t* config) {
    (void)self;
    if (!context || !config) return AGENTOS_EINVAL;

    size_t budget = config->target_tokens > 0 ? config->target_tokens : 2048;
    size_t approx_chars = budget * 4;

    if (context->entry_count <= 2 || context->total_content_len <= approx_chars) return AGENTOS_SUCCESS;

    size_t summary_max = 512;
    char* summary = (char*)malloc(summary_max);
    if (!summary) return AGENTOS_ENOMEM;

    size_t pos = 0;
    pos += snprintf(summary + pos, summary_max - pos, "[Summary of %zu earlier entries: ", context->entry_count - 1);
    for (size_t i = 0; i < context->entry_count - 1 && pos < summary_max - 64; i++) {
        agentos_context_entry_t* e = &context->entries[i];
        size_t preview = e->content_len < 50 ? e->content_len : 50;
        pos += snprintf(summary + pos, summary_max - pos, "\"%.50s\"; ", e->content ? e->content : "");
        (void)preview;
    }
    pos += snprintf(summary + pos, summary_max - pos, "]");

    for (size_t i = 0; i < context->entry_count - 1; i++) {
        if (context->entries[i].content) free(context->entries[i].content);
        if (context->entries[i].metadata) free(context->entries[i].metadata);
    }

    size_t old_total = context->total_content_len;
    context->entries[0].content = summary;
    context->entries[0].content_len = pos;
    context->entries[0].metadata = NULL;
    context->entries[0].metadata_len = 0;
    context->entries[0].priority = 0;

    memmove(context->entries + 1, context->entries + context->entry_count - 1, sizeof(agentos_context_entry_t));
    context->entry_count = 2;
    context->total_content_len = pos + context->entries[1].content_len;

    return AGENTOS_SUCCESS;
}

static agentos_error_t memory_augmenter_process(
    agentos_context_processor_t* self,
    agentos_model_context_t* context,
    const agentos_context_processor_config_t* config) {
    (void)self;
    if (!context || !config) return AGENTOS_EINVAL;
    if (context->entry_count == 0) return AGENTOS_SUCCESS;

    extern agentos_error_t agentos_sys_memory_search(const char* query, uint32_t limit,
                                                      char*** out_record_ids, float** out_scores,
                                                      size_t* out_count);
    char** record_ids = NULL;
    float* scores = NULL;
    size_t count = 0;

    const char* query = context->entries[context->entry_count - 1].content;
    if (!query) return AGENTOS_SUCCESS;

    agentos_error_t err = agentos_sys_memory_search(query, 3, &record_ids, &scores, &count);
    if (err != AGENTOS_SUCCESS || count == 0 || !record_ids) {
        if (record_ids) { extern void agentos_sys_free(void*); agentos_sys_free(record_ids); }
        if (scores) { extern void agentos_sys_free(void*); agentos_sys_free(scores); }
        return AGENTOS_SUCCESS;
    }

    size_t aug_max = 256 + count * 128;
    char* aug_content = (char*)malloc(aug_max);
    if (!aug_content) {
        extern void agentos_sys_free(void*);
        agentos_sys_free(record_ids);
        agentos_sys_free(scores);
        return AGENTOS_ENOMEM;
    }

    size_t pos = 0;
    pos += snprintf(aug_content + pos, aug_max - pos, "[Memory context: ");
    for (size_t i = 0; i < count && pos < aug_max - 64; i++) {
        pos += snprintf(aug_content + pos, aug_max - pos, "ref_%s(%.2f) ",
                        record_ids[i] ? record_ids[i] : "", scores[i]);
    }
    pos += snprintf(aug_content + pos, aug_max - pos, "]");

    agentos_model_context_add_entry(context, aug_content, pos, "memory_augmenter", 10);
    free(aug_content);

    extern void agentos_sys_free(void*);
    agentos_sys_free(record_ids);
    agentos_sys_free(scores);
    return AGENTOS_SUCCESS;
}

static void default_processor_destroy(agentos_context_processor_t* self) {
    if (!self) return;
    if (self->name) free(self->name);
    if (self->type) free(self->type);
    free(self);
}

static agentos_context_processor_t* create_processor(const char* name, const char* type,
                                                       agentos_context_process_func_t process_func) {
    agentos_context_processor_t* p = (agentos_context_processor_t*)calloc(1, sizeof(agentos_context_processor_t));
    if (!p) return NULL;
    p->name = strdup(name);
    p->type = strdup(type);
    p->process = process_func;
    p->destroy = default_processor_destroy;
    p->user_data = NULL;
    return p;
}

agentos_context_processor_t* agentos_context_processor_window_trimmer(void) {
    return create_processor("WindowTrimmer", "window_trimmer", window_trimmer_process);
}

agentos_context_processor_t* agentos_context_processor_compressor(void) {
    return create_processor("LLMCompressor", "compressor", compressor_process);
}

agentos_context_processor_t* agentos_context_processor_summarizer(void) {
    return create_processor("StructuredSummarizer", "summarizer", summarizer_process);
}

agentos_context_processor_t* agentos_context_processor_memory_augmenter(void) {
    return create_processor("MemoryAugmenter", "memory_augmenter", memory_augmenter_process);
}

agentos_context_engine_t* agentos_context_engine_create(void) {
    agentos_context_engine_t* engine = (agentos_context_engine_t*)calloc(1, sizeof(agentos_context_engine_t));
    if (!engine) return NULL;
    engine->processor_capacity = 8;
    engine->processors = (agentos_context_processor_t**)calloc(
        engine->processor_capacity, sizeof(agentos_context_processor_t*));
    if (!engine->processors) { free(engine); return NULL; }
    engine->processor_count = 0;
    return engine;
}

void agentos_context_engine_destroy(agentos_context_engine_t* engine) {
    if (!engine) return;
    if (engine->processors) {
        for (size_t i = 0; i < engine->processor_count; i++) {
            if (engine->processors[i] && engine->processors[i]->destroy) {
                engine->processors[i]->destroy(engine->processors[i]);
            }
        }
        free(engine->processors);
    }
    free(engine);
}

agentos_error_t agentos_context_engine_register_processor(
    agentos_context_engine_t* engine,
    agentos_context_processor_t* processor) {
    if (!engine || !processor) return AGENTOS_EINVAL;
    if (engine->processor_count >= engine->processor_capacity) {
        size_t new_cap = engine->processor_capacity * 2;
        agentos_context_processor_t** new_procs = (agentos_context_processor_t**)realloc(
            engine->processors, new_cap * sizeof(agentos_context_processor_t*));
        if (!new_procs) return AGENTOS_ENOMEM;
        engine->processors = new_procs;
        engine->processor_capacity = new_cap;
    }
    engine->processors[engine->processor_count++] = processor;
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_context_engine_process(
    agentos_context_engine_t* engine,
    agentos_model_context_t* context,
    const agentos_context_processor_config_t* config) {
    if (!engine || !context || !config) return AGENTOS_EINVAL;
    agentos_error_t err = AGENTOS_SUCCESS;
    for (size_t i = 0; i < engine->processor_count; i++) {
        if (engine->processors[i] && engine->processors[i]->process) {
            err = engine->processors[i]->process(engine->processors[i], context, config);
            if (err != AGENTOS_SUCCESS) break;
        }
    }
    return err;
}
