/**
 * @file memory_prealloc.c
 * @brief Critical path pre-allocation for signal handling, OOM, audit, and shutdown
 *
 * Pre-allocates emergency buffers at startup so that critical operations
 * (signal handlers, OOM reporting, audit logging, emergency shutdown)
 * never fail due to memory pressure.
 *
 * @copyright Copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "memory_prealloc.h"
#include "memory_compat.h"
#include "logging_compat.h"
#include "platform.h"

#include <string.h>

/* ============================================================================
 * Global pre-allocation pool (static singleton)
 * ============================================================================ */

static agentrt_prealloc_pool_t g_prealloc_pool = {0};
static agentrt_mutex_t g_prealloc_lock;

/* ============================================================================
 * Internal helpers
 * ============================================================================ */

/**
 * @brief Get buffer pointer by category
 */
static void *prealloc_get_buf(int category)
{
    switch (category) {
    case AGENTRT_PREALLOC_SIGNAL:   return g_prealloc_pool.signal_buf;
    case AGENTRT_PREALLOC_OOM:      return g_prealloc_pool.oom_buf;
    case AGENTRT_PREALLOC_AUDIT:    return g_prealloc_pool.audit_buf;
    case AGENTRT_PREALLOC_SHUTDOWN: return g_prealloc_pool.shutdown_buf;
    default:                        return NULL;
    }
}

/**
 * @brief Get buffer size by category
 */
static size_t prealloc_get_buf_size(int category)
{
    switch (category) {
    case AGENTRT_PREALLOC_SIGNAL:   return g_prealloc_pool.signal_buf_size;
    case AGENTRT_PREALLOC_OOM:      return g_prealloc_pool.oom_buf_size;
    case AGENTRT_PREALLOC_AUDIT:    return g_prealloc_pool.audit_buf_size;
    case AGENTRT_PREALLOC_SHUTDOWN: return g_prealloc_pool.shutdown_buf_size;
    default:                        return 0;
    }
}

/**
 * @brief Get in_use flag pointer by category
 */
static int *prealloc_get_in_use(int category)
{
    switch (category) {
    case AGENTRT_PREALLOC_SIGNAL:   return &g_prealloc_pool.signal_buf_in_use;
    case AGENTRT_PREALLOC_OOM:      return &g_prealloc_pool.oom_buf_in_use;
    case AGENTRT_PREALLOC_AUDIT:    return &g_prealloc_pool.audit_buf_in_use;
    case AGENTRT_PREALLOC_SHUTDOWN: return &g_prealloc_pool.shutdown_buf_in_use;
    default:                        return NULL;
    }
}

/**
 * @brief Get category name string for logging
 */
static const char *prealloc_category_name(int category)
{
    switch (category) {
    case AGENTRT_PREALLOC_SIGNAL:   return "SIGNAL";
    case AGENTRT_PREALLOC_OOM:      return "OOM";
    case AGENTRT_PREALLOC_AUDIT:    return "AUDIT";
    case AGENTRT_PREALLOC_SHUTDOWN: return "SHUTDOWN";
    default:                        return "UNKNOWN";
    }
}

/* ============================================================================
 * Public API
 * ============================================================================ */

