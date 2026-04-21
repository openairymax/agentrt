#include "garbage_collector.h"
#include "logger.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define GC_HASH_BUCKETS 256
#define GC_MAX_ROOTS 1024
#define GC_DEFAULT_PROMOTE_THRESHOLD 6

static uint64_t gc_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static size_t gc_hash_ptr(void* ptr) {
    uintptr_t val = (uintptr_t)ptr;
    val = (val >> 4) ^ (val >> 16);
    return (size_t)(val % GC_HASH_BUCKETS);
}

typedef struct gc_pool_tracker {
    void* pool;
    uint64_t (*used_fn)(void*);
    uint64_t (*total_fn)(void*);
    struct gc_pool_tracker* next;
} gc_pool_tracker_t;

struct garbage_collector {
    gc_config_t config;
    gc_stats_t stats;

    gc_object_header_t* live_list[GC_GEN_COUNT];
    size_t live_count[GC_GEN_COUNT];
    uint64_t live_bytes[GC_GEN_COUNT];

    gc_object_header_t* hash_table[GC_HASH_BUCKETS];

    gc_root_t* roots;
    size_t root_count;

    gc_weak_ref_t* weak_refs;

    gc_pool_tracker_t* pools;

    agentos_mutex_t* lock;
    uint64_t alloc_count_since_gc;
    uint64_t last_time_gc_ns;
    int collecting;
};

static const gc_config_t gc_default_config = {
    .young_pressure_threshold = 0.75,
    .old_pressure_threshold   = 0.85,
    .young_promote_threshold  = GC_DEFAULT_PROMOTE_THRESHOLD,
    .time_interval_sec        = 30,
    .max_collect_time_ms      = 50,
    .enable_concurrent_mark  = false,
    .enable_generational     = true,
    .enable_cycle_detection  = true
};

garbage_collector_t* gc_create(const gc_config_t* config) {
    garbage_collector_t* gc = (garbage_collector_t*)AGENTOS_CALLOC(1, sizeof(garbage_collector_t));
    if (!gc) return NULL;

    gc->config = config ? *config : gc_default_config;
    memset(&gc->stats, 0, sizeof(gc_stats_t));
    gc->lock = agentos_mutex_create();
    if (!gc->lock) { AGENTOS_FREE(gc); return NULL; }

    for (int i = 0; i < GC_GEN_COUNT; i++) {
        gc->live_list[i] = NULL;
        gc->live_count[i] = 0;
        gc->live_bytes[i] = 0;
    }
    for (int i = 0; i < GC_HASH_BUCKETS; i++) {
        gc->hash_table[i] = NULL;
    }
    gc->roots = NULL;
    gc->root_count = 0;
    gc->weak_refs = NULL;
    gc->pools = NULL;
    gc->alloc_count_since_gc = 0;
    gc->last_time_gc_ns = gc_time_ns();
    gc->collecting = 0;
    return gc;
}

void gc_destroy(garbage_collector_t* gc) {
    if (!gc) return;

    agentos_mutex_lock(gc->lock);

    for (int gen = 0; gen < GC_GEN_COUNT; gen++) {
        gc_object_header_t* obj = gc->live_list[gen];
        while (obj) {
            gc_object_header_t* next = obj->next;
            if (obj->finalizer && obj->user_data) {
                obj->finalizer(obj->user_data);
            }
            AGENTOS_FREE(obj);
            obj = next;
        }
    }

    gc_root_t* root = gc->roots;
    while (root) {
        gc_root_t* next = root->next;
        AGENTOS_FREE(root);
        root = next;
    }

    gc_weak_ref_t* wr = gc->weak_refs;
    while (wr) {
        gc_weak_ref_t* next = wr->next;
        AGENTOS_FREE(wr);
        wr = next;
    }

    gc_pool_tracker_t* pt = gc->pools;
    while (pt) {
        gc_pool_tracker_t* next = pt->next;
        AGENTOS_FREE(pt);
        pt = next;
    }

    agentos_mutex_unlock(gc->lock);
    agentos_mutex_destroy(gc->lock);
    AGENTOS_FREE(gc);
}

static gc_object_header_t* gc_lookup(garbage_collector_t* gc, void* ptr) {
    if (!ptr) return NULL;
    size_t bucket = gc_hash_ptr(ptr);
    gc_object_header_t* hdr = gc->hash_table[bucket];
    while (hdr) {
        if ((void*)((uint8_t*)hdr + sizeof(gc_object_header_t)) == ptr) {
            return hdr;
        }
        hdr = hdr->hash_next;
    }
    return NULL;
}

