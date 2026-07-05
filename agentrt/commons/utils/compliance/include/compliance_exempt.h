#ifndef AGENTRT_COMPLIANCE_EXEMPT_H
#define AGENTRT_COMPLIANCE_EXEMPT_H

#ifdef AGENTRT_COMPLIANCE_STRICT

#define AGENTRT_COMPLIANCE_EXEMPT_BEGIN
#define AGENTRT_COMPLIANCE_EXEMPT_END

#else

#define AGENTRT_COMPLIANCE_EXEMPT_BEGIN                                                                   \
    _Pragma("GCC diagnostic push")                                                                        \
    _Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"")

#define AGENTRT_COMPLIANCE_EXEMPT_END \
    _Pragma("GCC diagnostic pop")

#endif

#endif