int agentrt_prealloc_init(void)
{
    if (g_prealloc_pool.initialized) {
        return 0; /* already initialized */
    }

    agentrt_mutex_init(&g_prealloc_lock);

    agentrt_mutex_lock(&g_prealloc_lock);

    /* Allocate signal handler buffer */
    g_prealloc_pool.signal_buf = AGENTRT_MALLOC(AGENTRT_PREALLOC_SIGNAL_BUF_SIZE);
    if (!g_prealloc_pool.signal_buf) {
        goto fail;
    }
    g_prealloc_pool.signal_buf_size = AGENTRT_PREALLOC_SIGNAL_BUF_SIZE;
    g_prealloc_pool.signal_buf_in_use = 0;

    /* Allocate OOM error reporting buffer */
    g_prealloc_pool.oom_buf = AGENTRT_MALLOC(AGENTRT_PREALLOC_OOM_BUF_SIZE);
    if (!g_prealloc_pool.oom_buf) {
        AGENTRT_FREE(g_prealloc_pool.signal_buf);
        g_prealloc_pool.signal_buf = NULL;
        goto fail;
    }
    g_prealloc_pool.oom_buf_size = AGENTRT_PREALLOC_OOM_BUF_SIZE;
    g_prealloc_pool.oom_buf_in_use = 0;

    /* Allocate audit log emergency buffer */
    g_prealloc_pool.audit_buf = AGENTRT_MALLOC(AGENTRT_PREALLOC_AUDIT_BUF_SIZE);
    if (!g_prealloc_pool.audit_buf) {
        AGENTRT_FREE(g_prealloc_pool.signal_buf);
        g_prealloc_pool.signal_buf = NULL;
        AGENTRT_FREE(g_prealloc_pool.oom_buf);
        g_prealloc_pool.oom_buf = NULL;
        goto fail;
    }
    g_prealloc_pool.audit_buf_size = AGENTRT_PREALLOC_AUDIT_BUF_SIZE;
    g_prealloc_pool.audit_buf_in_use = 0;

    /* Allocate emergency shutdown buffer */
    g_prealloc_pool.shutdown_buf = AGENTRT_MALLOC(AGENTRT_PREALLOC_SHUTDOWN_BUF_SIZE);
    if (!g_prealloc_pool.shutdown_buf) {
        AGENTRT_FREE(g_prealloc_pool.signal_buf);
        g_prealloc_pool.signal_buf = NULL;
        AGENTRT_FREE(g_prealloc_pool.oom_buf);
        g_prealloc_pool.oom_buf = NULL;
        AGENTRT_FREE(g_prealloc_pool.audit_buf);
        g_prealloc_pool.audit_buf = NULL;
        goto fail;
    }
    g_prealloc_pool.shutdown_buf_size = AGENTRT_PREALLOC_SHUTDOWN_BUF_SIZE;
    g_prealloc_pool.shutdown_buf_in_use = 0;

    g_prealloc_pool.initialized = 1;

    agentrt_mutex_unlock(&g_prealloc_lock);

    AGENTRT_LOG_INFO("[PREALLOC] Emergency buffer pool initialized: "
            "signal=%d, oom=%d, audit=%d, shutdown=%d bytes",
            AGENTRT_PREALLOC_SIGNAL_BUF_SIZE,
            AGENTRT_PREALLOC_OOM_BUF_SIZE,
            AGENTRT_PREALLOC_AUDIT_BUF_SIZE,
            AGENTRT_PREALLOC_SHUTDOWN_BUF_SIZE);

    return 0;

fail:
    agentrt_mutex_unlock(&g_prealloc_lock);
    agentrt_mutex_destroy(&g_prealloc_lock);
    return -1;
}

void agentrt_prealloc_shutdown(void)
{
    if (!g_prealloc_pool.initialized) {
        return;
    }

    agentrt_mutex_lock(&g_prealloc_lock);

    AGENTRT_FREE(g_prealloc_pool.signal_buf);
    g_prealloc_pool.signal_buf = NULL;
    g_prealloc_pool.signal_buf_size = 0;
    g_prealloc_pool.signal_buf_in_use = 0;

    AGENTRT_FREE(g_prealloc_pool.oom_buf);
    g_prealloc_pool.oom_buf = NULL;
    g_prealloc_pool.oom_buf_size = 0;
    g_prealloc_pool.oom_buf_in_use = 0;

    AGENTRT_FREE(g_prealloc_pool.audit_buf);
    g_prealloc_pool.audit_buf = NULL;
    g_prealloc_pool.audit_buf_size = 0;
    g_prealloc_pool.audit_buf_in_use = 0;

    AGENTRT_FREE(g_prealloc_pool.shutdown_buf);
    g_prealloc_pool.shutdown_buf = NULL;
    g_prealloc_pool.shutdown_buf_size = 0;
    g_prealloc_pool.shutdown_buf_in_use = 0;

    g_prealloc_pool.initialized = 0;

    agentrt_mutex_unlock(&g_prealloc_lock);
    agentrt_mutex_destroy(&g_prealloc_lock);

    AGENTRT_LOG_INFO("[PREALLOC] Emergency buffer pool shut down");
}

void *agentrt_prealloc_acquire(int category)
{
    if (!g_prealloc_pool.initialized) {
        return NULL;
    }

    if (category < 0 || category > AGENTRT_PREALLOC_SHUTDOWN) {
        return NULL;
    }

    agentrt_mutex_lock(&g_prealloc_lock);

    int *in_use = prealloc_get_in_use(category);
    if (!in_use || *in_use) {
        AGENTRT_LOG_WARN("[PREALLOC] Buffer already in use for category %s, "
                "cannot acquire",
                prealloc_category_name(category));
        agentrt_mutex_unlock(&g_prealloc_lock);
        return NULL;
    }

    *in_use = 1;
    void *buf = prealloc_get_buf(category);

    agentrt_mutex_unlock(&g_prealloc_lock);
    return buf;
}

void agentrt_prealloc_release(int category)
{
    if (!g_prealloc_pool.initialized) {
        return;
    }

    if (category < 0 || category > AGENTRT_PREALLOC_SHUTDOWN) {
        return;
    }

    agentrt_mutex_lock(&g_prealloc_lock);

    int *in_use = prealloc_get_in_use(category);
    void *buf = prealloc_get_buf(category);
    size_t buf_size = prealloc_get_buf_size(category);

    if (in_use && buf && buf_size > 0) {
        /* Secure zero the buffer before releasing for security */
        __builtin_memset(buf, 0, buf_size);
    }

    if (in_use) {
        *in_use = 0;
    }

    agentrt_mutex_unlock(&g_prealloc_lock);
}

int agentrt_prealloc_is_initialized(void)
{
    return g_prealloc_pool.initialized;
}
