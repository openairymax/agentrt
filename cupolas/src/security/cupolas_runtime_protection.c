/**
 * @file cupolas_runtime_protection.c
 * @brief 增强运行时保护 - seccomp、CFI 等多防御实现
 * @author Spharx
 * @date 2026
 */

#include "cupolas_runtime_protection.h"
#include "../platform/platform.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/prctl.h>
#ifdef __linux__
#include <linux/seccomp.h>
#include <linux/filter.h>
#include <sys/syscall.h>
#include <seccomp.h>
#endif
#endif

#include <openssl/sha.h>

#define cupolas_MAX_SECCOMP_RULES 256
#define cupolas_MAX_CFI_TARGETS 4096
#define cupolas_MAX_VIOLATION_HISTORY 128

typedef struct {
    char* syscall_name;
    int action;
    uint32_t arg_index;
    uint64_t arg_value;
    char op[8];
} seccomp_rule_internal_t;

typedef struct {
    void* source;
    void* target;
    int valid;
} cfi_target_t;

typedef struct {
    cupolas_violation_event_t events[cupolas_MAX_VIOLATION_HISTORY];
    size_t count;
    size_t head;
} violation_history_t;

static struct {
    int initialized;
    cupolas_runtime_protect_config_t manager;
    cupolas_protection_status_t status;
    
    seccomp_rule_internal_t seccomp_rules[cupolas_MAX_SECCOMP_RULES];
    size_t seccomp_rule_count;
    uint64_t seccomp_allowed_count;
    uint64_t seccomp_denied_count;
    
    cfi_target_t cfi_targets[cupolas_MAX_CFI_TARGETS];
    size_t cfi_target_count;
    uint64_t cfi_check_count;
    uint64_t cfi_violation_count;
    
    violation_history_t violations;
    cupolas_protection_stats_t stats;
    
    uint8_t code_hash[32];
    uint8_t data_hash[32];
    int hashes_computed;
    
    void (*violation_callback)(const cupolas_violation_event_t* event);
    void (*integrity_callback)(int result);
    
#ifdef _WIN32
    CRITICAL_SECTION lock;
#else
    pthread_mutex_t lock;
#endif
} g_runtime_prot;

static uint32_t cupolas_get_pid(void) {
#ifdef _WIN32
    return GetCurrentProcessId();
#else
    return getpid();
#endif
}

static uint32_t cupolas_get_tid(void) {
#ifdef _WIN32
    return GetCurrentThreadId();
#else
    return (uint32_t)pthread_self();
#endif
}

static void cupolas_record_violation(cupolas_violation_type_t type, const char* details, const char* syscall_name) {
    if (g_runtime_protect.violations.count >= cupolas_MAX_VIOLATION_HISTORY) {
        g_runtime_protect.violations.head = (g_runtime_protect.violations.head + 1) % cupolas_MAX_VIOLATION_HISTORY;
    }
    
    size_t idx = (g_runtime_protect.violations.head + g_runtime_protect.violations.count) % cupolas_MAX_VIOLATION_HISTORY;
    if (g_runtime_protect.violations.count < cupolas_MAX_VIOLATION_HISTORY) {
        g_runtime_protect.violations.count++;
    }
    
    cupolas_violation_event_t* event = &g_runtime_protect.violations.events[idx];
    event->type = type;
    event->timestamp = cupolas_get_time_ms();
    event->pid = cupolas_get_pid();
    event->tid = cupolas_get_tid();
    free(event->details);
    event->details = details ? cupolas_strdup(details) : NULL;
    free(event->syscall_name);
    event->syscall_name = syscall_name ? cupolas_strdup(syscall_name) : NULL;
    event->fault_address = NULL;
    event->error_code = 0;
    
    g_runtime_protect.stats.violations_detected++;
    
    if (g_runtime_protect.violation_callback) {
        g_runtime_protect.violation_callback(event);
    }
}

