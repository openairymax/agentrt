/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * cupolas_utils.h - Common Utility Macros and Functions
 *
 * @brief Unified abstraction layer for Cupolas security module
 * @author Spharx AgentOS Team
 * @date 2026-04-05
 * @version 1.0.0
 *
 * This header provides platform-independent macros and utility functions
 * that eliminate code duplication across all Cupolas submodules.
 *
 * @section design Design Principles
 * - **Zero-cost abstraction**: Macros expand to direct API calls
 * - **Cross-platform**: Windows (CRITICAL_SECTION) / POSIX (pthread)
 * - **Type safety**: Compile-time type checking via C macros
 * - **Memory safety**: Automatic NULL handling and zero-initialization
 *
 * @section usage Usage Example
 * @code
 * #include "cupolas_utils.h"
 *
 * int my_function(my_config_t* config) {
 *     CUPOLAS_CHECK_NULL(config);
 *
 *     CUPOLAS_MUTEX_TYPE lock;
 *     CUPOLAS_MUTEX_INIT(&lock);
 *
 *     my_data_t* data = CUPOLAS_ALLOC_STRUCT(my_data_t);
 *     if (!data) return -1;
 *
 *     CUPOLAS_MUTEX_LOCK(&lock);
 *     // Critical section...
 *     CUPOLAS_MUTEX_UNLOCK(&lock);
 *
 *     CUPOLAS_FREE(data);
 *     CUPOLAS_MUTEX_DESTROY(&lock);
 *     return 0;
 * }
 * @endcode
 *
 * @see cupolas_utils.c for implementation details
 * @see ../security/README.md for security module usage
 */

#ifndef CUPOLAS_UTILS_H
#define CUPOLAS_UTILS_H

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Platform Abstraction Layer - Unified Thread Synchronization Primitives
 * ============================================================================
 *
 * These macros provide a unified interface for mutex operations across
 * different platforms, eliminating the need for #ifdef blocks in business logic.
 *
 * @note On Windows, maps to CRITICAL_SECTION API
 * @note On POSIX systems, maps to pthread_mutex_t API
 * @warning Not recursive: calling LOCK twice on same thread will deadlock
 *
 * @threadsafe All operations are thread-safe when used correctly
 */

#ifdef _WIN32
#include <windows.h>

/**
 * @brief Platform-specific mutex type (Windows)
 */
#define CUPOLAS_MUTEX_TYPE CRITICAL_SECTION

/**
 * @brief Initialize a mutex (Windows version)
 * @param m Pointer to CUPOLAS_MUTEX_TYPE variable
 * @pre m must point to valid memory
 * @post Mutex is initialized and ready for use
 */
#define CUPOLAS_MUTEX_INIT(m) InitializeCriticalSection(m)

/**
 * @brief Acquire mutex lock (blocking)
 * @param m Pointer to initialized mutex
 * @note Blocks until lock is acquired
 * @threadsafe Safe to call from multiple threads
 */
#define CUPOLAS_MUTEX_LOCK(m) EnterCriticalSection(m)

/**
 * @brief Release mutex lock
 * @param m Pointer to locked mutex
 * @pre Mutex must be locked by current thread
 * @post Mutex is available for other threads
 */
#define CUPOLAS_MUTEX_UNLOCK(m) LeaveCriticalSection(m)

/**
 * @brief Destroy mutex and free resources
 * @param m Pointer to initialized mutex
 * @post Mutex cannot be used after this call
 */
#define CUPOLAS_MUTEX_DESTROY(m) DeleteCriticalSection(m)

#else
#include <pthread.h>

/**
 * @brief Platform-specific mutex type (POSIX)
 */
#define CUPOLAS_MUTEX_TYPE pthread_mutex_t

/**
 * @brief Initialize a mutex (POSIX version)
 * @param m Pointer to CUPOLAS_MUTEX_TYPE variable
 */
#define CUPOLAS_MUTEX_INIT(m) pthread_mutex_init(m, NULL)

/**
 * @brief Acquire mutex lock (POSIX version)
 * @param m Pointer to initialized mutex
 */
#define CUPOLAS_MUTEX_LOCK(m) pthread_mutex_lock(m)

/**
 * @brief Release mutex lock (POSIX version)
 * @param m Pointer to locked mutex
 */
#define CUPOLAS_MUTEX_UNLOCK(m) pthread_mutex_unlock(m)

