#ifndef SYNC_INTERNAL_H
#define SYNC_INTERNAL_H

#include "sync.h"
#include "sync_types.h"

#include <stdint.h>

char *sync_internal_strdup(const char *str);
sync_result_t sync_internal_posix_error_to_result(int error_code);
void sync_internal_update_stats_lock(sync_stats_t *stats, int64_t elapsed_ms);
void sync_internal_update_stats_timeout(sync_stats_t *stats);
void sync_internal_update_stats_wait(sync_stats_t *stats, int64_t elapsed_ms);

#endif
