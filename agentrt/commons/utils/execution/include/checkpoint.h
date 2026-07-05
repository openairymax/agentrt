/**
 * @file checkpoint.h
 * @brief AgentRT 任务检查点接口
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
 *
 * "From data intelligence emerges."
 */

#ifndef AGENTRT_ATOMS_CHECKPOINT_H
#define AGENTRT_ATOMS_CHECKPOINT_H

#include "agentrt.h"

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
} agentrt_checkpoint_state_t;

typedef struct agentrt_task_checkpoint {
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
    agentrt_checkpoint_state_t state;
    uint32_t checksum;
    char metadata[512];
} agentrt_task_checkpoint_t;

typedef struct agentrt_checkpoint_stats {
    uint64_t total_checkpoints;
    uint64_t successful_checkpoints;
    uint64_t failed_checkpoints;
    uint64_t total_restore_ops;
    uint64_t last_checkpoint_time;
    uint64_t avg_checkpoint_size;
} agentrt_checkpoint_stats_t;

typedef void (*agentrt_checkpoint_hook_fn)(const char *task_id, const char *state_json,
                                           void *user_data);

AGENTRT_API agentrt_error_t agentrt_checkpoint_create(const char *task_id, const char *session_id,
                                                      uint64_t sequence_num, const char *state_json,
                                                      char **completed_nodes,
                                                      size_t completed_count, char **pending_nodes,
                                                      size_t pending_count,
                                                      agentrt_task_checkpoint_t **out_checkpoint);

AGENTRT_API agentrt_error_t agentrt_checkpoint_save(agentrt_task_checkpoint_t *checkpoint);
AGENTRT_API agentrt_error_t agentrt_checkpoint_restore(const char *task_id, uint64_t sequence_num,
                                                       agentrt_task_checkpoint_t **out_checkpoint);
AGENTRT_API agentrt_error_t agentrt_checkpoint_delete(const char *task_id, uint64_t sequence_num);
AGENTRT_API agentrt_error_t agentrt_checkpoint_list(const char *task_id,
                                                    agentrt_task_checkpoint_t ***out_checkpoints,
                                                    size_t *out_count);
AGENTRT_API agentrt_error_t agentrt_checkpoint_get_stats(agentrt_checkpoint_stats_t *out_stats);
AGENTRT_API agentrt_error_t agentrt_checkpoint_verify(const agentrt_task_checkpoint_t *checkpoint,
                                                      bool *is_valid);
AGENTRT_API agentrt_error_t agentrt_checkpoint_destroy(agentrt_task_checkpoint_t *checkpoint);
AGENTRT_API agentrt_error_t agentrt_checkpoint_init(const char *storage_path);
AGENTRT_API agentrt_error_t agentrt_checkpoint_shutdown(void);
AGENTRT_API agentrt_error_t agentrt_checkpoint_cleanup(uint64_t max_age_seconds, size_t max_count);
AGENTRT_API agentrt_error_t agentrt_snapshot_create(const char *task_id, const char *snapshot_path);
AGENTRT_API agentrt_error_t agentrt_snapshot_restore(const char *snapshot_path, char **task_id);

AGENTRT_API agentrt_error_t agentrt_checkpoint_set_auto_hook(agentrt_checkpoint_hook_fn hook,
                                                             void *user_data, uint64_t interval_ms);

AGENTRT_API agentrt_error_t agentrt_checkpoint_trigger_auto(const char *task_id);

#ifdef __cplusplus
}
#endif

#endif
