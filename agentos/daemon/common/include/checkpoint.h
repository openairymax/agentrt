/**
 * @file checkpoint.h
 * @brief AgentRT 任务检查点接口
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *
 * "From data intelligence emerges."
 */

#ifndef AGENTOS_ATOMS_CHECKPOINT_H
#define AGENTOS_ATOMS_CHECKPOINT_H

#include "agentos.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CHECKPOINT_STATE_PENDING = 0,
    CHECKPOINT_STATE_COMPLETED = 1,
    CHECKPOINT_STATE_FAILED = 2,
    CHECKPOINT_STATE_INVALID = 3
} agentos_checkpoint_state_t;

typedef struct agentos_task_checkpoint {
    char task_id[128];
    char session_id[128];
    uint64_t sequence_num;
    uint64_t timestamp;
    char *state_json;
    size_t state_size;
    char **completed_nodes;
    size_t completed_count;
    char **pending_nodes;
    size_t pending_count;
    agentos_checkpoint_state_t state;
    uint32_t checksum;
    char metadata[512];
} agentos_task_checkpoint_t;

typedef struct agentos_checkpoint_stats {
    uint64_t total_checkpoints;
    uint64_t successful_checkpoints;
    uint64_t failed_checkpoints;
    uint64_t total_restore_ops;
    uint64_t last_checkpoint_time;
    uint64_t avg_checkpoint_size;
} agentos_checkpoint_stats_t;

typedef void (*agentos_checkpoint_hook_fn)(const char *task_id, const char *state_json,
                                           void *user_data);

AGENTOS_API agentos_error_t agentos_checkpoint_create(const char *task_id, const char *session_id,
                                                      uint64_t sequence_num, const char *state_json,
                                                      char **completed_nodes,
                                                      size_t completed_count, char **pending_nodes,
                                                      size_t pending_count,
                                                      agentos_task_checkpoint_t **out_checkpoint);

AGENTOS_API agentos_error_t agentos_checkpoint_save(agentos_task_checkpoint_t *checkpoint);
AGENTOS_API agentos_error_t agentos_checkpoint_restore(const char *task_id, uint64_t sequence_num,
                                                       agentos_task_checkpoint_t **out_checkpoint);
AGENTOS_API agentos_error_t agentos_checkpoint_delete(const char *task_id, uint64_t sequence_num);
AGENTOS_API agentos_error_t agentos_checkpoint_list(const char *task_id,
                                                    agentos_task_checkpoint_t ***out_checkpoints,
                                                    size_t *out_count);
AGENTOS_API agentos_error_t agentos_checkpoint_get_stats(agentos_checkpoint_stats_t *out_stats);
AGENTOS_API agentos_error_t agentos_checkpoint_verify(const agentos_task_checkpoint_t *checkpoint,
                                                      bool *is_valid);
AGENTOS_API agentos_error_t agentos_checkpoint_destroy(agentos_task_checkpoint_t *checkpoint);
AGENTOS_API agentos_error_t agentos_checkpoint_init(const char *storage_path);
AGENTOS_API agentos_error_t agentos_checkpoint_shutdown(void);
AGENTOS_API agentos_error_t agentos_checkpoint_cleanup(uint64_t max_age_seconds, size_t max_count);
AGENTOS_API agentos_error_t agentos_snapshot_create(const char *task_id, const char *snapshot_path);
AGENTOS_API agentos_error_t agentos_snapshot_restore(const char *snapshot_path, char **task_id);

AGENTOS_API agentos_error_t agentos_checkpoint_set_auto_hook(agentos_checkpoint_hook_fn hook,
                                                             void *user_data, uint64_t interval_ms);

AGENTOS_API agentos_error_t agentos_checkpoint_trigger_auto(const char *task_id);

#ifdef __cplusplus
}
#endif

#endif
