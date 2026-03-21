/**
 * @file core.h
 * @brief Domes 核心内部定义
 */
#ifndef DOMES_CORE_H
#define DOMES_CORE_H

#include "domes.h"
#include "workbench/workbench.h"
#include "permission/permission.h"
#include "audit/audit.h"
#include "sanitizer/sanitizer.h"
#include <pthread.h>

struct domes_core {
    domes_config_t   config;
    workbench_manager_t*    wb_mgr;
    permission_engine_t*    perm_eng;
    audit_logger_t*         audit;
    sanitizer_t*            sanitizer;
    pthread_mutex_t         lock;
    volatile int            initialized;
};

#endif /* DOMES_CORE_H */