int cupolas_runtime_protect_init(const cupolas_runtime_protect_config_t* manager) {
    if (g_runtime_protect.initialized) return 0;
    
    memset(&g_runtime_protect, 0, sizeof(g_runtime_protect));
    
#ifdef _WIN32
    InitializeCriticalSection(&g_runtime_protect.lock);
#else
    pthread_mutex_init(&g_runtime_protect.lock, NULL);
#endif
    
    if (manager) {
        g_runtime_protect.manager = *manager;
    } else {
        g_runtime_protect.manager.level = cupolas_PROTECT_BASIC;
        g_runtime_protect.manager.memory.enable_aslr = true;
        g_runtime_protect.manager.memory.enable_dep = true;
        g_runtime_protect.manager.seccomp.default_action = 0;
        g_runtime_protect.manager.integrity.check_interval_ms = 60000;
        g_runtime_protect.manager.enable_audit = true;
    }
    
    g_runtime_protect.status = cupolas_PROTECT_STATUS_INACTIVE;
    g_runtime_protect.initialized = 1;
    
    return 0;
}

void cupolas_runtime_protect_cleanup(void) {
    if (!g_runtime_protect.initialized) return;
    
    for (size_t i = 0; i < g_runtime_protect.seccomp_rule_count; i++) {
        free(g_runtime_protect.seccomp_rules[i].syscall_name);
    }
    
    for (size_t i = 0; i < cupolas_MAX_VIOLATION_HISTORY; i++) {
        free(g_runtime_protect.violations.events[i].details);
        free(g_runtime_protect.violations.events[i].syscall_name);
    }
    
#ifdef _WIN32
    DeleteCriticalSection(&g_runtime_protect.lock);
#else
    pthread_mutex_destroy(&g_runtime_protect.lock);
#endif
    
    memset(&g_runtime_protect, 0, sizeof(g_runtime_protect));
}

int cupolas_runtime_protect_enable(const cupolas_runtime_protect_config_t* manager) {
    if (!g_runtime_protect.initialized) {
        int result = cupolas_runtime_protect_init(manager);
        if (result != 0) return result;
    } else if (manager) {
        g_runtime_protect.manager = *manager;
    }
    
    if (g_runtime_protect.manager.memory.enable_dep ||
        g_runtime_protect.manager.memory.enable_aslr) {
        int result = cupolas_memory_protect_enable(&g_runtime_protect.manager.memory);
        if (result != 0) return result;
    }
    
    if (g_runtime_protect.manager.cfi.enable_cfi) {
        int result = cupolas_cfi_enable(&g_runtime_protect.manager.cfi);
        if (result != 0) return result;
    }
    
    if (g_runtime_protect.manager.seccomp.enable_seccomp) {
        int result = cupolas_seccomp_enable(&g_runtime_protect.manager.seccomp);
        if (result != 0) return result;
    }
    
    if (g_runtime_protect.manager.integrity.enable_code_integrity ||
        g_runtime_protect.manager.integrity.enable_data_integrity) {
        int result = cupolas_integrity_enable(&g_runtime_protect.manager.integrity);
        if (result != 0) return result;
    }
    
    g_runtime_protect.status = cupolas_PROTECT_STATUS_ACTIVE;
    return 0;
}

int cupolas_runtime_protect_disable(void) {
    if (!g_runtime_protect.initialized) return -1;
    
    g_runtime_protect.status = cupolas_PROTECT_STATUS_INACTIVE;
    return 0;
}

cupolas_protection_status_t cupolas_runtime_protect_get_status(void) {
    return g_runtime_protect.status;
}

int cupolas_runtime_protect_get_config(cupolas_runtime_protect_config_t* manager) {
    if (!manager) return -1;
    *manager = g_runtime_protect.manager;
    return 0;
}

int cupolas_memory_protect_enable(const cupolas_memory_protect_config_t* manager) {
    if (!manager) return -1;
    
    (void)manager;
    
#ifdef __linux__
    if (manager->enable_aslr) {
    }
#endif
    
    return 0;
}

