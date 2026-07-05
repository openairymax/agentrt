/**
 * @file core_init.c
 * @brief 内核入口点（初始化与关闭）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 初始化顺序（严格依赖链）：
 *   1. mem      — 内存分配器（AGENTRT_MALLOC/FREE 基础）
 *   2. oom      — OOM 分级响应框架（依赖 mem，五级压力 NORMAL→FATAL）
 *   3. task     — 任务调度器（依赖 mem + oom 内存检查）
 *   4. ipc      — 进程间通信（依赖 task）
 *   5. eventloop — 时间事件循环（依赖 task）
 *   6. heapstore — 持久化存储集成（依赖上述所有子系统就绪）
 *
 * 关闭顺序（逆序）：
 *   heapstore → eventloop → timer → ipc → task → oom → mem
 */

#include "agentrt.h"
#include "atomic_compat.h"
#include "oom_handler.h"
#include "heapstore_integration.h"

static atomic_long g_core_initialized = 0;

int agentrt_core_init(void)
{
    long expected = 0;
    if (!atomic_compare_exchange_strong_explicit(&g_core_initialized, &expected, 2,
                                                 memory_order_seq_cst, memory_order_seq_cst)) {
        if (expected == 2) {
            while (atomic_load_explicit(&g_core_initialized, memory_order_seq_cst) == 2) {}
        }
        return AGENTRT_SUCCESS;
    }

    int ret = 0;

    /* ---- 1. 内存分配器 ---- */
    ret = agentrt_mem_init(0);
    if (ret != 0) {
        AGENTRT_LOG_ERROR("core_init: agentrt_mem_init failed, ret=%d", ret);
        goto fail;
    }
    AGENTRT_LOG_INFO("core_init: [OK] memory allocator initialized");

    /* ---- 2. OOM 分级响应框架（SEC-12 合规） ---- */
    ret = agentrt_oom_init(0);  /* 0 = 自动检测系统总内存 */
    if (ret != AGENTRT_SUCCESS) {
        AGENTRT_LOG_ERROR("core_init: agentrt_oom_init failed, ret=%d", ret);
        goto cleanup_mem;
    }

    agentrt_oom_config_t oom_cfg;
    agentrt_oom_config_defaults(&oom_cfg);
    oom_cfg.enable_allocation_check = true;  /* 启用分配时内存压力检查 */
    (void)agentrt_oom_config_apply(&oom_cfg);
    AGENTRT_LOG_INFO("core_init: [OK] OOM handler initialized (5-level pressure framework)");

    /* ---- 3. 任务调度器 ---- */
    ret = agentrt_task_init();
    if (ret != 0) {
        AGENTRT_LOG_ERROR("core_init: agentrt_task_init failed, ret=%d", ret);
        goto cleanup_oom;
    }
    AGENTRT_LOG_INFO("core_init: [OK] task scheduler initialized");

    /* ---- 4. 进程间通信 ---- */
    ret = agentrt_ipc_init();
    if (ret != 0) {
        AGENTRT_LOG_ERROR("core_init: agentrt_ipc_init failed, ret=%d", ret);
        goto cleanup_task;
    }
    AGENTRT_LOG_INFO("core_init: [OK] IPC subsystem initialized");

    /* ---- 5. 时间事件循环 ---- */
    ret = agentrt_time_eventloop_init();
    if (ret != 0) {
        AGENTRT_LOG_ERROR("core_init: agentrt_time_eventloop_init failed, ret=%d", ret);
        goto cleanup_ipc;
    }
    AGENTRT_LOG_INFO("core_init: [OK] time eventloop initialized");

    /* ---- 6. heapstore 持久化存储集成 ---- */
    /* heapstore_integration.h 注释要求"在 agentrt_core_init() 之后调用"，
     * 但在所有核心子系统就绪后、g_core_initialized 置 1 前调用是安全的：
     * 此时 mem/task/ipc/eventloop 均已初始化，heapstore 可正常使用。
     * heapstore 是初始化链最后一环，失败时降级为无持久化模式（非致命），
     * 因此无需 cleanup 标签 — 成功则 return，失败则继续降级运行。 */
    ret = heapstore_integration_init(NULL);  /* NULL = 使用 AGENTRT_HEAPSTORE_ROOT 环境变量或默认路径 */
    if (ret != AGENTRT_SUCCESS) {
        AGENTRT_LOG_WARN("core_init: heapstore_integration_init failed, ret=%d (persistence degraded)", ret);
        /* heapstore 失败不致命 — 系统可降级为无持久化模式继续运行 */
    } else {
        AGENTRT_LOG_INFO("core_init: [OK] heapstore integration initialized (persistence enabled)");
    }

    atomic_store_explicit(&g_core_initialized, 1, memory_order_seq_cst);
    AGENTRT_LOG_INFO("core_init: [OK] AgentOS core initialized successfully");
    return AGENTRT_SUCCESS;

cleanup_ipc:
    /* eventloop_init 失败时跳转此处 — eventloop 未初始化，无需清理 */
    agentrt_ipc_cleanup();
cleanup_task:
    agentrt_task_cleanup();
cleanup_oom:
    agentrt_oom_destroy();
cleanup_mem:
    agentrt_mem_cleanup();
fail:
    atomic_store_explicit(&g_core_initialized, 0, memory_order_seq_cst);
    return ret;
}

void agentrt_core_shutdown(void)
{
    long expected = 1;
    if (!atomic_compare_exchange_strong_explicit(&g_core_initialized, &expected, 0,
                                                 memory_order_seq_cst, memory_order_seq_cst)) {
        return;
    }

    AGENTRT_LOG_INFO("core_shutdown: shutting down AgentOS core...");

    /* 逆序关闭：heapstore → eventloop → timer → ipc → task → oom → mem */
    heapstore_integration_shutdown();
    AGENTRT_LOG_INFO("core_shutdown: [OK] heapstore shutdown");

    agentrt_time_eventloop_cleanup();
    agentrt_time_timer_cleanup();
    AGENTRT_LOG_INFO("core_shutdown: [OK] time subsystem shutdown");

    agentrt_ipc_cleanup();
    AGENTRT_LOG_INFO("core_shutdown: [OK] IPC subsystem shutdown");

    agentrt_task_cleanup();
    AGENTRT_LOG_INFO("core_shutdown: [OK] task scheduler shutdown");

    agentrt_oom_destroy();
    AGENTRT_LOG_INFO("core_shutdown: [OK] OOM handler destroyed");

    agentrt_mem_cleanup();
    AGENTRT_LOG_INFO("core_shutdown: [OK] memory allocator cleanup");

    AGENTRT_LOG_INFO("core_shutdown: AgentOS core shutdown complete");
}
