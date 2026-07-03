/**
 * @file core_init.c
 * @brief 内核入口点（初始化与关闭）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 初始化顺序（严格依赖链）：
 *   1. mem      — 内存分配器（AGENTOS_MALLOC/FREE 基础）
 *   2. oom      — OOM 分级响应框架（依赖 mem，五级压力 NORMAL→FATAL）
 *   3. task     — 任务调度器（依赖 mem + oom 内存检查）
 *   4. ipc      — 进程间通信（依赖 task）
 *   5. eventloop — 时间事件循环（依赖 task）
 *   6. heapstore — 持久化存储集成（依赖上述所有子系统就绪）
 *
 * 关闭顺序（逆序）：
 *   heapstore → eventloop → timer → ipc → task → oom → mem
 */

#include "agentos.h"
#include "atomic_compat.h"
#include "oom_handler.h"
#include "heapstore_integration.h"

static atomic_long g_core_initialized = 0;

int agentos_core_init(void)
{
    long expected = 0;
    if (!atomic_compare_exchange_strong_explicit(&g_core_initialized, &expected, 2,
                                                 memory_order_seq_cst, memory_order_seq_cst)) {
        if (expected == 2) {
            while (atomic_load_explicit(&g_core_initialized, memory_order_seq_cst) == 2) {}
        }
        return AGENTOS_SUCCESS;
    }

    int ret = 0;

    /* ---- 1. 内存分配器 ---- */
    ret = agentos_mem_init(0);
    if (ret != 0) {
        AGENTOS_LOG_ERROR("core_init: agentos_mem_init failed, ret=%d", ret);
        goto fail;
    }
    AGENTOS_LOG_INFO("core_init: [OK] memory allocator initialized");

    /* ---- 2. OOM 分级响应框架（SEC-12 合规） ---- */
    ret = agentos_oom_init(0);  /* 0 = 自动检测系统总内存 */
    if (ret != AGENTOS_SUCCESS) {
        AGENTOS_LOG_ERROR("core_init: agentos_oom_init failed, ret=%d", ret);
        goto cleanup_mem;
    }

    agentos_oom_config_t oom_cfg;
    agentos_oom_config_defaults(&oom_cfg);
    oom_cfg.enable_allocation_check = true;  /* 启用分配时内存压力检查 */
    (void)agentos_oom_config_apply(&oom_cfg);
    AGENTOS_LOG_INFO("core_init: [OK] OOM handler initialized (5-level pressure framework)");

    /* ---- 3. 任务调度器 ---- */
    ret = agentos_task_init();
    if (ret != 0) {
        AGENTOS_LOG_ERROR("core_init: agentos_task_init failed, ret=%d", ret);
        goto cleanup_oom;
    }
    AGENTOS_LOG_INFO("core_init: [OK] task scheduler initialized");

    /* ---- 4. 进程间通信 ---- */
    ret = agentos_ipc_init();
    if (ret != 0) {
        AGENTOS_LOG_ERROR("core_init: agentos_ipc_init failed, ret=%d", ret);
        goto cleanup_task;
    }
    AGENTOS_LOG_INFO("core_init: [OK] IPC subsystem initialized");

    /* ---- 5. 时间事件循环 ---- */
    ret = agentos_time_eventloop_init();
    if (ret != 0) {
        AGENTOS_LOG_ERROR("core_init: agentos_time_eventloop_init failed, ret=%d", ret);
        goto cleanup_ipc;
    }
    AGENTOS_LOG_INFO("core_init: [OK] time eventloop initialized");

    /* ---- 6. heapstore 持久化存储集成 ---- */
    /* heapstore_integration.h 注释要求"在 agentos_core_init() 之后调用"，
     * 但在所有核心子系统就绪后、g_core_initialized 置 1 前调用是安全的：
     * 此时 mem/task/ipc/eventloop 均已初始化，heapstore 可正常使用。
     * heapstore 是初始化链最后一环，失败时降级为无持久化模式（非致命），
     * 因此无需 cleanup 标签 — 成功则 return，失败则继续降级运行。 */
    ret = heapstore_integration_init(NULL);  /* NULL = 使用 AGENTOS_HEAPSTORE_ROOT 环境变量或默认路径 */
    if (ret != AGENTOS_SUCCESS) {
        AGENTOS_LOG_WARN("core_init: heapstore_integration_init failed, ret=%d (persistence degraded)", ret);
        /* heapstore 失败不致命 — 系统可降级为无持久化模式继续运行 */
    } else {
        AGENTOS_LOG_INFO("core_init: [OK] heapstore integration initialized (persistence enabled)");
    }

    atomic_store_explicit(&g_core_initialized, 1, memory_order_seq_cst);
    AGENTOS_LOG_INFO("core_init: [OK] AgentOS core initialized successfully");
    return AGENTOS_SUCCESS;

cleanup_ipc:
    /* eventloop_init 失败时跳转此处 — eventloop 未初始化，无需清理 */
    agentos_ipc_cleanup();
cleanup_task:
    agentos_task_cleanup();
cleanup_oom:
    agentos_oom_destroy();
cleanup_mem:
    agentos_mem_cleanup();
fail:
    atomic_store_explicit(&g_core_initialized, 0, memory_order_seq_cst);
    return ret;
}

void agentos_core_shutdown(void)
{
    long expected = 1;
    if (!atomic_compare_exchange_strong_explicit(&g_core_initialized, &expected, 0,
                                                 memory_order_seq_cst, memory_order_seq_cst)) {
        return;
    }

    AGENTOS_LOG_INFO("core_shutdown: shutting down AgentOS core...");

    /* 逆序关闭：heapstore → eventloop → timer → ipc → task → oom → mem */
    heapstore_integration_shutdown();
    AGENTOS_LOG_INFO("core_shutdown: [OK] heapstore shutdown");

    agentos_time_eventloop_cleanup();
    agentos_time_timer_cleanup();
    AGENTOS_LOG_INFO("core_shutdown: [OK] time subsystem shutdown");

    agentos_ipc_cleanup();
    AGENTOS_LOG_INFO("core_shutdown: [OK] IPC subsystem shutdown");

    agentos_task_cleanup();
    AGENTOS_LOG_INFO("core_shutdown: [OK] task scheduler shutdown");

    agentos_oom_destroy();
    AGENTOS_LOG_INFO("core_shutdown: [OK] OOM handler destroyed");

    agentos_mem_cleanup();
    AGENTOS_LOG_INFO("core_shutdown: [OK] memory allocator cleanup");

    AGENTOS_LOG_INFO("core_shutdown: AgentOS core shutdown complete");
}