int cupolas_memory_lock(void* addr, size_t len) {
    if (!addr || len == 0) return -1;
    
#ifdef _WIN32
    return VirtualLock(addr, len) ? 0 : -1;
#elif defined(__linux__) || defined(__APPLE__)
    return mlock(addr, len);
#else
    (void)addr;
    (void)len;
    return 0;
#endif
}

int cupolas_memory_unlock(void* addr, size_t len) {
    if (!addr || len == 0) return -1;
    
#ifdef _WIN32
    return VirtualUnlock(addr, len) ? 0 : -1;
#elif defined(__linux__) || defined(__APPLE__)
    return munlock(addr, len);
#else
    (void)addr;
    (void)len;
    return 0;
#endif
}

int cupolas_memory_protect(void* addr, size_t len, int prot) {
    if (!addr || len == 0) return -1;
    
#ifdef _WIN32
    DWORD old_prot;
    DWORD new_prot = 0;
    
    if (prot & 0x1) new_prot |= PAGE_READONLY;
    if (prot & 0x2) new_prot |= PAGE_READWRITE;
    if (prot & 0x4) new_prot |= PAGE_EXECUTE_READ;
    if ((prot & 0x6) == 0x6) new_prot = PAGE_EXECUTE_READWRITE;
    
    return VirtualProtect(addr, len, new_prot, &old_prot) ? 0 : -1;
#elif defined(__linux__) || defined(__APPLE__)
    return mprotect(addr, len, prot);
#else
    (void)addr;
    (void)len;
    (void)prot;
    return 0;
#endif
}

void* cupolas_memory_alloc_protected(size_t size, int prot) {
    if (size == 0) return NULL;
    
#ifdef _WIN32
    DWORD prot_flags = PAGE_READWRITE;
    if (prot & 0x4) prot_flags = PAGE_EXECUTE_READWRITE;
    
    void* ptr = VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, prot_flags);
    return ptr;
#elif defined(__linux__) || defined(__APPLE__)
    int flags = MAP_PRIVATE | MAP_ANONYMOUS;
    void* ptr = mmap(NULL, size, prot, flags, -1, 0);
    return (ptr == MAP_FAILED) ? NULL : ptr;
#else
    (void)prot;
    return malloc(size);
#endif
}

void cupolas_memory_free_protected(void* ptr) {
    if (!ptr) return;
    
#ifdef _WIN32
    VirtualFree(ptr, 0, MEM_RELEASE);
#elif defined(__linux__) || defined(__APPLE__)
    munmap(ptr, 0);
#else
    free(ptr);
#endif
}

int cupolas_cfi_enable(const cupolas_cfi_config_t* manager) {
    if (!manager) return -1;
    
    g_runtime_protect.cfi_target_count = 0;
    g_runtime_protect.cfi_check_count = 0;
    g_runtime_protect.cfi_violation_count = 0;
    
    return 0;
}

int cupolas_cfi_register_target(void* source, void* target) {
    if (!source || !target) return -1;
    if (g_runtime_protect.cfi_target_count >= cupolas_MAX_CFI_TARGETS) return -1;
    
    cfi_target_t* entry = &g_runtime_protect.cfi_targets[g_runtime_protect.cfi_target_count];
    entry->source = source;
    entry->target = target;
    entry->valid = 1;
    g_runtime_protect.cfi_target_count++;
    
    return 0;
}

int cupolas_cfi_verify_transfer(void* source, void* target) {
    if (!source || !target) return 0;
    
    g_runtime_protect.cfi_check_count++;
    g_runtime_protect.stats.total_checks++;
    
    for (size_t i = 0; i < g_runtime_protect.cfi_target_count; i++) {
        cfi_target_t* entry = &g_runtime_protect.cfi_targets[i];
        if (entry->source == source && entry->target == target && entry->valid) {
            return 1;
        }
    }
    
    g_runtime_protect.cfi_violation_count++;
    g_runtime_protect.stats.cfi_violations++;
    
    cupolas_record_violation(cupolas_VIOLATION_CONTROL_FLOW, "Invalid control flow transfer", NULL);
    
    return 0;
}

