#ifndef GARBAGE_COLLECTOR_H
#define GARBAGE_COLLECTOR_H

#include "../../../commons/utils/memory/include/memory_compat.h"
#include "agentos.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GC_MAGIC_ALIVE     0xDEADBEEFCAFEULL
#define GC_MAGIC_COLLECTED 0xC011EC7EDULL

typedef enum {
    GC_GEN_YOUNG = 0,
    GC_GEN_OLD   = 1,
    GC_GEN_COUNT = 2
} gc_generation_t;

enum {
    GC_COLOR_WHITE = 0,
    GC_COLOR_GRAY  = 1,
    GC_COLOR_BLACK = 2
};

typedef enum {
    GC_TRIGGER_MANUAL    = 0,
    GC_TRIGGER_PRESSURE  = 1,
    GC_TRIGGER_TIME      = 2,
    GC_TRIGGER_ALLOC     = 3
} gc_trigger_t;

typedef struct gc_stats {
    uint64_t total_collections;
    uint64_t young_collections;
    uint64_t old_collections;
    uint64_t objects_reclaimed;
    uint64_t bytes_reclaimed;
    uint64_t objects_promoted;
    uint64_t cycle_detected;
    uint64_t weak_ref_cleared;
    uint64_t collect_time_ns;
    uint64_t last_collect_ns;
    double   avg_pause_ns;
    uint8_t  generation_gc_counts[GC_GEN_COUNT];
} gc_stats_t;

typedef struct gc_config {
    double   young_pressure_threshold;
    double   old_pressure_threshold;
    uint32_t young_promote_threshold;
    uint32_t time_interval_sec;
    uint32_t max_collect_time_ms;
    bool     enable_concurrent_mark;
    bool     enable_generational;
    bool     enable_cycle_detection;
} gc_config_t;

typedef struct gc_object_header {
    uint64_t magic;
    uint64_t size;
    uint32_t ref_count;
    uint32_t weak_ref_count;
    uint8_t  color;
    uint8_t  generation;
    uint16_t flags;
    void*    user_data;
    void (*finalizer)(void* user_data);
    uint64_t last_access_ns;
    struct gc_object_header* next;
    struct gc_object_header* prev;
    struct gc_object_header* hash_next;
} gc_object_header_t;

typedef struct gc_weak_ref {
    void* target;
    void** slot;
    struct gc_weak_ref* next;
} gc_weak_ref_t;

typedef struct gc_root {
    void** location;
    const char* name;
    struct gc_root* next;
} gc_root_t;

struct garbage_collector;
typedef struct garbage_collector garbage_collector_t;

garbage_collector_t* gc_create(const gc_config_t* config);
void gc_destroy(garbage_collector_t* gc);

int gc_retain(garbage_collector_t* gc, void* ptr);
int gc_release(garbage_collector_t* gc, void* ptr);

int gc_add_root(garbage_collector_t* gc, void** root_loc, const char* name);
int gc_remove_root(garbage_collector_t* gc, void** root_loc);
size_t gc_get_root_count(garbage_collector_t* gc);

gc_weak_ref_t* gc_add_weak_ref(garbage_collector_t* gc, void* target, void** slot);
void gc_remove_weak_ref(garbage_collector_t* gc, gc_weak_ref_t* wref);
void* gc_resolve_weak_ref(gc_weak_ref_t* wref);

int gc_collect(garbage_collector_t* gc, gc_trigger_t trigger);
int gc_collect_young(garbage_collector_t* gc);
int gc_collect_old(garbage_collector_t* gc);

const gc_stats_t* gc_get_stats(garbage_collector_t* gc);
int gc_reset_stats(garbage_collector_t* gc);

uint64_t gc_get_live_bytes(garbage_collector_t* gc);
uint64_t gc_get_total_bytes(garbage_collector_t* gc);
double gc_get_pressure(garbage_collector_t* gc);

int gc_register_pool(garbage_collector_t* gc, void* pool,
                     uint64_t (*pool_used_fn)(void*),
                     uint64_t (*pool_total_fn)(void*));
int gc_unregister_pool(garbage_collector_t* gc, void* pool);

#ifdef __cplusplus
}
#endif

#endif