int gc_retain(garbage_collector_t* gc, void* ptr) {
    if (!gc || !ptr) return -1;
    agentos_mutex_lock(gc->lock);

    gc_object_header_t* hdr = gc_lookup(gc, ptr);
    if (!hdr || hdr->magic != GC_MAGIC_ALIVE) {
        agentos_mutex_unlock(gc->lock);
        return -1;
    }
    hdr->ref_count++;
    hdr->last_access_ns = gc_time_ns();

    agentos_mutex_unlock(gc->lock);
    return (int)hdr->ref_count;
}

int gc_release(garbage_collector_t* gc, void* ptr) {
    if (!gc || !ptr) return -1;
    agentos_mutex_lock(gc->lock);

    gc_object_header_t* hdr = gc_lookup(gc, ptr);
    if (!hdr || hdr->magic != GC_MAGIC_ALIVE) {
        agentos_mutex_unlock(gc->lock);
        return -1;
    }

    if (hdr->ref_count > 0) hdr->ref_count--;

    agentos_mutex_unlock(gc->lock);
    return (int)hdr->ref_count;
}

int gc_add_root(garbage_collector_t* gc, void** root_loc, const char* name) {
    if (!gc || !root_loc) return -1;
    agentos_mutex_lock(gc->lock);

    if (gc->root_count >= GC_MAX_ROOTS) {
        agentos_mutex_unlock(gc->lock);
        return -2;
    }

    gc_root_t* existing = gc->roots;
    while (existing) {
        if (existing->location == root_loc) {
            agentos_mutex_unlock(gc->lock);
            return 0;
        }
        existing = existing->next;
    }

    gc_root_t* root = (gc_root_t*)AGENTOS_CALLOC(1, sizeof(gc_root_t));
    if (!root) { agentos_mutex_unlock(gc->lock); return -3; }

    root->location = root_loc;
    root->name = name;
    root->next = gc->roots;
    gc->roots = root;
    gc->root_count++;

    agentos_mutex_unlock(gc->lock);
    return 0;
}

int gc_remove_root(garbage_collector_t* gc, void** root_loc) {
    if (!gc || !root_loc) return -1;
    agentos_mutex_lock(gc->lock);

    gc_root_t** pp = &gc->roots;
    while (*pp) {
        if ((*pp)->location == root_loc) {
            gc_root_t* doomed = *pp;
            *pp = doomed->next;
            AGENTOS_FREE(doomed);
            gc->root_count--;
            agentos_mutex_unlock(gc->lock);
            return 0;
        }
        pp = &(*pp)->next;
    }

    agentos_mutex_unlock(gc->lock);
    return -1;
}

size_t gc_get_root_count(garbage_collector_t* gc) {
    if (!gc) return 0;
    return gc->root_count;
}

gc_weak_ref_t* gc_add_weak_ref(garbage_collector_t* gc, void* target, void** slot) {
    if (!gc || !target || !slot) return NULL;
    agentos_mutex_lock(gc->lock);

    gc_object_header_t* hdr = gc_lookup(gc, target);
    if (hdr && hdr->magic == GC_MAGIC_ALIVE) {
        hdr->weak_ref_count++;
    }

    gc_weak_ref_t* wref = (gc_weak_ref_t*)AGENTOS_CALLOC(1, sizeof(gc_weak_ref_t));
    if (!wref) { agentos_mutex_unlock(gc->lock); return NULL; }

    wref->target = target;
    wref->slot = slot;
    wref->next = gc->weak_refs;
    gc->weak_refs = wref;

    agentos_mutex_unlock(gc->lock);
    return wref;
}

void gc_remove_weak_ref(garbage_collector_t* gc, gc_weak_ref_t* wref) {
    if (!gc || !wref) return;
    agentos_mutex_lock(gc->lock);

    gc_object_header_t* hdr = gc_lookup(gc, wref->target);
    if (hdr && hdr->magic == GC_MAGIC_ALIVE && hdr->weak_ref_count > 0) {
        hdr->weak_ref_count--;
    }

    gc_weak_ref_t** pp = &gc->weak_refs;
    while (*pp) {
        if (*pp == wref) {
            *pp = wref->next;
            break;
        }
        pp = &(*pp)->next;
    }
    AGENTOS_FREE(wref);

    agentos_mutex_unlock(gc->lock);
}

void* gc_resolve_weak_ref(gc_weak_ref_t* wref) {
    if (!wref || !wref->slot) return NULL;
    return *wref->slot;
}

static void gc_mark_gray(garbage_collector_t* gc, gc_object_header_t* hdr) {
    if (!hdr || hdr->color == GC_COLOR_GRAY || hdr->color == GC_COLOR_BLACK) return;
    hdr->color = GC_COLOR_GRAY;
}

