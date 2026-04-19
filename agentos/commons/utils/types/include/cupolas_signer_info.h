/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * cupolas_signer_info.h - Canonical Signer Information Type Definition
 *
 * Single source of truth for cupolas_signer_info_t across all modules.
 * All modules MUST include this file instead of defining it locally.
 *
 * Design Principles (per Engineering Standards Manual v13.0):
 * - TYPE-002 fix: Eliminates redefinition conflict between
 *   cupolas/cupolas_signature.h (full struct) and
 *   daemon_security.h (stub struct { char name[128]; })
 * - DRY Principle: One definition, used everywhere
 * - SEC-017 compliant: Production-grade definition always available
 */

#ifndef CUPOLAS_SIGNER_INFO_H
#define CUPOLAS_SIGNER_INFO_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Code signer identity information (full production definition)
 *
 * Contains complete X.509 certificate identity fields for
 * signature verification in the cupolas security framework.
 */
typedef struct {
    char* subject_cn;               /**< Common name (heap allocated) */
    char* subject_org;              /**< Organization (heap allocated) */
    char* subject_ou;               /**< Organizational unit (heap allocated) */
    char* issuer_cn;                /**< Issuer CN (heap allocated) */
    char* serial_number;            /**< Serial number (heap allocated) */
    uint64_t not_before;            /**< Validity start (Unix epoch) */
    uint64_t not_after;             /**< Validity end (Unix epoch) */
    bool is_ca;                     /**< Is CA certificate */
    uint32_t key_usage;             /**< Key usage flags (RFC 5280) */
} cupolas_signer_info_t;

#endif /* CUPOLAS_SIGNER_INFO_H */
