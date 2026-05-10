// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: Apache-2.0
/**
 * @file netdb.h
 * @brief Windows compatibility shim for POSIX <netdb.h>
 *
 * Provides minimal netdb API using Winsock2.
 * Note: winsock2.h must be included before this header.
 */

#ifndef AGENTOS_COMPAT_NETDB_H
#define AGENTOS_COMPAT_NETDB_H

#ifdef __cplusplus
extern "C" {
#endif

#include <winsock2.h>
#include <ws2tcpip.h>

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_COMPAT_NETDB_H */