/**
 * @file stdatomic.h
 * @brief Redirect <stdatomic.h> for cross-platform compatibility.
 *
 * On MSVC: delegate entirely to atomic_compat.h.
 * On GCC/Clang: include system stdatomic.h first, then atomic_compat.h
 * for compat functions (atomic_load_32 etc.) that system headers lack.
 */
#ifndef AGENTOS_COREKERN_STDATOMIC_SHIM
#define AGENTOS_COREKERN_STDATOMIC_SHIM

#if defined(_MSC_VER)
#include "atomic_compat.h"
#else
#include_next <stdatomic.h>
#include "atomic_compat.h"
#endif

#endif