/**
 * @brief Destroy mutex (POSIX version)
 * @param m Pointer to initialized mutex
 */
#define CUPOLAS_MUTEX_DESTROY(m) pthread_mutex_destroy(m)

#endif /* _WIN32 */

/* ============================================================================
 * Memory Management Macros - Unified Allocation and Deallocation
 * ============================================================================
 *
 * Safe memory management wrappers that provide:
 * - Automatic zero-initialization (calloc-based)
 * - NULL-safe deallocation with pointer clearing
 * - Type-safe allocation with sizeof() automation
 *
 * @warning Always use these macros instead of raw malloc/calloc/free
 * @ownership All allocated memory must be freed with corresponding FREE macro
 */

/**
 * @brief Allocate zero-initialized array of given type
 * @param type Data type to allocate
 * @param count Number of elements
 * @return Pointer to zero-initialized memory, or NULL on failure
 * @note Uses calloc internally for security (zero-init prevents info leaks)
 *
 * @code
 * int* numbers = CUPOLAS_ALLOC(int, 100); // 100 ints, all zeroed
 * @endcode
 */
#define CUPOLAS_ALLOC(type, count) \
    ((type*)calloc(count, sizeof(type)))

/**
 * @brief Allocate single zero-initialized structure
 * @param type Structure type name
 * @return Pointer to zero-initialized struct, or NULL on failure
 *
 * @code
 * my_config_t* cfg = CUPOLAS_ALLOC_STRUCT(my_config_t);
 * @endcode
 */
#define CUPOLAS_ALLOC_STRUCT(type) \
    ((type*)calloc(1, sizeof(type)))

/**
 * @brief Allocate uninitialized array (faster than CUPOLAS_ALLOC)
 * @param type Data type
 * @param count Number of elements
 * @return Pointer to uninitialized memory, or NULL on failure
 * @warning Memory contents are undefined; initialize before use
 *
 * @code
 * char* buffer = CUPOLAS_ALLOC_ARRAY(char, 4096);
 * memset(buffer, 0, 4096); // Manual init if needed
 * @endcode
 */
#define CUPOLAS_ALLOC_ARRAY(type, count) \
    ((type*)malloc((count) * sizeof(type)))

/**
 * @brief Reallocate memory block with type safety
 * @param ptr Existing pointer (can be NULL)
 * @param type Data type
 * @param count New element count
 * @return Pointer to reallocated memory, or NULL on failure
 * @note Original pointer is invalid after call; use returned value
 *
 * @code
 * arr = CUPOLAS_REALLOC(arr, int, 200); // Expand to 200 ints
 * @endcode
 */
#define CUPOLAS_REALLOC(ptr, type, count) \
    ((type*)realloc(ptr, (count) * sizeof(type)))

/**
 * @brief Free memory and set pointer to NULL (dangling pointer prevention)
 * @param ptr Pointer variable to free
 * @post ptr is set to NULL to prevent use-after-free
 *
 * @code
 * char* data = malloc(1024);
 * CUPOLAS_FREE(data); // data == NULL now
 * // Using data here would be obvious bug (NULL dereference)
 * @endcode
 */
#define CUPOLAS_FREE(ptr) \
    do { free(ptr); ptr = NULL; } while(0)

/**
 * @brief Free memory with explicit NULL check
 * @param ptr Pointer variable (may already be NULL)
 * @post ptr is set to NULL
 * @sa CUPOLAS_FREE
 */
#define CUPOLAS_FREE_ARRAY(ptr) \
    do { if(ptr) { free(ptr); ptr = NULL; } } while(0)

/* ============================================================================
 * Error Handling Macros - Unified Error Checking and Early Return
 * ============================================================================
 *
 * Reduce boilerplate code for common error checking patterns.
 * All macros perform early return on failure condition.
 *
 * @note Return value conventions:
 *   - Default: returns -1 on error
 *   - _RET variants: allow custom return value
 */

/**
 * @brief Check for NULL pointer, return -1 if true
 * @param ptr Pointer to check
 * @return -1 if ptr is NULL, otherwise continues execution
 *
 * @code
 * int process(config_t* cfg) {
 *     CUPOLAS_CHECK_NULL(cfg);
 *     // cfg is guaranteed non-NULL here
 * }
 * @endcode
 */
#define CUPOLAS_CHECK_NULL(ptr) \
    do { if ((ptr) == NULL) return -1; } while(0)

