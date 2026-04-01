/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * cupolas_runtime_protection.h - Runtime Protection: seccomp, CFI, and Multiple Defense Layers
 */

#ifndef CUPOLAS_RUNTIME_PROTECTION_H
#define CUPOLAS_RUNTIME_PROTECTION_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Protection levels
 * 
 * Design principles:
 * - Layered defense: Multiple security layers
 * - Memory protection: ASLR, DEP, stack canaries
 * - Control flow integrity: Validate indirect branches
 * - Anti-tampering: Runtime integrity checks
 * - Least privilege: Only necessary syscalls allowed
 */
typedef enum {
    CUPOLAS_PROTECT_NONE = 0,         /**< No protection */
    CUPOLAS_PROTECT_BASIC = 1,        /**< Basic protection */
    CUPOLAS_PROTECT_ENHANCED = 2,     /**< Enhanced protection */
    CUPOLAS_PROTECT_MAXIMUM = 3       /**< Maximum protection */
} cupolas_protection_level_t;

/**
 * @brief Protection status
 */
typedef enum {
    CUPOLAS_PROTECT_STATUS_INACTIVE = 0,
    CUPOLAS_PROTECT_STATUS_ACTIVE = 1,
    CUPOLAS_PROTECT_STATUS_VIOLATION = 2,
    CUPOLAS_PROTECT_STATUS_COMPROMISED = 3
} cupolas_protection_status_t;

/**
 * @brief Violation types
 */
typedef enum {
    CUPOLAS_VIOLATION_NONE = 0,
    CUPOLAS_VIOLATION_SYSCALL = 1,        /**< Illegal syscall */
    CUPOLAS_VIOLATION_MEMORY = 2,         /**< Memory violation */
    CUPOLAS_VIOLATION_CONTROL_FLOW = 3,   /**< CFI violation */
    CUPOLAS_VIOLATION_INTEGRITY = 4,      /**< Integrity violation */
    CUPOLAS_VIOLATION_RESOURCE = 5        /**< Resource violation */
} cupolas_violation_type_t;

/**
 * @brief Memory protection configuration
 */
typedef struct {
    bool enable_aslr;               /**< Address space layout randomization */
    bool enable_dep;                /**< Data execution prevention (NX bit) */
    bool enable_stack_protector;    /**< Stack protector (Stack Canary) */
    bool enable_heap_guard;         /**< Heap guard pages */
    bool enable_mprotect;           /**< Memory page protection */
    bool enable_guard_pages;        /**< Guard pages */
    uint32_t stack_canary_type;     /**< Stack canary type */
} cupolas_memory_protect_config_t;

/**
 * @brief Control flow integrity configuration
 */
typedef struct {
    bool enable_cfi;                /**< Control flow integrity */
    bool enable_safestack;          /**< SafeStack */
    bool enable_shadow_stack;       /**< Shadow stack */
    bool enable_ibt;                /**< Indirect branch tracking */
    bool enable_cet;                /**< Control-flow enforcement technology */
    uint32_t cfi_level;             /**< CFI level (1-3) */
} cupolas_cfi_config_t;

/**
 * @brief Syscall filtering configuration
 */
typedef struct {
    bool enable_seccomp;            /**< Enable seccomp */
    bool enable_seccomp_bpf;        /**< Enable BPF filtering */
    int default_action;             /**< Default action (allow/deny/kill) */
    const char** allowed_syscalls;  /**< Allowed syscalls list */
    size_t syscall_count;           /**< Number of syscalls */
    const char** log_syscalls;      /**< Syscalls to log */
    size_t log_count;               /**< Log count */
} cupolas_seccomp_config_t;

/**
 * @brief Integrity checking configuration
 */
typedef struct {
    bool enable_code_integrity;     /**< Enable code integrity */
    bool enable_data_integrity;     /**< Enable data integrity */
    bool enable_ro_sections;        /**< Enable read-only sections */
    bool enable_self_check;         /**< Enable self-check */
    uint32_t check_interval_ms;     /**< Check interval (milliseconds) */
    uint32_t hash_algorithm;        /**< Hash algorithm */
} cupolas_integrity_config_t;

/**
 * @brief Runtime protection configuration
 */
typedef struct {
    cupolas_protection_level_t level;
    
    cupolas_memory_protect_config_t memory;
    cupolas_cfi_config_t cfi;
    cupolas_seccomp_config_t seccomp;
    cupolas_integrity_config_t integrity;
    
    bool enable_audit;              /**< Enable audit logging */
    bool enable_violation_handler;  /**< Enable violation handler */
    void (*violation_callback)(cupolas_violation_type_t type, const char* details);
} cupolas_runtime_protect_config_t;

/**
 * @brief Violation event structure
 */
