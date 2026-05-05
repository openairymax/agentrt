/* Stub implementations for LTO-unresolvable symbols */
#include "compensation.h"
#include "execution.h"
#include "id_utils.h"
#include "memory_provider.h"
#include <stdlib.h>
#include <string.h>

/* id_utils stubs */
agentos_error_t agentos_generate_uuid(char *buf)
{
    if (!buf)
        return -1;
    memset(buf, '0', 36);
    buf[8]  = '-';
    buf[13] = '-';
    buf[18] = '-';
    buf[23] = '-';
    buf[36] = '\0';
    return 0;
}

void agentos_generate_plan_id(char *buf, size_t len)
{
    if (!buf || len < 2)
        return;
    memset(buf, 'P', len - 1);
    buf[0]       = 'p';
    buf[len - 1] = '\0';
}

/* execution engine stubs */
agentos_error_t agentos_execution_register_unit(agentos_execution_engine_t *engine, const char *name,
                                                agentos_execution_unit_t unit)
{
    (void) engine;
    (void) name;
    (void) unit;
    return AGENTOS_SUCCESS;
}

void agentos_execution_unregister_unit(agentos_execution_engine_t *engine, const char *name)
{
    (void) engine;
    (void) name;
}

void agentos_execution_set_feedback_callback(agentos_execution_engine_t *engine, agentos_feedback_callback_t callback,
                                             void *user_data)
{
    (void) engine;
    (void) callback;
    (void) user_data;
}

/* compensation stubs */
agentos_error_t agentos_compensation_compensate(agentos_compensation_t *mgr, const char *action_id)
{
    (void) mgr;
    (void) action_id;
    return AGENTOS_SUCCESS;
}

/* memory provider stubs - using provider interface instead of direct memoryrovol calls */
static int g_memory_provider_initialized = 0;

/* syscall stubs */
void *agentos_sys_memory_search(const char *query, size_t limit)
{
    (void) query;
    (void) limit;
    return NULL;
}

void agentos_sys_free(void *ptr)
{
    (void) ptr;
    free(ptr);
}

agentos_memory_provider_t* agentos_memory_provider_get_active(void)
{
    return NULL;
}

agentos_error_t agentos_builtin_memory_provider_init(const char* config_path)
{
    (void) config_path;
    g_memory_provider_initialized = 1;
    return AGENTOS_SUCCESS;
}

void agentos_memory_provider_free_query_results(char** results, float* scores, size_t count)
{
    (void) count;
    if (results) { for (size_t i = 0; i < count; i++) free(results[i]); free(results); }
    if (scores) free(scores);
}
