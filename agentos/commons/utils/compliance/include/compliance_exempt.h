#ifndef AGENTOS_COMPLIANCE_EXEMPT_H
#define AGENTOS_COMPLIANCE_EXEMPT_H

#ifdef AGENTOS_COMPLIANCE_STRICT

#define AGENTOS_COMPLIANCE_EXEMPT_BEGIN
#define AGENTOS_COMPLIANCE_EXEMPT_END

#else

#define AGENTOS_COMPLIANCE_EXEMPT_BEGIN                                                            \
    _Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"") \
        _Pragma("GCC diagnostic ignored \"-Wpoison-system-directories\"")

#define AGENTOS_COMPLIANCE_EXEMPT_END _Pragma("GCC diagnostic pop")

#endif

#endif