static void gc_scan_blacken(garbage_collector_t* gc) {
    bool changed = true;
    while (changed) {
        changed = false;
        for (int gen = 0; gen < GC_GEN_COUNT; gen++) {
            gc_object_header_t* obj = gc->live_list[gen];
            while (obj) {
                if (obj->color == GC_COLOR_GRAY && obj->ref_count > 0) {
                    obj->color = GC_COLOR_BLACK;
                    changed = true;
                }
                obj = obj->next;
            }
        }
    }
}

static uint64_t gc_sweep_generation(garbage_collector_t* gc, int gen) {
    uint64_t reclaimed_bytes = 0;
    uint64_t reclaimed_objects = 0;

    gc_object_header_t** pp = &gc->live_list[gen];
    while (*pp) {
        gc_object_header_t* obj = *pp;
        if (obj->color == GC_COLOR_WHITE && obj->ref_count == 0) {
            *pp = obj->next;

            if (obj->finalizer && obj->user_data) {
                obj->finalizer(obj->user_data);
            }

            gc_weak_ref_t* wr = gc->weak_refs;
            while (wr) {
                if (wr->target == (void*)((uint8_t*)obj + sizeof(gc_object_header_t))) {
                    *(wr->slot) = NULL;
                    gc->stats.weak_ref_cleared++;
                }
                wr = wr->next;
            }

            reclaimed_bytes += obj->size;
            reclaimed_objects++;

            size_t bucket = gc_hash_ptr((void*)((uint8_t*)obj + sizeof(gc_object_header_t)));
            gc_object_header_t** hp = &gc->hash_table[bucket];
            while (*hp) {
                if (*hp == obj) { *hp = obj->hash_next; break; }
                hp = &(*hp)->hash_next;
            }

            obj->magic = GC_MAGIC_COLLECTED;
            AGENTOS_FREE(obj);

            gc->live_count[gen]--;
        } else {
            if (obj->color != GC_COLOR_BLACK) {
                obj->color = GC_COLOR_WHITE;
            }

            if (gc->config.enable_generational &&
                gen == GC_GEN_YOUNG &&
                obj->ref_count >= gc->config.young_promote_threshold) {

                obj->generation = GC_GEN_OLD;
                obj->next = gc->live_list[GC_GEN_OLD];
                if (gc->live_list[GC_GEN_OLD]) {
                    gc->live_list[GC_GEN_OLD]->prev = obj;
                }
                obj->prev = NULL;
                gc->live_list[GC_GEN_OLD] = obj;
                *pp = obj->next;
                if (*pp) (*pp)->prev = NULL;

                gc->live_count[GC_GEN_YOUNG]--;
                gc->live_count[GC_GEN_OLD]++;
                gc->live_bytes[GC_GEN_YOUNG] -= obj->size;
                gc->live_bytes[GC_GEN_OLD] += obj->size;
                gc->stats.objects_promoted++;
                continue;
            }
            pp = &obj->next;
        }
    }

    gc->stats.objects_reclaimed += reclaimed_objects;
    gc->stats.bytes_reclaimed += reclaimed_bytes;
    return reclaimed_objects;
}

static int gc_run_collect(garbage_collector_t* gc, int target_gen, gc_trigger_t trigger) {
    if (!gc || gc->collecting) return -1;
    gc->collecting = 1;

    uint64_t t_start = gc_time_ns();

    for (int gen = 0; gen < GC_GEN_COUNT; gen++) {
        if (target_gen >= 0 && gen != target_gen) continue;

        gc_object_header_t* obj = gc->live_list[gen];
        while (obj) {
            obj->color = GC_COLOR_WHITE;
            obj = obj->next;
        }
    }

    gc_root_t* root = gc->roots;
    while (root) {
        if (root->location && *(root->location)) {
            gc_object_header_t* hdr = gc_lookup(gc, *(root->location));
            if (hdr) gc_mark_gray(gc, hdr);
        }
        root = root->next;
    }

    gc_scan_blacken(gc);

    if (target_gen >= 0) {
        gc_sweep_generation(gc, target_gen);
        gc->stats.generation_gc_counts[target_gen]++;
        if (target_gen == GC_GEN_YOUNG)
            gc->stats.young_collections++;
        else
            gc->stats.old_collections++;
    } else {
        for (int gen = 0; gen < GC_GEN_COUNT; gen++) {
            gc_sweep_generation(gc, gen);
            gc->stats.generation_gc_counts[gen]++;
        }
        gc->stats.young_collections++;
        gc->stats.old_collections++;
    }

    uint64_t t_end = gc_time_ns();
    uint64_t duration = t_end - t_start;

    gc->stats.total_collections++;
    gc->stats.collect_time_ns += duration;
    gc->stats.last_collect_ns = t_end;
    if (gc->stats.total_collections > 0) {
        gc->stats.avg_pause_ns = (double)gc->stats.collect_time_ns / gc->stats.total_collections;
    }
    gc->alloc_count_since_gc = 0;
    gc->last_time_gc_ns = t_end;

    gc->collecting = 0;
    return 0;
}

