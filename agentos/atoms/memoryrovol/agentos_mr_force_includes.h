#ifndef AGENTOS_MR_FORCE_INCLUDES_H
#define AGENTOS_MR_FORCE_INCLUDES_H

#include "../../commons/utils/memory/include/memory_compat.h"
#include "../../commons/platform/include/platform.h"

#ifdef AGENTOS_USE_SCHEDULER_THREAD_IMPL
int agentos_thread_create(agentos_thread_t *thread, agentos_thread_func_t func, void *arg);
int agentos_thread_join(agentos_thread_t thread, void **retval);
#endif

#endif