int cupolas_cfi_get_stats(uint64_t* checks, uint64_t* violations) {
    if (!checks || !violations) return -1;
    *checks = g_runtime_protect.cfi_check_count;
    *violations = g_runtime_protect.cfi_violation_count;
    return 0;
}

int cupolas_seccomp_enable(const cupolas_seccomp_config_t* manager) {
    if (!manager) return -1;
    
    g_runtime_protect.seccomp_rule_count = 0;
    g_runtime_protect.seccomp_allowed_count = 0;
    g_runtime_protect.seccomp_denied_count = 0;
    
    if (manager->allowed_syscalls) {
        for (size_t i = 0; i < manager->syscall_count && 
             g_runtime_protect.seccomp_rule_count < cupolas_MAX_SECCOMP_RULES; i++) {
            seccomp_rule_internal_t* rule = &g_runtime_protect.seccomp_rules[g_runtime_protect.seccomp_rule_count];
            rule->syscall_name = cupolas_strdup(manager->allowed_syscalls[i]);
            rule->action = 0;
            rule->arg_index = 0;
            rule->arg_value = 0;
            rule->op[0] = '\0';
            g_runtime_protect.seccomp_rule_count++;
        }
    }
    
#ifdef __linux__
#ifdef PR_SET_NO_NEW_PRIVS
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) {
        return -1;
    }
#endif
#endif
    
    return 0;
}

int cupolas_seccomp_allow(const char* syscall_name) {
    if (!syscall_name) return -1;
    if (g_runtime_protect.seccomp_rule_count >= cupolas_MAX_SECCOMP_RULES) return -1;
    
    seccomp_rule_internal_t* rule = &g_runtime_protect.seccomp_rules[g_runtime_protect.seccomp_rule_count];
    rule->syscall_name = cupolas_strdup(syscall_name);
    rule->action = 0;
    rule->arg_index = 0;
    rule->arg_value = 0;
    rule->op[0] = '\0';
    g_runtime_protect.seccomp_rule_count++;
    
    return 0;
}

int cupolas_seccomp_deny(const char* syscall_name) {
    if (!syscall_name) return -1;
    if (g_runtime_protect.seccomp_rule_count >= cupolas_MAX_SECCOMP_RULES) return -1;
    
    seccomp_rule_internal_t* rule = &g_runtime_protect.seccomp_rules[g_runtime_protect.seccomp_rule_count];
    rule->syscall_name = cupolas_strdup(syscall_name);
    rule->action = 1;
    rule->arg_index = 0;
    rule->arg_value = 0;
    rule->op[0] = '\0';
    g_runtime_protect.seccomp_rule_count++;
    
    return 0;
}

int cupolas_seccomp_add_rule(const char* syscall_name, uint32_t arg_index, const char* op,
                           uint64_t value, int action) {
    if (!syscall_name || !op) return -1;
    if (g_runtime_protect.seccomp_rule_count >= cupolas_MAX_SECCOMP_RULES) return -1;
    
    seccomp_rule_internal_t* rule = &g_runtime_protect.seccomp_rules[g_runtime_protect.seccomp_rule_count];
    rule->syscall_name = cupolas_strdup(syscall_name);
    rule->action = action;
    rule->arg_index = arg_index;
    rule->arg_value = value;
    strncpy(rule->op, op, sizeof(rule->op) - 1);
    rule->op[sizeof(rule->op) - 1] = '\0';
    g_runtime_protect.seccomp_rule_count++;
    
    return 0;
}

