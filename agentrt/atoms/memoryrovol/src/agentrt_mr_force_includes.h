#ifndef AGENTRT_MR_FORCE_INCLUDES_H
#define AGENTRT_MR_FORCE_INCLUDES_H

#include "../../../commons/utils/memory/include/memory_compat.h"
#include "../../../commons/platform/include/platform.h"

#ifdef AGENTRT_USE_SCHEDULER_THREAD_IMPL
int agentrt_thread_create(agentrt_thread_t *thread, agentrt_thread_func_t func, void *arg);
int agentrt_thread_join(agentrt_thread_t thread, void **retval);
#endif

#endif