int gc_collect(garbage_collector_t* gc, gc_trigger_t trigger) {
    if (!gc) return -1;
    agentos_mutex_lock(gc->lock);
    int ret = gc_run_collect(gc, -1, trigger);
    agentos_mutex_unlock(gc->lock);
    return ret;
}

int gc_collect_young(garbage_collector_t* gc) {
    if (!gc) return -1;
    agentos_mutex_lock(gc->lock);
    int ret = gc_run_collect(gc, GC_GEN_YOUNG, GC_TRIGGER_TIME);
    agentos_mutex_unlock(gc->lock);
    return ret;
}

int gc_collect_old(garbage_collector_t* gc) {
    if (!gc) return -1;
    agentos_mutex_lock(gc->lock);
    int ret = gc_run_collect(gc, GC_GEN_OLD, GC_TRIGGER_TIME);
    agentos_mutex_unlock(gc->lock);
    return ret;
}

const gc_stats_t* gc_get_stats(garbage_collector_t* gc) {
    if (!gc) return NULL;
    return &gc->stats;
}

int gc_reset_stats(garbage_collector_t* gc) {
    if (!gc) return -1;
    agentos_mutex_lock(gc->lock);
    memset(&gc->stats, 0, sizeof(gc_stats_t));
    gc->last_time_gc_ns = gc_time_ns();
    agentos_mutex_unlock(gc->lock);
    return 0;
}

uint64_t gc_get_live_bytes(garbage_collector_t* gc) {
    if (!gc) return 0;
    uint64_t total = 0;
    agentos_mutex_lock(gc->lock);
    for (int i = 0; i < GC_GEN_COUNT; i++) total += gc->live_bytes[i];
    agentos_mutex_unlock(gc->lock);
    return total;
}

uint64_t gc_get_total_bytes(garbage_collector_t* gc) {
    if (!gc) return 0;
    uint64_t total = 0;
    agentos_mutex_lock(gc->lock);

    gc_pool_tracker_t* pt = gc->pools;
    while (pt) {
        if (pt->total_fn) total += pt->total_fn(pt->pool);
        pt = pt->next;
    }
    for (int i = 0; i < GC_GEN_COUNT; i++) total += gc->live_bytes[i];

    agentos_mutex_unlock(gc->lock);
    return total;
}

double gc_get_pressure(garbage_collector_t* gc) {
    if (!gc) return 0.0;
    uint64_t total = gc_get_total_bytes(gc);
    if (total == 0) return 0.0;
    uint64_t used = 0;

    gc_pool_tracker_t* pt = gc->pools;
    while (pt) {
        if (pt->used_fn) used += pt->used_fn(pt->pool);
        pt = pt->next;
    }
    for (int i = 0; i < GC_GEN_COUNT; i++) used += gc->live_bytes[i];

    return (double)used / (double)total;
}

int gc_register_pool(garbage_collector_t* gc, void* pool,
                     uint64_t (*pool_used_fn)(void*),
                     uint64_t (*pool_total_fn)(void*)) {
    if (!gc || !pool) return -1;
    agentos_mutex_lock(gc->lock);

    gc_pool_tracker_t* pt = (gc_pool_tracker_t*)AGENTOS_CALLOC(1, sizeof(gc_pool_tracker_t));
    if (!pt) { agentos_mutex_unlock(gc->lock); return -1; }

    pt->pool = pool;
    pt->used_fn = pool_used_fn;
    pt->total_fn = pool_total_fn;
    pt->next = gc->pools;
    gc->pools = pt;

    agentos_mutex_unlock(gc->lock);
    return 0;
}

int gc_unregister_pool(garbage_collector_t* gc, void* pool) {
    if (!gc || !pool) return -1;
    agentos_mutex_lock(gc->lock);

    gc_pool_tracker_t** pp = &gc->pools;
    while (*pp) {
        if ((*pp)->pool == pool) {
            gc_pool_tracker_t* doomed = *pp;
            *pp = doomed->next;
            AGENTOS_FREE(doomed);
            agentos_mutex_unlock(gc->lock);
            return 0;
        }
        pp = &(*pp)->next;
    }

    agentos_mutex_unlock(gc->lock);
    return -1;
}