/**
 * @brief Check for NULL pointer with custom return value
 * @param ptr Pointer to check
 * @param ret Value to return if NULL
 * @return ret if ptr is NULL
 */
#define CUPOLAS_CHECK_NULL_RET(ptr, ret) \
    do { if ((ptr) == NULL) return (ret); } while(0)

/**
 * @brief Check expression result, return -1 if nonzero (error)
 * @param expr Expression to evaluate (should return 0 on success)
 * @return -1 if expr != 0
 *
 * @code
 * CUPOLAS_CHECK_RESULT(initialize());
 * @endcode
 */
#define CUPOLAS_CHECK_RESULT(expr) \
    do { if ((expr) != 0) return -1; } while(0)

/**
 * @brief Check result with custom error return value
 * @param expr Expression to evaluate
 * @param ret Value to return on error
 */
#define CUPOLAS_CHECK_RESULT_RET(expr, ret) \
    do { if ((expr) != 0) return (ret); } while(0)

/**
 * @brief Check condition is true, return -1 if false
 * @param cond Boolean condition to test
 * @return -1 if condition is false
 */
#define CUPOLAS_CHECK_TRUE(cond) \
    do { if (!(cond)) return -1; } while(0)

/**
 * @brief Check condition with custom return value
 * @param cond Boolean condition
 * @ret Value to return if false
 */
#define CUPOLAS_CHECK_TRUE_RET(cond, ret) \
    do { if (!(cond)) return (ret); } while(0)

/* ============================================================================
 * String and Utility Macros
 * ============================================================================
 *
 * General-purpose preprocessor utilities for string manipulation,
 * array operations, and math functions.
 */

/** @brief Convert token to string literal */
#define CUPOLAS_STRINGIFY(x) #x

/** @brief Two-level stringify for macro expansion */
#define CUPOLAS_TOSTRING(x) CUPOLAS_STRINGIFY(x)

/** @brief Concatenate two tokens */
#define CUPOLAS_CONCAT(a, b) a##b

/** @brief Concatenate three tokens */
#define CUPOLAS_CONCAT3(a, b, c) a##b##c

/**
 * @brief Calculate number of elements in static array
 * @param arr Array variable (not pointer!)
 * @return Number of elements
 * @warning Undefined behavior if arr is a pointer, not array
 *
 * @code
 * int values[10];
 * size_t n = CUPOLAS_ARRAY_SIZE(values); // n == 10
 * @endcode
 */
#define CUPOLAS_ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

/** @brief Return minimum of two values */
#define CUPOLAS_MIN(a, b) ((a) < (b) ? (a) : (b))

/** @brief Return maximum of two values */
#define CUPOLAS_MAX(a, b) ((a) > (b) ? (a) : (b))

/** @brief Clamp value to range [lo, hi] */
#define CUPOLAS_CLAMP(x, lo, hi) CUPOLAS_MIN(CUPOLAS_MAX(x, lo), hi)

/** @brief Absolute value */
#define CUPOLAS_ABS(x) ((x) < 0 ? -(x) : (x))

/* ============================================================================
 * Logging Macros - Conditional Debug Output
 * ============================================================================
 *
 * Level-based logging with compile-time toggle.
 * Define CUPOLAS_ENABLE_LOGGING to activate.
 *
 * In release builds (NDEBUG defined), all logging is disabled
 * automatically to avoid performance overhead.
 */

#ifdef CUPOLAS_ENABLE_LOGGING

/**
 * @brief Log informational message
 * @param fmt printf-style format string
 * @param ... Format arguments
 */
