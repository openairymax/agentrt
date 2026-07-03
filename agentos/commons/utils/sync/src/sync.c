/**
 * @file sync.c
 * @brief 统一线程同步原语模块 - 核心层实现
 *
 * 提供跨平台、安全、高效的线程同步原语实现。
 * 支持Windows和POSIX系统，包含互斥锁、条件变量、信号量、读写锁等。
 *
 * @note 本文件为模块入口点，实际实现已拆分到以下文件：
 *       - sync_mutex.c: 互斥锁
 *       - sync_recursive_mutex.c: 递归互斥锁
 *       - sync_rwlock.c: 读写锁
 *       - sync_spinlock.c: 自旋锁
 *       - sync_semaphore.c: 信号量
 *       - sync_condition.c: 条件变量
 *       - sync_barrier.c: 屏障
 *       - sync_event.c: 事件
 *
 * @copyright Copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "sync.h"

#include "error.h"
#include "sync_internal.h"
#include "sync_types.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <process.h>
#include <synchapi.h>
#include <windows.h>
#else
#include <errno.h>
#include <sched.h>
#include <semaphore.h>
#include <sys/time.h>
#include <unistd.h>
#endif

#include "sync_platform.h"

/**
 * @brief 全局同步模块状态结构体
 */
typedef struct {
    sync_error_callback_t error_callback;
    void *user_context;
    bool initialized;
} sync_global_state_t;

/**
 * @brief 全局同步模块状态
 */
static sync_global_state_t g_sync_state = {NULL, NULL, false};

static bool g_initialized = false;

/**
 * @brief 初始化同步模块
 */
sync_result_t sync_init(sync_error_callback_t error_callback, void *context)
{
    if (g_initialized) {
        return SYNC_SUCCESS;
    }

    g_sync_state.error_callback = error_callback;
    g_sync_state.user_context = context;
    g_sync_state.initialized = true;
    g_initialized = true;

    return SYNC_SUCCESS;
}

/**
 * @brief 清理同步模块
 */
void sync_cleanup(void)
{
    if (!g_initialized) {
        return;
    }

    g_sync_state.error_callback = NULL;
    g_sync_state.user_context = NULL;
    g_sync_state.initialized = false;
    g_initialized = false;
}

/**
 * @brief 获取同步原语类型
 */
sync_type_t sync_get_type(void *lock, sync_lock_type_t lock_type)
{
    (void)lock;
    switch (lock_type) {
    case SYNC_LOCK_MUTEX:
        return SYNC_TYPE_MUTEX;
    case SYNC_LOCK_RECURSIVE_MUTEX:
        return SYNC_TYPE_RECURSIVE_MUTEX;
    case SYNC_LOCK_RWLOCK:
        return SYNC_TYPE_RWLOCK;
    case SYNC_LOCK_SPINLOCK:
        return SYNC_TYPE_SPINLOCK;
    case SYNC_LOCK_SEMAPHORE:
        return SYNC_TYPE_SEMAPHORE;
    case SYNC_LOCK_CONDITION:
        return SYNC_TYPE_CONDITION;
    case SYNC_LOCK_BARRIER:
        return SYNC_TYPE_BARRIER;
    case SYNC_LOCK_EVENT:
        return SYNC_TYPE_EVENT;
    default:
        return SYNC_TYPE_UNKNOWN;
    }
}

/**
 * @brief 获取锁的名称
 */
const char *sync_get_name(void *lock)
{
    if (lock == NULL) {
        return NULL;
    }

    struct sync_mutex *base = (struct sync_mutex *)lock;

    if (!base->initialized) {
        return NULL;
    }

    switch (base->type) {
    case SYNC_TYPE_MUTEX:
    case SYNC_TYPE_RECURSIVE_MUTEX:
        return ((struct sync_mutex *)lock)->name;
    case SYNC_TYPE_RWLOCK:
        return ((struct sync_rwlock *)lock)->name;
    case SYNC_TYPE_SPINLOCK:
        return ((struct sync_spinlock *)lock)->name;
    case SYNC_TYPE_SEMAPHORE:
        return ((struct sync_semaphore *)lock)->name;
    case SYNC_TYPE_CONDITION:
        return ((struct sync_condition *)lock)->name;
    case SYNC_TYPE_BARRIER:
        return ((struct sync_barrier *)lock)->name;
    case SYNC_TYPE_EVENT:
        return ((struct sync_event *)lock)->name;
    default:
        return NULL;
    }
}

