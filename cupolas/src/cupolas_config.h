/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * cupolas_config.h - Configuration Hot-Reload: Runtime Configuration Manager
 *
 * Design Principles:
 * - Hot-Reload: Update config without restart
 * - Atomic Switch: Safe release of old config when new takes effect
 * - Validation First: Validate before applying changes
 * - Rollback Support: Auto-rollback on validation failure
 *
 * Supported Configuration Types:
 * - Permission Rules (permission rules)
 * - Sanitizer Rules (sanitizer rules)
 * - Resource Limits (resource limits)
 * - Log Levels (log levels)
 * - Audit Policy (audit policy)
 */

#ifndef cupolas_CONFIG_H
#define cupolas_CONFIG_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Configuration Type */
typedef enum config_type {
    CONFIG_TYPE_PERMISSION_RULES = 0,
    CONFIG_TYPE_SANITIZER_RULES,
    CONFIG_TYPE_RESOURCE_LIMITS,
    CONFIG_TYPE_LOG_LEVEL,
    CONFIG_TYPE_AUDIT_POLICY,
    CONFIG_TYPE_ALL
} config_type_t;

/* Configuration Status */
typedef enum config_status {
    CONFIG_STATUS_OK = 0,
    CONFIG_STATUS_LOADING,
    CONFIG_STATUS_VALIDATING,
    CONFIG_STATUS_APPLIED,
    CONFIG_STATUS_ROLLBACK,
    CONFIG_STATUS_ERROR
} config_status_t;

/* Configuration Event */
typedef enum config_event {
    CONFIG_EVENT_LOADED = 0,
    CONFIG_EVENT_APPLIED,
    CONFIG_EVENT_ROLLBACK,
    CONFIG_EVENT_ERROR
} config_event_t;

/* Configuration Version Info */
typedef struct config_version {
    uint32_t major;
    uint32_t minor;
    uint32_t patch;
    uint64_t timestamp_ns;
    const char* commit_hash;
} config_version_t;

/* Configuration Change Descriptor */
typedef struct config_change {
    config_type_t type;
    config_status_t status;
    config_version_t old_version;
    config_version_t new_version;
    const char* file_path;
    const char* error_message;
} config_change_t;

/**
 * @brief Configuration observer callback
 * @param[in] event Configuration event type
 * @param[in] change Configuration change descriptor (caller retains ownership)
 * @param[in] user_data User-provided data (caller retains ownership)
 * @note Thread-safe: Called from internal lock context
 * @reentrant Yes
 */
typedef void (*config_observer_t)(config_event_t event, const config_change_t* change, void* user_data);

/* Configuration Validation Result */
typedef struct config_validation_result {
    bool valid;
    const char** errors;
    size_t error_count;
    const char** warnings;
    size_t warning_count;
} config_validation_result_t;

/* Configuration Handle */
typedef struct cupolas_config cupolas_config_t;

/**
 * @brief Create configuration manager
 * @param[in] config_dir Configuration directory (NULL for default)
 * @return Configuration manager handle, NULL on failure
 * @post On success, caller owns the returned handle
 * @note Thread-safe: Yes
 * @reentrant No (create/destroy must be paired)
 * @ownership Returned handle: caller owns, must call cupolas_config_destroy
 */
cupolas_config_t* cupolas_config_create(const char* config_dir);

/**
 * @brief Destroy configuration manager
 * @param[in] cfg Configuration manager handle (must not be NULL)
 * @pre Handle was created by cupolas_config_create
 * @post All resources are released
 * @note Thread-safe: No, must not be called while other threads access cfg
 * @reentrant No
 * @ownership cfg: caller transfers ownership
 */
void cupolas_config_destroy(cupolas_config_t* cfg);

/**
 * @brief Load configuration file
 * @param[in] cfg Configuration manager handle (must not be NULL)
 * @param[in] type Configuration type
 * @param[in] file_path File path (NULL for default path)
 * @return 0 on success, negative on failure
 * @note Thread-safe: Yes
 * @reentrant Yes
 * @ownership file_path: caller retains ownership, may be NULL
 */
int cupolas_config_load(cupolas_config_t* cfg, config_type_t type, const char* file_path);

/**
 * @brief Reload configuration file
 * @param[in] cfg Configuration manager handle (must not be NULL)
 * @param[in] type Configuration type
 * @return 0 on success, negative on failure
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
int cupolas_config_reload(cupolas_config_t* cfg, config_type_t type);

/**
 * @brief Validate configuration
 * @param[in] cfg Configuration manager handle (must not be NULL)
 * @param[in] type Configuration type
 * @param[out] result Validation result output (must not be NULL)
 * @return 0 if validation passes, negative on failure
 * @note Thread-safe: Yes
 * @reentrant Yes
 * @ownership result: callee writes, caller owns
 */
int cupolas_config_validate(cupolas_config_t* cfg, config_type_t type,
                        config_validation_result_t* result);

