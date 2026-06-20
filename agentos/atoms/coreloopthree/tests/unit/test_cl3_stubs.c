/* Stub implementations for LTO-unresolvable symbols */
#include "compensation.h"
#include "daemon_bootstrap_ipc.h"
#include "daemon_bootstrap_sd.h"
#include "execution.h"
#include "ipc_bus_helper.h"
#include "ipc_service_bus.h"
#include "llm_service.h"
#include "memory_compat.h"
#include "memoryrovol_bridge.h"
#include "service_discovery_helper.h"
#include "tool_approval.h"
#include "tool_service.h"

#include <stdlib.h>
#include <string.h>

/* execution engine stubs */
agentos_error_t agentos_execution_register_unit(agentos_execution_engine_t *engine,
                                                const char *name, agentos_execution_unit_t unit)
{
    (void)engine;
    (void)name;
    (void)unit;
    return AGENTOS_SUCCESS;
}

void agentos_execution_unregister_unit(agentos_execution_engine_t *engine, const char *name)
{
    (void)engine;
    (void)name;
}

void agentos_execution_set_feedback_callback(agentos_execution_engine_t *engine,
                                             agentos_feedback_callback_t callback, void *user_data)
{
    (void)engine;
    (void)callback;
    (void)user_data;
}

/* compensation stubs */
agentos_error_t agentos_compensation_compensate(agentos_compensation_t *mgr, const char *action_id)
{
    (void)mgr;
    (void)action_id;
    return AGENTOS_SUCCESS;
}

/* syscall stubs */
/* agentos_sys_memory_search removed: defined in agentos_syscall */

void agentos_sys_free(void *ptr)
{
    (void)ptr;
    free(ptr);
}

/* ==================== LLM service stubs ==================== */

int llm_service_complete(llm_service_t *svc, const llm_request_config_t *manager,
                         llm_response_t **out_response)
{
    (void)svc;
    (void)manager;
    if (out_response) *out_response = NULL;
    return -1;
}

int llm_service_complete_stream(llm_service_t *svc, const llm_request_config_t *manager,
                                llm_stream_callback_t callback, void *callback_data,
                                llm_response_t **out_response)
{
    (void)svc;
    (void)manager;
    (void)callback;
    (void)callback_data;
    if (out_response) *out_response = NULL;
    return -1;
}

void llm_response_free(llm_response_t *resp)
{
    (void)resp;
}

/* ==================== Tool service stubs ==================== */

int tool_service_execute(tool_service_t *svc, const tool_execute_request_t *req,
                         tool_result_t **out_result)
{
    (void)svc;
    (void)req;
    if (out_result) *out_result = NULL;
    return -1;
}

void tool_result_free(tool_result_t *res)
{
    (void)res;
}

/* ==================== Daemon bootstrap stubs ==================== */

daemon_bootstrap_sd_t *daemon_bootstrap_sd_start(const char *name, const char *type,
                                                  const char *host, uint16_t port,
                                                  const char *tags, uint32_t ttl_ms)
{
    (void)name;
    (void)type;
    (void)host;
    (void)port;
    (void)tags;
    (void)ttl_ms;
    return NULL;
}

void daemon_bootstrap_sd_stop(daemon_bootstrap_sd_t *bsd)
{
    (void)bsd;
}

sd_helper_t *daemon_bootstrap_sd_get_helper(daemon_bootstrap_sd_t *bsd)
{
    (void)bsd;
    return NULL;
}

int sd_helper_select_with_strategy(sd_helper_t *sdh, const char *service_name,
                                   sd_lb_strategy_t strategy,
                                   sd_instance_t *instance)
{
    (void)sdh;
    (void)service_name;
    (void)strategy;
    (void)instance;
    return -1;
}

daemon_bootstrap_ipc_t *daemon_bootstrap_ipc_start(const char *daemon_name,
                                                    const char *channel_name,
                                                    const char *host, uint16_t port,
                                                    ipc_bus_proto_t protocol)
{
    (void)daemon_name;
    (void)channel_name;
    (void)host;
    (void)port;
    (void)protocol;
    return NULL;
}

void daemon_bootstrap_ipc_stop(daemon_bootstrap_ipc_t *bipc)
{
    (void)bipc;
}

ipc_bus_helper_t *daemon_bootstrap_ipc_get_helper(daemon_bootstrap_ipc_t *bipc)
{
    (void)bipc;
    return NULL;
}

int ipc_bus_helper_request(ipc_bus_helper_t *ibh, const char *target_service,
                            const ipc_bus_message_t *request,
                            ipc_bus_message_t *response, uint32_t timeout_ms)
{
    (void)ibh;
    (void)target_service;
    (void)request;
    (void)response;
    (void)timeout_ms;
    return -1;
}

/* ==================== Tool approval stubs ==================== */

tool_approval_ctx_t *tool_approval_create(const tool_approval_config_t *cfg)
{
    (void)cfg;
    return NULL;
}

void tool_approval_destroy(tool_approval_ctx_t *ctx)
{
    (void)ctx;
}

int tool_approval_check(tool_approval_ctx_t *ctx, const tool_metadata_t *meta,
                         const char *params_json, tool_approval_detail_t *detail)
{
    (void)ctx;
    (void)meta;
    (void)params_json;
    (void)detail;
    return -1;
}

/* ==================== Bootstrap misc stubs ==================== */

bool daemon_bootstrap_ipc_is_running(daemon_bootstrap_ipc_t *bipc)
{
    (void)bipc;
    return false;
}

/* ==================== C-L12: MemoryRovol bridge stubs ==================== */

memoryrovol_bridge_t *memoryrovol_bridge_create(const memoryrovol_bridge_config_t *config)
{
    (void)config;
    return NULL;
}

void memoryrovol_bridge_destroy(memoryrovol_bridge_t *bridge)
{
    (void)bridge;
}

agentos_memory_provider_t *memoryrovol_bridge_get_provider(memoryrovol_bridge_t *bridge)
{
    (void)bridge;
    return NULL;
}

void memoryrovol_bridge_dump_stats(memoryrovol_bridge_t *bridge)
{
    (void)bridge;
}