/**
 * @brief 获取锁的统计信息
 */
sync_result_t sync_get_stats(void *lock, sync_stats_t *stats)
{
    if (lock == NULL || stats == NULL) {
        return SYNC_ERROR_INVALID;
    }

    struct sync_mutex *base = (struct sync_mutex *)lock;
    if (!base->initialized) {
        return SYNC_ERROR_INVALID;
    }

    // 复制实际统计信息
    *stats = base->stats;

    return SYNC_SUCCESS;
}

/**
 * @brief 重置锁的统计信息
 */
sync_result_t sync_reset_stats(void *lock)
{
    if (lock == NULL) {
        return SYNC_ERROR_INVALID;
    }

    struct sync_mutex *base = (struct sync_mutex *)lock;
    if (!base->initialized) {
        return SYNC_ERROR_INVALID;
    }

    // 实际重置统计信息
    AGENTOS_MEMSET(&base->stats, 0, sizeof(sync_stats_t));

    return SYNC_SUCCESS;
}

#define SYNC_MAX_OPTION_SLOTS 64

typedef struct {
    void *lock;
    uint64_t timeout_ms;
    bool priority_inherit;
    bool robust;
    bool in_use;
} sync_option_slot_t;

static sync_option_slot_t s_option_slots[SYNC_MAX_OPTION_SLOTS] = {{0}};
static size_t s_option_count = 0;

static sync_option_slot_t *find_option_slot(void *lock)
{
    for (size_t i = 0; i < s_option_count; i++) {
        if (s_option_slots[i].in_use && s_option_slots[i].lock == lock)
            return &s_option_slots[i];
    }
    return NULL;
}

static sync_option_slot_t *alloc_option_slot(void *lock)
{
    sync_option_slot_t *slot = find_option_slot(lock);
    if (slot)
        return slot;
    if (s_option_count >= SYNC_MAX_OPTION_SLOTS)
        return NULL;
    slot = &s_option_slots[s_option_count++];
    slot->lock = lock;
    slot->timeout_ms = 0;
    slot->priority_inherit = false;
    slot->robust = false;
    slot->in_use = true;
    return slot;
}

sync_result_t sync_set_option(void *lock, int option, void *value)
{
    if (lock == NULL || value == NULL) {
        return SYNC_ERROR_INVALID;
    }

    struct sync_mutex *base = (struct sync_mutex *)lock;
    if (!base->initialized) {
        return SYNC_ERROR_INVALID;
    }

    switch (option) {
    case SYNC_OPTION_NAME: {
        /* v0.1.1 修复：委托给 sync_set_name 进行 strdup + 注册，
         * 此前直接 base->name = name 赋值裸指针会导致悬垂指针/泄漏/非法 free。 */
        return sync_set_name(lock, (const char *)value);
    }
    case SYNC_OPTION_TIMEOUT: {
        uint64_t timeout = *(uint64_t *)value;
        sync_option_slot_t *slot = alloc_option_slot(lock);
        if (!slot)
            return SYNC_ERROR_MEMORY;
        slot->timeout_ms = timeout;
        return SYNC_SUCCESS;
    }
    case SYNC_OPTION_PRIORITY_INHERIT: {
        bool pi = *(bool *)value;
        sync_option_slot_t *slot = alloc_option_slot(lock);
        if (!slot)
            return SYNC_ERROR_MEMORY;
        slot->priority_inherit = pi;
        return SYNC_SUCCESS;
    }
    case SYNC_OPTION_ROBUST: {
        bool rb = *(bool *)value;
        sync_option_slot_t *slot = alloc_option_slot(lock);
        if (!slot)
            return SYNC_ERROR_MEMORY;
        slot->robust = rb;
        return SYNC_SUCCESS;
    }
    default:
        return SYNC_ERROR_UNSUPPORTED;
    }
}