typedef struct {
    cupolas_violation_type_t type;
    uint64_t timestamp;
    uint32_t pid;
    uint32_t tid;
    char* details;
    char* syscall_name;
    void* fault_address;
    int error_code;
} cupolas_violation_event_t;

/**
 * @brief Protection statistics
 */
typedef struct {
    uint64_t total_checks;
    uint64_t violations_detected;
    uint64_t violations_blocked;
    uint64_t integrity_checks;
    uint64_t integrity_failures;
    uint64_t syscall_denied;
    uint64_t memory_violations;
    uint64_t cfi_violations;
} cupolas_protection_stats_t;

/**
 * @brief Initialize runtime protection module
 * @param[in] config Configuration (NULL for defaults)
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads (initialization only)
 * @reentrant No
 * @ownership config: caller retains ownership
 */
int cupolas_runtime_protect_init(const cupolas_runtime_protect_config_t* config);

/**
 * @brief Shutdown runtime protection module
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other operations)
 * @reentrant No
 */
void cupolas_runtime_protect_cleanup(void);

/**
 * @brief Enable runtime protection
 * @param[in] config Protection configuration
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other operations)
 * @reentrant No
 * @ownership config: caller retains ownership
 */
int cupolas_runtime_protect_enable(const cupolas_runtime_protect_config_t* config);

/**
 * @brief Disable runtime protection
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other operations)
 * @reentrant No
 */
int cupolas_runtime_protect_disable(void);

/**
 * @brief Get protection status
 * @return Protection status
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 */
cupolas_protection_status_t cupolas_runtime_protect_get_status(void);

/**
 * @brief Get current configuration
 * @param[out] config Configuration output
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * @ownership config: caller provides buffer, function writes to it
 */
int cupolas_runtime_protect_get_config(cupolas_runtime_protect_config_t* config);

/**
 * @brief Enable memory protection
 * @param[in] config Memory protection configuration
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other operations)
 * @reentrant No
 * @ownership config: caller retains ownership
 */
int cupolas_memory_protect_enable(const cupolas_memory_protect_config_t* config);

/**
 * @brief Lock memory pages
 * @param[in] addr Memory address
 * @param[in] len Length
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads
 * @reentrant No
 */
int cupolas_memory_lock(void* addr, size_t len);

/**
 * @brief Unlock memory pages
 * @param[in] addr Memory address
 * @param[in] len Length
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads
 * @reentrant No
 */
int cupolas_memory_unlock(void* addr, size_t len);

/**
 * @brief Protect memory pages
 * @param[in] addr Memory address
 * @param[in] len Length
 * @param[in] prot Protection flags (PROT_READ | PROT_WRITE | PROT_EXEC)
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads
 * @reentrant No
 */
int cupolas_memory_protect(void* addr, size_t len, int prot);

/**
 * @brief Allocate protected memory
 * @param[in] size Size
 * @param[in] prot Protection flags
 * @return Memory pointer, NULL on failure
 * @note Thread-safe: Safe to call from multiple threads
 * @reentrant No
 */
void* cupolas_memory_alloc_protected(size_t size, int prot);

/**
 * @brief Free protected memory
 * @param[in] ptr Memory pointer
 * @note Thread-safe: Safe to call from multiple threads
 * @reentrant No
 */
void cupolas_memory_free_protected(void* ptr);

/**
 * @brief Enable control flow integrity
 * @param[in] config CFI configuration
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other operations)
 * @reentrant No
 * @ownership config: caller retains ownership
 */
int cupolas_cfi_enable(const cupolas_cfi_config_t* config);

/**
 * @brief Register valid branch target
 * @param[in] source Source address
 * @param[in] target Target address
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads
 * @reentrant No
 */
int cupolas_cfi_register_target(void* source, void* target);

/**
 * @brief Verify control flow transfer
 * @param[in] source Source address
 * @param[in] target Target address
 * @return 1 if valid, 0 if invalid
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 */
int cupolas_cfi_verify_transfer(void* source, void* target);

/**
 * @brief Get CFI statistics
 * @param[out] checks Number of checks
 * @param[out] violations Number of violations
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * @ownership checks and violations: caller provides buffers, function writes to them
 */
int cupolas_cfi_get_stats(uint64_t* checks, uint64_t* violations);

/**
 * @brief Enable syscall filtering
 * @param[in] config Seccomp configuration
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other operations)
 * @reentrant No
 * @ownership config: caller retains ownership
 */
int cupolas_seccomp_enable(const cupolas_seccomp_config_t* config);

/**
 * @brief Allow a syscall
 * @param[in] syscall_name Syscall name
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other operations)
 * @reentrant No
 * @ownership syscall_name: caller retains ownership
 */
int cupolas_seccomp_allow(const char* syscall_name);

