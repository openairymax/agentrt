/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * cupolas_vault_cred_type.h - Canonical Credential Type Definition
 *
 * Single source of truth for cupolas_vault_cred_type_t across all modules.
 */

#ifndef CUPOLAS_VAULT_CRED_TYPE_H
#define CUPOLAS_VAULT_CRED_TYPE_H

#include <stdint.h>

/**
 * @brief Credential type classification for vault storage
 */
typedef enum {
    CUPOLAS_VAULT_CRED_PASSWORD = 1,     /**< Password */
    CUPOLAS_VAULT_CRED_TOKEN = 2,        /**< Token (API Key, OAuth Token) */
    CUPOLAS_VAULT_CRED_KEY = 3,          /**< Key (private key) */
    CUPOLAS_VAULT_CRED_CERTIFICATE = 4,  /**< Certificate */
    CUPOLAS_VAULT_CRED_SECRET = 5,       /**< Generic secret */
    CUPOLAS_VAULT_CRED_NOTE = 6          /**< Secure note */
} cupolas_vault_cred_type_t;

#endif /* CUPOLAS_VAULT_CRED_TYPE_H */