sync_result_t sync_get_option(void *lock, int option, void *value)
{
    if (lock == NULL || value == NULL) {
        return SYNC_ERROR_INVALID;
    }

    struct sync_mutex *base = (struct sync_mutex *)lock;
    if (!base->initialized) {
        return SYNC_ERROR_INVALID;
    }

    switch (option) {
    case SYNC_OPTION_NAME: {
        const char **out = (const char **)value;
        *out = base->name;
        return SYNC_SUCCESS;
    }
    case SYNC_OPTION_TIMEOUT: {
        uint64_t *out = (uint64_t *)value;
        sync_option_slot_t *slot = find_option_slot(lock);
        *out = slot ? slot->timeout_ms : 0;
        return SYNC_SUCCESS;
    }
    case SYNC_OPTION_PRIORITY_INHERIT: {
        bool *out = (bool *)value;
        sync_option_slot_t *slot = find_option_slot(lock);
        *out = slot ? slot->priority_inherit : false;
        return SYNC_SUCCESS;
    }
    case SYNC_OPTION_ROBUST: {
        bool *out = (bool *)value;
        sync_option_slot_t *slot = find_option_slot(lock);
        *out = slot ? slot->robust : false;
        return SYNC_SUCCESS;
    }
    default:
        return SYNC_ERROR_UNSUPPORTED;
    }
}

/**
 * @brief 检查锁是否有效
 */
bool sync_is_valid(void *lock)
{
    return lock != NULL;
}

/**
 * @brief 打印锁的调试信息
 */
sync_result_t sync_debug(void *lock)
{
    if (lock == NULL) {
        return SYNC_ERROR_INVALID;
    }

    struct sync_mutex *base = (struct sync_mutex *)lock;

    fputs("\n[SYNC DEBUG] ====================\n", stderr);
    {
        char buf[256];
        snprintf(buf, sizeof(buf), "[SYNC DEBUG] Lock at: %p\n", (void *)lock);
        fputs(buf, stderr);
        snprintf(buf, sizeof(buf), "[SYNC DEBUG] Type: %d\n", base->type);
        fputs(buf, stderr);
        snprintf(buf, sizeof(buf), "[SYNC DEBUG] Initialized: %s\n",
                 base->initialized ? "true" : "false");
        fputs(buf, stderr);
    }

    const char *name = sync_get_name(lock);
    if (name != NULL) {
        char buf[256];
        snprintf(buf, sizeof(buf), "[SYNC DEBUG] Name: %s\n", name);
        fputs(buf, stderr);
    } else {
        fputs("[SYNC DEBUG] Name: (unnamed)\n", stderr);
    }

    sync_stats_t stats;
    if (sync_get_stats(lock, &stats) == SYNC_SUCCESS) {
        char buf[256];
        fputs("[SYNC DEBUG] --- Statistics ---\n", stderr);
        snprintf(buf, sizeof(buf), "[SYNC DEBUG] Lock count: %zu\n", stats.lock_count);
        fputs(buf, stderr);
        snprintf(buf, sizeof(buf), "[SYNC DEBUG] Unlock count: %zu\n", stats.unlock_count);
        fputs(buf, stderr);
        snprintf(buf, sizeof(buf), "[SYNC DEBUG] Wait count: %zu\n", stats.wait_count);
        fputs(buf, stderr);
        snprintf(buf, sizeof(buf), "[SYNC DEBUG] Timeout count: %zu\n", stats.timeout_count);
        fputs(buf, stderr);
        snprintf(buf, sizeof(buf), "[SYNC DEBUG] Deadlock count: %zu\n", stats.deadlock_count);
        fputs(buf, stderr);
        snprintf(buf, sizeof(buf), "[SYNC DEBUG] Total wait time: %lu ms\n",
                (unsigned long)stats.total_wait_time_ms);
        fputs(buf, stderr);
        snprintf(buf, sizeof(buf), "[SYNC DEBUG] Max wait time: %lu ms\n",
                (unsigned long)stats.max_wait_time_ms);
        fputs(buf, stderr);
    }

    fputs("[SYNC DEBUG] ====================\n\n", stderr);

    return SYNC_SUCCESS;
}

