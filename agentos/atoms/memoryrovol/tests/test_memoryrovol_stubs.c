#include "layer4_pattern.h"
#include "memoryrovol.h"
#include "error.h"

agentos_error_t agentos_rule_generator_from_cluster(
    agentos_rule_generator_t* gen,
    const char** cluster_ids,
    size_t count,
    char** out_rule)
{
    (void)gen; (void)cluster_ids; (void)count;
    if (out_rule) *out_rule = NULL;
    return AGENTOS_ENOTSUP;
}
