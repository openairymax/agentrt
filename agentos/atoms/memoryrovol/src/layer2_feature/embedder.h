/**
 * @file embedder.h
 * @brief L2 特征层嵌入器接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_EMBEDDER_H
#define AGENTOS_EMBEDDER_H

#include "agentos.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

agentos_error_t agentos_embedder_init(const char* api_key, const char* base_url, int dimension);

agentos_error_t agentos_embedder_embed(const char* text, float** out_embedding, size_t* out_dim);

void agentos_embedder_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_EMBEDDER_H */