/**
 * @brief 获取当前时间戳（毫秒）
 */
uint64_t sync_get_timestamp_ms(void)
{
#ifdef _WIN32
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    uint64_t timestamp = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    return timestamp / 10000;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
#endif
}

/**
 * @brief 线程睡眠（毫秒）
 */
void sync_sleep_ms(uint64_t ms)
{
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    usleep((useconds_t)(ms * 1000));
#endif
}

/**
 * @brief 线程 yield
 */
void sync_yield(void)
{
#ifdef _WIN32
    SwitchToThread();
#else
    sched_yield();
#endif
}

/**
 * @brief 线程休眠（匹配 sync.h 声明的签名）
 */
void sync_sleep(unsigned int ms)
{
    sync_sleep_ms((uint64_t)ms);
}

/* ==================== 锁注册表（用于死锁检测诊断） ==================== */

#define SYNC_MAX_REGISTRY 256

typedef struct {
    void *lock;
    char *name;          /* strdup 副本，注册表拥有所有权 */
    sync_type_t type;
    bool in_use;
} sync_registry_entry_t;

static sync_registry_entry_t s_lock_registry[SYNC_MAX_REGISTRY];
static size_t s_registry_count = 0;

#ifdef _WIN32
static CRITICAL_SECTION s_registry_mutex;
static bool s_registry_mutex_init = false;
static void registry_ensure_init(void)
{
    if (!s_registry_mutex_init) {
        InitializeCriticalSection(&s_registry_mutex);
        s_registry_mutex_init = true;
    }
}
#define REGISTRY_LOCK()   do { registry_ensure_init(); EnterCriticalSection(&s_registry_mutex); } while(0)
#define REGISTRY_UNLOCK() LeaveCriticalSection(&s_registry_mutex)
#else
static pthread_mutex_t s_registry_mutex = PTHREAD_MUTEX_INITIALIZER;
#define REGISTRY_LOCK()   (void)pthread_mutex_lock(&s_registry_mutex)
#define REGISTRY_UNLOCK() (void)pthread_mutex_unlock(&s_registry_mutex)
#endif

/**
 * @brief 向全局注册表注册/更新一个命名锁（用于死锁检测诊断）
 */
static void registry_register(void *lock, const char *name, sync_type_t type)
{
    REGISTRY_LOCK();

    /* 已注册则更新名称 */
    for (size_t i = 0; i < s_registry_count; i++) {
        if (s_lock_registry[i].in_use && s_lock_registry[i].lock == lock) {
            if (s_lock_registry[i].name)
                AGENTOS_FREE(s_lock_registry[i].name);
            s_lock_registry[i].name = name ? sync_internal_strdup(name) : NULL;
            s_lock_registry[i].type = type;
            REGISTRY_UNLOCK();
            return;
        }
    }

    /* 复用已释放的槽位 */
    for (size_t i = 0; i < s_registry_count; i++) {
        if (!s_lock_registry[i].in_use) {
            s_lock_registry[i].lock = lock;
            s_lock_registry[i].name = name ? sync_internal_strdup(name) : NULL;
            s_lock_registry[i].type = type;
            s_lock_registry[i].in_use = true;
            REGISTRY_UNLOCK();
            return;
        }
    }

    /* 追加新条目 */
    if (s_registry_count < SYNC_MAX_REGISTRY) {
        s_lock_registry[s_registry_count].lock = lock;
        s_lock_registry[s_registry_count].name = name ? sync_internal_strdup(name) : NULL;
        s_lock_registry[s_registry_count].type = type;
        s_lock_registry[s_registry_count].in_use = true;
        s_registry_count++;
    }

    REGISTRY_UNLOCK();
}

/**
 * @brief 从全局注册表移除一个锁
 */
