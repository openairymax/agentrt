/**
 * @file stdatomic.h
 * @brief Redirect <stdatomic.h> for MSVC compatibility.
 *
 * On GCC/Clang with C11 support: include the system <stdatomic.h> directly.
 * On MSVC (which lacks <stdatomic.h>): redirect to atomic_compat.h.
 *
 * This file is placed in corekern include dir to provide MSVC compatibility,
 * but must NOT shadow the system <stdatomic.h> on GCC/Clang platforms.
 */
#ifndef AGENTOS_COREKERN_STDATOMIC_SHIM
#define AGENTOS_COREKERN_STDATOMIC_SHIM

#if defined(_MSC_VER)
#include "atomic_compat.h"
#else
#include_next <stdatomic.h>
#endif

#endif