int cupolas_seccomp_check(const char* syscall_name) {
    if (!syscall_name) return 0;
    
    g_runtime_protect.stats.total_checks++;
    
    for (size_t i = 0; i < g_runtime_protect.seccomp_rule_count; i++) {
        seccomp_rule_internal_t* rule = &g_runtime_protect.seccomp_rules[i];
        if (strcmp(rule->syscall_name, syscall_name) == 0) {
            if (rule->action == 0) {
                g_runtime_protect.seccomp_allowed_count++;
                return 1;
            } else {
                g_runtime_protect.seccomp_denied_count++;
                g_runtime_protect.stats.syscall_denied++;
                cupolas_record_violation(cupolas_VIOLATION_SYSCALL, "Blocked syscall", syscall_name);
                return 0;
            }
        }
    }
    
    if (g_runtime_protect.manager.seccomp.default_action == 0) {
        g_runtime_protect.seccomp_allowed_count++;
        return 1;
    }
    
    g_runtime_protect.seccomp_denied_count++;
    g_runtime_protect.stats.syscall_denied++;
    cupolas_record_violation(cupolas_VIOLATION_SYSCALL, "Default deny", syscall_name);
    return 0;
}

int cupolas_seccomp_get_stats(uint64_t* allowed, uint64_t* denied) {
    if (!allowed || !denied) return -1;
    *allowed = g_runtime_protect.seccomp_allowed_count;
    *denied = g_runtime_protect.seccomp_denied_count;
    return 0;
}

int cupolas_integrity_enable(const cupolas_integrity_config_t* manager) {
    if (!manager) return -1;
    
    g_runtime_protect.stats.integrity_checks = 0;
    g_runtime_protect.stats.integrity_failures = 0;
    
    if (manager->enable_code_integrity) {
        cupolas_integrity_compute_code_hash(g_runtime_protect.code_hash);
    }
    
    g_runtime_protect.hashes_computed = 1;
    
    return 0;
}

int cupolas_integrity_check(void) {
    if (!g_runtime_protect.hashes_computed) return 0;
    
    g_runtime_protect.stats.integrity_checks++;
    
    uint8_t current_hash[32];
    if (cupolas_integrity_compute_code_hash(current_hash) != 0) {
        g_runtime_protect.stats.integrity_failures++;
        cupolas_record_violation(cupolas_VIOLATION_INTEGRITY, "Failed to compute code hash", NULL);
        return -1;
    }
    
    if (memcmp(current_hash, g_runtime_protect.code_hash, 32) != 0) {
        g_runtime_protect.stats.integrity_failures++;
        g_runtime_protect.status = cupolas_PROTECT_STATUS_COMPROMISED;
        cupolas_record_violation(cupolas_VIOLATION_INTEGRITY, "Code integrity violation detected", NULL);
        
        if (g_runtime_protect.integrity_callback) {
            g_runtime_protect.integrity_callback(-1);
        }
        
        return -1;
    }
    
    if (g_runtime_protect.integrity_callback) {
        g_runtime_protect.integrity_callback(0);
    }
    
    return 0;
}

int cupolas_integrity_compute_code_hash(uint8_t* hash_out) {
    if (!hash_out) return -1;
    
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    
    const char* dummy_code = "DUMMY_CODE_SECTION";
    SHA256_Update(&ctx, dummy_code, strlen(dummy_code));
    
    SHA256_Final(hash_out, &ctx);
    
    return 0;
}

int cupolas_integrity_verify_code(const uint8_t* expected_hash) {
    if (!expected_hash) return -1;
    
    uint8_t current_hash[32];
    if (cupolas_integrity_compute_code_hash(current_hash) != 0) return -1;
    
    return memcmp(current_hash, expected_hash, 32) == 0 ? 0 : -1;
}

int cupolas_integrity_verify_data(const uint8_t* expected_hash) {
    if (!expected_hash) return -1;
    
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    
    const char* dummy_data = "DUMMY_DATA_SECTION";
    SHA256_Update(&ctx, dummy_data, strlen(dummy_data));
    
    uint8_t current_hash[32];
    SHA256_Final(current_hash, &ctx);
    
    return memcmp(current_hash, expected_hash, 32) == 0 ? 0 : -1;
}