static void registry_unregister(void *lock)
{
    REGISTRY_LOCK();
    for (size_t i = 0; i < s_registry_count; i++) {
        if (s_lock_registry[i].in_use && s_lock_registry[i].lock == lock) {
            if (s_lock_registry[i].name)
                AGENTOS_FREE(s_lock_registry[i].name);
            s_lock_registry[i].in_use = false;
            s_lock_registry[i].lock = NULL;
            s_lock_registry[i].name = NULL;
            break;
        }
    }
    REGISTRY_UNLOCK();
}

/**
 * @brief 使用非阻塞 trylock 探测锁是否被持有
 *
 * 对已注册的锁执行 trylock：成功获取则立即释放（未被持有），
 * 返回 EBUSY/EAGAIN 则说明锁被持有。
 * 对于 condition/barrier/event 等无"持有"概念的类型，跳过探测。
 */
static bool registry_lock_is_held(void *lock, sync_type_t type)
{
    struct sync_mutex *base = (struct sync_mutex *)lock;
    if (!base->initialized)
        return false;

    switch (type) {
    case SYNC_TYPE_MUTEX:
    case SYNC_TYPE_RECURSIVE_MUTEX: {
        struct sync_mutex *m = (struct sync_mutex *)lock;
#ifdef _WIN32
        if (TryEnterCriticalSection(&m->mutex)) {
            LeaveCriticalSection(&m->mutex);
            return false;
        }
        return true;
#else
        int rc = pthread_mutex_trylock(&m->mutex);
        if (rc == 0) {
            pthread_mutex_unlock(&m->mutex);
            return false;
        }
        return true; /* EBUSY → 被持有 */
#endif
    }
    case SYNC_TYPE_SPINLOCK: {
        struct sync_spinlock *sp = (struct sync_spinlock *)lock;
#ifdef _WIN32
        /* Windows 自旋锁使用 atomic int，用 CAS 探测 */
        int expected = 0;
        if (_InterlockedCompareExchange((volatile LONG *)&sp->lock, 1, expected) == expected) {
            _InterlockedExchange((volatile LONG *)&sp->lock, 0);
            return false;
        }
        return true;
#else
        int rc = pthread_spin_trylock(&sp->lock);
        if (rc == 0) {
            pthread_spin_unlock(&sp->lock);
            return false;
        }
        return true;
#endif
    }
    case SYNC_TYPE_RWLOCK: {
        struct sync_rwlock *rw = (struct sync_rwlock *)lock;
#ifdef _WIN32
        if (TryAcquireSRWLockShared(&rw->rwlock)) {
            ReleaseSRWLockShared(&rw->rwlock);
            return false;
        }
        return true;
#else
        int rc = pthread_rwlock_tryrdlock(&rw->rwlock);
        if (rc == 0) {
            pthread_rwlock_unlock(&rw->rwlock);
            return false;
        }
        return true;
#endif
    }
    case SYNC_TYPE_SEMAPHORE: {
        struct sync_semaphore *sem = (struct sync_semaphore *)lock;
#ifdef _WIN32
        DWORD wr = WaitForSingleObject(sem->semaphore, 0);
        if (wr == WAIT_OBJECT_0) {
            ReleaseSemaphore(sem->semaphore, 1, NULL);
            return false;
        }
        return true; /* WAIT_TIMEOUT → 被持有 */
#else
        int rc = sem_trywait(&sem->semaphore);
        if (rc == 0) {
            sem_post(&sem->semaphore);
            return false;
        }
        return true; /* EAGAIN/EWOULDBLOCK → 被持有 */
#endif
    }
    default:
        /* condition/barrier/event 无"持有"概念，跳过 */
        return false;
    }
}

/**
 * @brief 设置锁名称
 *
 * 使用 strdup 复制名称（注册表拥有独立副本），释放旧名称，
 * 按锁类型分发到正确的结构体字段。同时将锁注册到全局注册表，
 * 供 sync_check_deadlock() 诊断使用。
 * 传入 name=NULL 时取消注册并清除名称。
 */
