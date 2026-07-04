// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
/**
 * @file netdb.h
 * @brief Windows compatibility shim for POSIX <netdb.h>
 *
 * Provides minimal netdb API using Winsock2 on Windows.
 * On non-Windows platforms, this header simply includes the system <netdb.h>.
 */

#ifndef AGENTOS_COMPAT_NETDB_H
#define AGENTOS_COMPAT_NETDB_H

#pragma GCC system_header

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32

#include <winsock2.h>
#include <ws2tcpip.h>

#else

#include_next <netdb.h>

#endif /* _WIN32 */

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_COMPAT_NETDB_H */