/**
 * @brief Apply configuration change
 * @param[in] cfg Configuration manager handle (must not be NULL)
 * @param[in] type Configuration type
 * @return 0 on success, negative on failure
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
int cupolas_config_apply(cupolas_config_t* cfg, config_type_t type);

/**
 * @brief Rollback configuration change
 * @param[in] cfg Configuration manager handle (must not be NULL)
 * @param[in] type Configuration type
 * @return 0 on success, negative on failure
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
int cupolas_config_rollback(cupolas_config_t* cfg, config_type_t type);

/**
 * @brief Get configuration version
 * @param[in] cfg Configuration manager handle (must not be NULL)
 * @param[in] type Configuration type
 * @param[out] version Version output (must not be NULL)
 * @return 0 on success, negative on failure
 * @note Thread-safe: Yes
 * @reentrant Yes
 * @ownership version: callee writes, caller owns
 */
int cupolas_config_get_version(cupolas_config_t* cfg, config_type_t type,
                            config_version_t* version);

/**
 * @brief Register configuration observer
 * @param[in] cfg Configuration manager handle (must not be NULL)
 * @param[in] type Configuration type to watch (CONFIG_TYPE_ALL for all)
 * @param[in] callback Observer callback (must not be NULL)
 * @param[in] user_data User data passed to callback (caller retains ownership)
 * @return Observer ID on success, -1 on failure
 * @note Thread-safe: Yes
 * @reentrant Yes
 * @ownership callback, user_data: caller retains ownership
 */
int cupolas_config_watch(cupolas_config_t* cfg, config_type_t type,
                      config_observer_t callback, void* user_data);

/**
 * @brief Unregister configuration observer
 * @param[in] cfg Configuration manager handle (must not be NULL)
 * @param[in] watcher_id Observer ID returned by cupolas_config_watch
 * @return 0 on success, negative on failure
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
int cupolas_config_unwatch(cupolas_config_t* cfg, int watcher_id);

/**
 * @brief Get configuration status
 * @param[in] cfg Configuration manager handle (must not be NULL)
 * @param[in] type Configuration type
 * @return Configuration status
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
config_status_t cupolas_config_get_status(cupolas_config_t* cfg, config_type_t type);

/**
 * @brief Set auto-reload interval
 * @param[in] cfg Configuration manager handle (must not be NULL)
 * @param[in] type Configuration type (CONFIG_TYPE_ALL for all)
 * @param[in] interval_ms Reload interval in milliseconds (0 to disable)
 * @return 0 on success, negative on failure
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
int cupolas_config_set_auto_reload(cupolas_config_t* cfg, config_type_t type,
                                uint32_t interval_ms);

/**
 * @brief Trigger configuration reload check
 * @param[in] cfg Configuration manager handle (must not be NULL)
 * @param[in] type Configuration type (CONFIG_TYPE_ALL for all)
 * @return Number of configuration types that changed
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
int cupolas_config_check_reload(cupolas_config_t* cfg, config_type_t type);

/**
 * @brief Get last error message
 * @param[in] cfg Configuration manager handle (must not be NULL)
 * @return Error message string (static, do not free), NULL if no error
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
const char* cupolas_config_get_last_error(cupolas_config_t* cfg);

/**
 * @brief Get configuration directory
 * @param[in] cfg Configuration manager handle (must not be NULL)
 * @return Configuration directory path (static, do not free)
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
const char* cupolas_config_get_config_dir(cupolas_config_t* cfg);

/**
 * @brief Export current configuration as JSON
 * @param[in] cfg Configuration manager handle (must not be NULL)
 * @param[in] type Configuration type
 * @param[out] buffer Output buffer (must not be NULL)
 * @param[in] size Buffer size in bytes
 * @return Bytes written on success, 0 on failure
 * @note Thread-safe: Yes
 * @reentrant Yes
 * @ownership buffer: caller owns
 */
size_t cupolas_config_export_json(cupolas_config_t* cfg, config_type_t type,
                               char* buffer, size_t size);

/**
 * @brief Export current configuration as YAML
 * @param[in] cfg Configuration manager handle (must not be NULL)
 * @param[in] type Configuration type
 * @param[out] buffer Output buffer (must not be NULL)
 * @param[in] size Buffer size in bytes
 * @return Bytes written on success, 0 on failure
 * @note Thread-safe: Yes
 * @reentrant Yes
 * @ownership buffer: caller owns
 */
size_t cupolas_config_export_yaml(cupolas_config_t* cfg, config_type_t type,
                               char* buffer, size_t size);

/* ============================================================================
 * Convenience Functions
 * ============================================================================ */

/**
 * @brief Reload all configurations
 * @param[in] cfg Configuration manager handle (must not be NULL)
 * @return Number of configuration types that changed
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
int cupolas_config_reload_all(cupolas_config_t* cfg);

/**
 * @brief Validate all configurations
 * @param[in] cfg Configuration manager handle (must not be NULL)
 * @return true if all configurations valid, false otherwise
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
bool cupolas_config_validate_all(cupolas_config_t* cfg);

/**
 * @brief Get configuration status string
 * @param[in] status Configuration status
 * @return Status string (static, do not free)
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
const char* cupolas_config_status_string(config_status_t status);

/**
 * @brief Get configuration type string
 * @param[in] type Configuration type
 * @return Type string (static, do not free)
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
const char* cupolas_config_type_string(config_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* cupolas_CONFIG_H */