sync_result_t sync_set_name(void *lock, const char *name)
{
    if (lock == NULL) {
        return SYNC_ERROR_INVALID;
    }

    struct sync_mutex *base = (struct sync_mutex *)lock;
    if (!base->initialized) {
        return SYNC_ERROR_INVALID;
    }

    sync_type_t type = base->type;

    /* 释放旧名称（create 路径和本函数都用 sync_internal_strdup 分配） */
    const char *old_name = sync_get_name(lock);

    /* 按类型分发，设置新名称 */
    char *new_name = NULL;
    if (name) {
        new_name = sync_internal_strdup(name);
        if (!new_name)
            return SYNC_ERROR_MEMORY;
    }

    /* 释放旧名称并设置新名称 */
    switch (type) {
    case SYNC_TYPE_MUTEX:
    case SYNC_TYPE_RECURSIVE_MUTEX: {
        struct sync_mutex *m = (struct sync_mutex *)lock;
        if (old_name)
            AGENTOS_FREE((void *)m->name);
        m->name = new_name;
        break;
    }
    case SYNC_TYPE_RWLOCK: {
        struct sync_rwlock *rw = (struct sync_rwlock *)lock;
        if (old_name)
            AGENTOS_FREE((void *)rw->name);
        rw->name = new_name;
        break;
    }
    case SYNC_TYPE_SPINLOCK: {
        struct sync_spinlock *sp = (struct sync_spinlock *)lock;
        if (old_name)
            AGENTOS_FREE((void *)sp->name);
        sp->name = new_name;
        break;
    }
    case SYNC_TYPE_SEMAPHORE: {
        struct sync_semaphore *sem = (struct sync_semaphore *)lock;
        if (old_name)
            AGENTOS_FREE((void *)sem->name);
        sem->name = new_name;
        break;
    }
    case SYNC_TYPE_CONDITION: {
        struct sync_condition *c = (struct sync_condition *)lock;
        if (old_name)
            AGENTOS_FREE((void *)c->name);
        c->name = new_name;
        break;
    }
    case SYNC_TYPE_BARRIER: {
        struct sync_barrier *b = (struct sync_barrier *)lock;
        if (old_name)
            AGENTOS_FREE((void *)b->name);
        b->name = new_name;
        break;
    }
    case SYNC_TYPE_EVENT: {
        struct sync_event *e = (struct sync_event *)lock;
        if (old_name)
            AGENTOS_FREE((void *)e->name);
        e->name = new_name;
        break;
    }
    default:
        if (new_name)
            AGENTOS_FREE(new_name);
        return SYNC_ERROR_UNSUPPORTED;
    }

    /* 注册/注销到全局注册表 */
    if (name) {
        registry_register(lock, name, type);
    } else {
        registry_unregister(lock);
    }

    return SYNC_SUCCESS;
}

/**
 * @brief 检查死锁
 *
 * 遍历全局锁注册表，对每个已注册的命名锁执行非阻塞 trylock 探测，
 * 检测当前被持有的锁。被持有的锁可能是死锁的参与者。
 * 自清理：遍历时移除已释放（initialized=false）的陈旧条目。
 *
 * @param[out] info 死锁信息（如果检测到）
 * @param[in] max_info_size lock_names/thread_names 数组的最大条目数
 * @return 检测到被持有的锁返回 SYNC_ERROR_DEADLOCK，否则返回 SYNC_SUCCESS
 */
