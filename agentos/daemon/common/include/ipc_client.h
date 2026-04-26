#ifndef AGENTOS_IPC_CLIENT_H
#define AGENTOS_IPC_CLIENT_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

int svc_ipc_init(const char* baseruntime_url);
void svc_ipc_cleanup(void);
int svc_rpc_call(const char* method, const char* params,
                 char** out_result, uint32_t timeout_ms);
int svc_ipc_set_timeout(uint32_t timeout_ms);
int svc_ipc_get_pool_status(int* total, int* available);

#ifdef __cplusplus
}
#endif

#endif