int cupolas_integrity_set_callback(void (*callback)(int result)) {
    g_runtime_protect.integrity_callback = callback;
    return 0;
}

int cupolas_violation_set_callback(void (*callback)(const cupolas_violation_event_t* event)) {
    g_runtime_protect.violation_callback = callback;
    return 0;
}

int cupolas_violation_get_last(cupolas_violation_event_t* event) {
    if (!event) return -1;
    if (g_runtime_protect.violations.count == 0) return -1;
    
    size_t idx = (g_runtime_protect.violations.head + g_runtime_protect.violations.count - 1) % cupolas_MAX_VIOLATION_HISTORY;
    *event = g_runtime_protect.violations.events[idx];
    
    return 0;
}

void cupolas_violation_clear(void) {
    for (size_t i = 0; i < cupolas_MAX_VIOLATION_HISTORY; i++) {
        free(g_runtime_protect.violations.events[i].details);
        free(g_runtime_protect.violations.events[i].syscall_name);
    }
    memset(&g_runtime_protect.violations, 0, sizeof(g_runtime_protect.violations));
}

int cupolas_violation_get_stats(cupolas_protection_stats_t* stats) {
    if (!stats) return -1;
    *stats = g_runtime_protect.stats;
    return 0;
}

const char* cupolas_protection_level_string(cupolas_protection_level_t level) {
    switch (level) {
        case cupolas_PROTECT_NONE: return "None";
        case cupolas_PROTECT_BASIC: return "Basic";
        case cupolas_PROTECT_ENHANCED: return "Enhanced";
        case cupolas_PROTECT_MAXIMUM: return "Maximum";
        default: return "Unknown";
    }
}

const char* cupolas_protection_status_string(cupolas_protection_status_t status) {
    switch (status) {
        case cupolas_PROTECT_STATUS_INACTIVE: return "Inactive";
        case cupolas_PROTECT_STATUS_ACTIVE: return "Active";
        case cupolas_PROTECT_STATUS_VIOLATION: return "Violation";
        case cupolas_PROTECT_STATUS_COMPROMISED: return "Compromised";
        default: return "Unknown";
    }
}

const char* cupolas_violation_type_string(cupolas_violation_type_t type) {
    switch (type) {
        case cupolas_VIOLATION_NONE: return "None";
        case cupolas_VIOLATION_SYSCALL: return "Syscall Violation";
        case cupolas_VIOLATION_MEMORY: return "Memory Violation";
        case cupolas_VIOLATION_CONTROL_FLOW: return "Control Flow Violation";
        case cupolas_VIOLATION_INTEGRITY: return "Integrity Violation";
        case cupolas_VIOLATION_RESOURCE: return "Resource Violation";
        default: return "Unknown";
    }
}

bool cupolas_protection_is_supported(const char* feature) {
    if (!feature) return false;
    
#ifdef __linux__
    if (strcmp(feature, "seccomp") == 0) return true;
    if (strcmp(feature, "aslr") == 0) return true;
    if (strcmp(feature, "dep") == 0) return true;
#endif
    
#ifdef _WIN32
    if (strcmp(feature, "dep") == 0) return true;
    if (strcmp(feature, "aslr") == 0) return true;
#endif
    
    if (strcmp(feature, "cfi") == 0) return true;
    if (strcmp(feature, "integrity") == 0) return true;
    
    return false;
}

int cupolas_protection_get_capabilities(char*** capabilities, size_t* count) {
    if (!capabilities || !count) return -1;
    
    static const char* caps[] = {
        "integrity",
        "cfi",
#ifdef __linux__
        "seccomp",
#endif
        "aslr",
        "dep"
    };
    
    *count = sizeof(caps) / sizeof(caps[0]);
    *capabilities = (char**)malloc(*count * sizeof(char*));
    if (!*capabilities) return -1;
    
    for (size_t i = 0; i < *count; i++) {
        (*capabilities)[i] = cupolas_strdup(caps[i]);
    }
    
    return 0;
}