/**
 * @brief Deny a syscall
 * @param[in] syscall_name Syscall name
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other operations)
 * @reentrant No
 * @ownership syscall_name: caller retains ownership
 */
int cupolas_seccomp_deny(const char* syscall_name);

/**
 * @brief Add syscall rule with argument filtering
 * @param[in] syscall_name Syscall name
 * @param[in] arg_index Argument index
 * @param[in] op Operator (==, !=, <, >, &)
 * @param[in] value Value
 * @param[in] action Action
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other operations)
 * @reentrant No
 * @ownership syscall_name and op: caller retains ownership
 */
int cupolas_seccomp_add_rule(const char* syscall_name, 
                            uint32_t arg_index,
                            const char* op,
                            uint64_t value,
                            int action);

/**
 * @brief Check if syscall is allowed
 * @param[in] syscall_name Syscall name
 * @return 1 if allowed, 0 if denied
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * @ownership syscall_name: caller retains ownership
 */
int cupolas_seccomp_check(const char* syscall_name);

/**
 * @brief Get seccomp statistics
 * @param[out] allowed Number of allowed syscalls
 * @param[out] denied Number of denied syscalls
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * @ownership allowed and denied: caller provides buffers, function writes to them
 */
int cupolas_seccomp_get_stats(uint64_t* allowed, uint64_t* denied);

/**
 * @brief Enable integrity checking
 * @param[in] config Integrity configuration
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other operations)
 * @reentrant No
 * @ownership config: caller retains ownership
 */
int cupolas_integrity_enable(const cupolas_integrity_config_t* config);

/**
 * @brief Perform integrity check
 * @return 0 if intact, negative if compromised
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 */
int cupolas_integrity_check(void);

/**
 * @brief Compute code section hash
 * @param[out] hash_out Hash output (32 bytes)
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads
 * @reentrant Yes
 * @ownership hash_out: caller provides buffer, function writes to it
 */
int cupolas_integrity_compute_code_hash(uint8_t* hash_out);

/**
 * @brief Verify code section integrity
 * @param[in] expected_hash Expected hash
 * @return 0 if intact, negative if compromised
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * @ownership expected_hash: caller retains ownership
 */
int cupolas_integrity_verify_code(const uint8_t* expected_hash);

/**
 * @brief Verify data section integrity
 * @param[in] expected_hash Expected hash
 * @return 0 if intact, negative if compromised
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * @ownership expected_hash: caller retains ownership
 */
int cupolas_integrity_verify_data(const uint8_t* expected_hash);

/**
 * @brief Set integrity check callback
 * @param[in] callback Callback function
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads
 * @reentrant No
 */
int cupolas_integrity_set_callback(void (*callback)(int result));

/**
 * @brief Set violation handler callback
 * @param[in] callback Callback function
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads
 * @reentrant No
 */
int cupolas_violation_set_callback(void (*callback)(const cupolas_violation_event_t* event));

/**
 * @brief Get last violation event
 * @param[out] event Event output
 * @return 0 on success, negative if no event
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * @ownership event: caller provides buffer, function writes to it
 */
int cupolas_violation_get_last(cupolas_violation_event_t* event);

/**
 * @brief Clear violation events
 * @note Thread-safe: Safe to call from multiple threads
 * @reentrant No
 */
void cupolas_violation_clear(void);

/**
 * @brief Get violation statistics
 * @param[out] stats Statistics output
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * @ownership stats: caller provides buffer, function writes to it
 */
int cupolas_violation_get_stats(cupolas_protection_stats_t* stats);

/**
 * @brief Get protection level string
 * @param[in] level Protection level
 * @return Level name string (static, do not free)
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 */
const char* cupolas_protection_level_string(cupolas_protection_level_t level);

/**
 * @brief Get protection status string
 * @param[in] status Protection status
 * @return Status string (static, do not free)
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 */
const char* cupolas_protection_status_string(cupolas_protection_status_t status);

/**
 * @brief Get violation type string
 * @param[in] type Violation type
 * @return Type name string (static, do not free)
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 */
const char* cupolas_violation_type_string(cupolas_violation_type_t type);

/**
 * @brief Check if protection feature is supported
 * @param[in] feature Feature name (aslr, dep, cfi, seccomp, etc.)
 * @return true if supported, false otherwise
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * @ownership feature: caller retains ownership
 */
bool cupolas_protection_is_supported(const char* feature);

/**
 * @brief Get system protection capabilities
 * @param[out] capabilities Capabilities list output
 * @param[out] count Number of capabilities
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * @ownership capabilities: caller provides buffer, function writes to it
 * @ownership count: caller provides buffer, function writes to it
 */
int cupolas_protection_get_capabilities(char*** capabilities, size_t* count);

#ifdef __cplusplus
}
#endif

#endif /* CUPOLAS_RUNTIME_PROTECTION_H */
