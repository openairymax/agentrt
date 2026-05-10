/**
 * @file stdatomic.h
 * @brief Redirect <stdatomic.h> to atomic_compat.h for MSVC compatibility.
 *        Placed in corekern include dir to shadow system <stdatomic.h>.
 */
#ifndef AGENTOS_COREKERN_STDATOMIC_SHIM
#define AGENTOS_COREKERN_STDATOMIC_SHIM
#include "atomic_compat.h"
#endif