sync_result_t sync_check_deadlock(sync_deadlock_info_t *info, size_t max_info_size)
{
    if (info == NULL || max_info_size == 0) {
        return SYNC_ERROR_INVALID;
    }

    AGENTOS_MEMSET(info, 0, sizeof(sync_deadlock_info_t));
    info->detection_time = (uint64_t)time(NULL);

    /* 本地缓冲：收集被持有的锁名 */
    size_t cap = (max_info_size < SYNC_MAX_REGISTRY) ? max_info_size : SYNC_MAX_REGISTRY;
    char *held_names[SYNC_MAX_REGISTRY];
    for (size_t i = 0; i < SYNC_MAX_REGISTRY; i++)
        held_names[i] = NULL;
    size_t held_count = 0;   /* 实际写入 held_names 的数量 */
    size_t total_held = 0;   /* 被持有的锁总数（可能超过 cap） */

    REGISTRY_LOCK();
    for (size_t i = 0; i < s_registry_count; i++) {
        if (!s_lock_registry[i].in_use)
            continue;

        struct sync_mutex *base = (struct sync_mutex *)s_lock_registry[i].lock;

        /* 自清理：移除已释放的陈旧条目 */
        if (base == NULL || !base->initialized) {
            if (s_lock_registry[i].name)
                AGENTOS_FREE(s_lock_registry[i].name);
            s_lock_registry[i].in_use = false;
            s_lock_registry[i].lock = NULL;
            s_lock_registry[i].name = NULL;
            continue;
        }

        /* trylock 探测锁是否被持有 */
        if (registry_lock_is_held(s_lock_registry[i].lock, s_lock_registry[i].type)) {
            total_held++;
            if (held_count < cap && s_lock_registry[i].name) {
                held_names[held_count] = sync_internal_strdup(s_lock_registry[i].name);
                if (held_names[held_count])
                    held_count++;
            }
        }
    }
    REGISTRY_UNLOCK();

    info->lock_count = total_held;

    if (total_held > 0) {
        /* 分配输出数组并转移所有权 */
        if (held_count > 0) {
            info->lock_names = (char **)AGENTOS_CALLOC(held_count, sizeof(char *));
            if (info->lock_names) {
                for (size_t i = 0; i < held_count; i++)
                    info->lock_names[i] = held_names[i];
            } else {
                /* 内存不足，释放本地缓冲 */
                for (size_t i = 0; i < held_count; i++) {
                    if (held_names[i])
                        AGENTOS_FREE(held_names[i]);
                }
            }
        }
        return SYNC_ERROR_DEADLOCK;
    }

    return SYNC_SUCCESS;
}

/**
 * @brief 获取当前线程ID
 */
uint64_t sync_get_thread_id(void)
{
#ifdef _WIN32
    return (uint64_t)GetCurrentThreadId();
#else
    return (uint64_t)pthread_self();
#endif
}

/**
 * @brief 原子操作：比较并交换
 */
bool sync_atomic_cas(volatile void *ptr, uintptr_t expected, uintptr_t desired)
{
#ifdef _WIN32
    return _InterlockedCompareExchange64((volatile LONG64 *)ptr, (LONG64)desired, (LONG64)expected) == (LONG64)expected;
#else
    return __sync_bool_compare_and_swap((volatile uintptr_t *)ptr, expected, desired);
#endif
}

/**
 * @brief 原子操作：增加
 */
uintptr_t sync_atomic_add(volatile void *ptr, uintptr_t value)
{
#ifdef _WIN32
    return (uintptr_t)_InterlockedExchangeAdd64((volatile LONG64 *)ptr, (LONG64)value);
#else
    return __sync_fetch_and_add((volatile uintptr_t *)ptr, value);
#endif
}

/**
 * @brief 原子操作：减少
 */
uintptr_t sync_atomic_sub(volatile void *ptr, uintptr_t value)
{
#ifdef _WIN32
    return (uintptr_t)_InterlockedExchangeAdd64((volatile LONG64 *)ptr, -(LONG64)value);
#else
    return __sync_fetch_and_sub((volatile uintptr_t *)ptr, value);
#endif
}

/**
 * @brief 原子操作：获取
 */
uintptr_t sync_atomic_load(volatile void *ptr)
{
#ifdef _WIN32
    return (uintptr_t)_InterlockedExchangeAdd64((volatile LONG64 *)ptr, 0);
#else
    return __sync_fetch_and_add((volatile uintptr_t *)ptr, 0);
#endif
}

/**
 * @brief 原子操作：存储
 */
void sync_atomic_store(volatile void *ptr, uintptr_t value)
{
#ifdef _WIN32
    _InterlockedExchange64((volatile LONG64 *)ptr, (LONG64)value);
#else
    __sync_lock_test_and_set((volatile uintptr_t *)ptr, value);
#endif
}