#define CUPOLAS_LOG(fmt, ...) \
    fprintf(stderr, "[CUPOLAS] " fmt "\n", ##__VA_ARGS__)

/**
 * @brief Log error message
 * @param fmt printf-style format string
 * @param ... Format arguments
 */
#define CUPOLAS_LOG_ERROR(fmt, ...) \
    fprintf(stderr, "[CUPOLAS ERROR] " fmt "\n", ##__VA_ARGS__)

/**
 * @brief Log debug message (stripped in release builds)
 * @param fmt printf-style format string
 * @param ... Format arguments
 * @note Only emitted when debugging is enabled
 */
#define CUPOLAS_LOG_DEBUG(fmt, ...) \
    fprintf(stderr, "[CUPOLAS DEBUG] " fmt "\n", ##__VA_ARGS__)

#else

/** @brief Disabled logging stub (compiles to nothing) */
#define CUPOLAS_LOG(fmt, ...) ((void)0)

/** @brief Disabled error logging stub */
#define CUPOLAS_LOG_ERROR(fmt, ...) ((void)0)

/** @brief Disabled debug logging stub */
#define CUPOLAS_LOG_DEBUG(fmt, ...) ((void)0)

#endif /* CUPOLAS_ENABLE_LOGGING */

/* ============================================================================
 * Compiler Hints - Performance Optimization Annotations
 * ============================================================================
 *
 * Branch prediction hints and inline control for hot-path optimization.
 * Use LIKELY/UNLIKELY in performance-critical code paths.
 */

#if defined(__GNUC__) || defined(__clang__)
/** @brief Force inline expansion (GCC/Clang) */
#define CUPOLAS_INLINE __inline__ __attribute__((always_inline))

/** @brief Prevent inlining (for debugging/profiling) */
#define CUPOLAS_NOINLINE __attribute__((noinline))

/** @brief Hint that condition is usually true */
#define CUPOLAS_LIKELY(x) __builtin_expect(!!(x), 1)

/** @brief Hint that condition is usually false (error path) */
#define CUPOLAS_UNLIKELY(x) __builtin_expect(!!(x), 0)

#elif defined(_MSC_VER)
/** @brief Force inline expansion (MSVC) */
#define CUPOLAS_INLINE __forceinline

/** @brief Prevent inlining (MSVC) */
#define CUPOLAS_NOINLINE __declspec(noinline)

/** @brief No-op branch hint (MSVC ignores this) */
#define CUPOLAS_LIKELY(x) (x)

/** @brief No-op branch hint (MSVC ignores this) */
#define CUPOLAS_UNLIKELY(x) (x)

#else
/** @brief Fallback inline keyword */
#define CUPOLAS_INLINE inline

/** @brief Fallback noinline (empty) */
#define CUPOLAS_NOINLINE

/** @brief Fallback branch hints (identity) */
#define CUPOLAS_LIKELY(x) (x)
#define CUPOLAS_UNLIKELY(x) (x)

#endif /* Compiler detection */

/* ============================================================================
 * Alignment and Padding Utilities
 * ============================================================================
 *
 * Memory alignment helpers for cache-line alignment and structure padding.
 */

/**
 * @brief Round up to nearest alignment boundary
 * @param value Input value
 * @param align Alignment (must be power of 2)
 * @return Aligned value >= input
 */
#define CUPOLAS_ALIGN_UP(x, align) (((x) + (align) - 1) & ~((align) - 1))

/**
 * @brief Round down to nearest alignment boundary
 * @param value Input value
 * @param align Alignment (must be power of 2)
 * @return Aligned value <= input
 */
#define CUPOLAS_ALIGN_DOWN(x, align) ((x) & ~((align) - 1))

/**
 * @brief Check if value is aligned to boundary
 * @param value Input value
 * @param align Alignment (must be power of 2)
 * @return Non-zero if aligned, 0 if not
 */
#define CUPOLAS_IS_ALIGNED(x, align) (!((x) & ((align) - 1)))

/* ============================================================================
 * Bit Manipulation Utilities
 * ============================================================================
 *
 * Portable bit operations for flag management and bitfield access.
 */

/** @brief Create bitmask with bit n set (0-indexed) */
#define CUPOLAS_BIT(n) (1U << (n))

/** @brief Set bit n in value x */
#define CUPOLAS_BIT_SET(x, n) ((x) |= CUPOLAS_BIT(n))

/** @brief Clear bit n in value x */
#define CUPOLAS_BIT_CLEAR(x, n) ((x) &= ~CUPOLAS_BIT(n))

/** @brief Test if bit n is set (returns 0 or 1) */
#define CUPOLAS_BIT_TEST(x, n) (!!((x) & CUPOLAS_BIT(n)))

/** @brief Toggle bit n in value x */
#define CUPOLAS_BIT_FLIP(x, n) ((x) ^= CUPOLAS_BIT(n))

/* ============================================================================
 * Time Utilities - Cross-Platform Sleep
 * ============================================================================
 *
 * Portable sleep function with millisecond precision.
 */

#ifdef _WIN32
#include <windows.h>

/**
 * @brief Sleep for specified milliseconds (Windows)
 * @param ms Milliseconds to sleep (uint32_t range)
 */
#define CUPOLAS_SLEEP_MS(ms) Sleep(ms)

#else
#include <unistd.h>

/**
 * @brief Sleep for specified milliseconds (POSIX)
 * @param ms Milliseconds to sleep
 * @note Uses usleep internally; may have microsecond precision
 */
#define CUPOLAS_SLEEP_MS(ms) usleep((ms) * 1000)

#endif /* _WIN32 */

/* ============================================================================
 * Compile-Time Assertions
 * ============================================================================
 *
 * Static assertions that fail at compile time with descriptive messages.
 * Use for invariant checking and API contracts.
 */

/**
 * @brief Compile-time assertion with custom message
 * @param cond Condition that must be true
 * @param msg Error message shown on failure
 * @compile_error If cond is false: "negative size for array 'static_assert_LINE'"
 *
 * @code
 * CUPOLAS_STATIC_ASSERT(sizeof(int) == 4, "int must be 32-bit");
 * @endcode
 */
#define CUPOLAS_STATIC_ASSERT(cond, msg) \
    typedef char CUPOLAS_CONCAT(static_assert_, __LINE__)[(cond) ? 1 : -1]

/* ============================================================================
 * Deprecation Markers
 * ============================================================================
 *
 * Mark deprecated APIs with compiler warnings to guide migration.
 */

#if defined(__GNUC__) || defined(__clang__)
/**
 * @brief Mark function/macro as deprecated (GCC/Clang)
 * @param msg Deprecation warning message
 * @compile_warning "msg is deprecated"
 */
#define CUPOLAS_DEPRECATED(msg) __attribute__((deprecated(msg)))

#elif defined(_MSC_VER)
/**
 * @brief Mark as deprecated (MSVC)
 * @param msg Deprecation message
 */
#define CUPOLAS_DEPRECATED(msg) __declspec(deprecated(msg))

#else
/** @brief Fallback deprecation marker (no warning) */
#define CUPOLAS_DEPRECATED(msg)

#endif /* Compiler detection */

/* ============================================================================
 * Utility Function Declarations
 * ============================================================================
 *
 * Implemented in cupolas_utils.c
 * These provide runtime functionality beyond what macros can offer.
 */

/**
 * @brief Safely duplicate a string (NULL-safe)
 * @param str Source string (may be NULL)
 * @return Newly allocated copy, or NULL if str is NULL or allocation fails
 * @ownership Caller must free result with CUPOLAS_FREE()
 *
 * @code
 * char* copy = cupolas_strdup(original);
 * if (copy) {
 *     // Use copy...
 *     CUPOLAS_FREE(copy);
 * }
 * @endcode
 */
char* cupolas_strdup(const char* str);

/**
 * @brief Safe string copy with size limit (strlcpy implementation)
 * @param dest Destination buffer
 * @param src Source string
 * @param len Size of destination buffer (including null terminator)
 * @return Total length of src (excluding null), regardless of truncation
 * @note Always null-terminates dest, even if truncated
 * @sa strlcpy BSD function
 */
size_t cupolas_strlcpy(char* dest, const char* src, size_t len);

/**
 * @brief Secure memset that won't be optimized away
 * @param ptr Memory to fill
 * @param len Number of bytes to write
 * @note Uses volatile or compiler-specific barriers to prevent dead-store elimination
 * @security Use for erasing sensitive data (passwords, keys)
 */
void cupolas_memset_s(void* ptr, size_t len);

/**
 * @brief Get current timestamp in milliseconds
 * @return Unix epoch time in milliseconds (uint64_t)
 * @note Monotonic clock preferred when available
 */
uint64_t cupolas_get_timestamp_ms(void);

/**
 * @brief Compute djb2 hash of string
 * @param str Null-terminated string to hash
 * @return 32-bit hash value
 * @note Good distribution for hashtable keys; not cryptographically secure
 */
uint32_t cupolas_hash_string(const char* str);

/**
 * @brief Unified logging function with level control
 * @param level Log level ("INFO", "ERROR", "DEBUG", etc.)
 * @param fmt printf-style format string
 * @param ... Variable arguments
 * @note Thread-safe when used with CUPOLAS_ENABLE_LOGGING
 */
void cupolas_log_message(const char* level, const char* fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* CUPOLAS_UTILS_H */
