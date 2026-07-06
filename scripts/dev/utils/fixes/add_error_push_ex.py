#!/usr/bin/env python3
"""Add error_push_ex context propagation to protocol/gateway files missing it."""

import re
import os

# Error code to human-readable description mapping
error_descriptions = {
    'AGENTRT_ERR_NULL_POINTER': 'null pointer',
    'AGENTRT_ERR_INVALID_PARAM': 'invalid parameter',
    'AGENTRT_ERR_OUT_OF_MEMORY': 'out of memory',
    'AGENTRT_ERR_OVERFLOW': 'overflow',
    'AGENTRT_ERR_NOT_FOUND': 'not found',
    'AGENTRT_ERR_ALREADY_EXISTS': 'already exists',
    'AGENTRT_ERR_TIMEOUT': 'timeout',
    'AGENTRT_ERR_NOT_SUPPORTED': 'not supported',
    'AGENTRT_ERR_SYS_NOT_INIT': 'not initialized',
    'AGENTRT_ERR_UNKNOWN': 'unknown error',
    'AGENTRT_ERR_PERMISSION_DENIED': 'permission denied',
    'AGENTRT_ERR_IO': 'I/O error',
    'AGENTRT_ERR_ESANITIZE': 'sanitization error',
    'AGENTRT_ERR_BUSY': 'busy',
    'AGENTRT_ERR_CANCELED': 'canceled',
    'AGENTRT_ERR_ESECURITY': 'security error',
    'AGENTRT_ERR_SYS_FILE': 'file error',
    'AGENTRT_ERR_SYS_RESOURCE': 'resource error',
    'AGENTRT_ERR_WOULD_BLOCK': 'would block',
    'AGENTRT_ERR_INTERRUPTED': 'interrupted',
    'CUPOLAS_ERR_NULL_POINTER': 'null pointer',
    'CUPOLAS_ERR_INVALID_PARAM': 'invalid parameter',
    'CUPOLAS_ERR_OUT_OF_MEMORY': 'out of memory',
    'CUPOLAS_ERR_NOT_FOUND': 'not found',
    'CUPOLAS_ERR_UNKNOWN': 'unknown error',
}

# File-level context hints (will be used to enrich descriptions)
context_hints = {
    'openjiuwen_adapter.c': 'openjiuwen',
    'syscall_router.c': 'syscall_router',
    'langchain_adapter.c': 'langchain',
    'autogen_adapter.c': 'autogen',
    'claude_adapter.c': 'claude',
    'openai_enterprise_adapter.c': 'openai',
    'china_eco_adapter.c': 'china_eco',
    'protocol_transformers.c': 'transformers',
    'agentrt_protocol_interface.c': 'protocol_interface',
    'agntcy_acp_adapter.c': 'agntcy_acp',
    'mcp_transport.c': 'mcp_transport',
    'protocol_registry.c': 'protocol_registry',
    'mcp_server.c': 'mcp_server',
    'http_gateway.c': 'http_gateway',
    'protocol_toplevel_impl.c': 'protocol_toplevel',
}

def get_indent(line):
    return len(line) - len(line.lstrip())

def process_file(filepath):
    with open(filepath, 'r') as f:
        lines = f.readlines()

    fname = os.path.basename(filepath)
    ctx = context_hints.get(fname, fname.replace('.c', ''))

    # Check if file already has error.h
    has_error_include = any('#include' in l and ('error.h' in l or 'error_compat' in l) for l in lines)

    # Check if file uses CUPOLAS_ERR (for cupolas files)
    uses_cupolas_err = any('CUPOLAS_ERR_' in l for l in lines)

    total_added = 0

    i = 0
    while i < len(lines):
        line = lines[i]
        # Match 'return AGENTRT_ERR_XXX;' or 'return CUPOLAS_ERR_XXX;'
        m = re.match(r'(\s*)return\s+(AGENTRT_ERR_\w+|CUPOLAS_ERR_\w+)\s*;', line)
        if m:
            indent = m.group(1)
            err_code = m.group(2)
            # Check if previous line already has AGENTRT_ERROR_HANDLE or error_push_ex
            if i > 0 and ('AGENTRT_ERROR_HANDLE' in lines[i-1] or 'error_push_ex' in lines[i-1]):
                i += 1
                continue

            desc = error_descriptions.get(err_code, 'error')
            error_push_line = f'{indent}agentrt_error_push_ex({err_code}, __FILE__, __LINE__, __func__, "{ctx}: {desc}");\n'
            lines.insert(i, error_push_line)
            total_added += 1
            i += 2  # Skip past the inserted line + the return line
        else:
            i += 1

    if total_added > 0:
        # Add error include if needed
        if not has_error_include:
            # Find a good place to insert the include
            insert_pos = 0
            for j, l in enumerate(lines):
                if '#include' in l and ('memory_compat' in l or 'string_compat' in l or 'logging_compat' in l or 'compat' in l):
                    insert_pos = j + 1
                    break
            
            include_line = '#include "error.h"\n'
            lines.insert(insert_pos, include_line)

        with open(filepath, 'w') as f:
            f.writelines(lines)

    return total_added

# Files to process (sorted by descending error count)
files_to_fix = [
    'agentrt/protocols/integrations/openjiuwen/src/openjiuwen_adapter.c',
    'agentrt/gateway/src/utils/syscall_router.c',
    'agentrt/protocols/frameworks/langchain/src/langchain_adapter.c',
    'agentrt/protocols/frameworks/autogen/src/autogen_adapter.c',
    'agentrt/protocols/integrations/claude/src/claude_adapter.c',
    'agentrt/protocols/integrations/openai/src/openai_enterprise_adapter.c',
    'agentrt/protocols/integrations/china_eco/src/china_eco_adapter.c',
    'agentrt/protocols/core/transformers/src/protocol_transformers.c',
    'agentrt/protocols/src/agentrt_protocol_interface.c',
    'agentrt/protocols/standards/agntcy/src/agntcy_acp_adapter.c',
    'agentrt/protocols/core/registry/src/protocol_registry.c',
    'agentrt/protocols/standards/mcp/src/mcp_transport.c',
    'agentrt/gateway/src/utils/mcp_server.c',
    'agentrt/gateway/src/gateway/http_gateway.c',
    'agentrt/protocols/src/protocol_toplevel_impl.c',
]

grand_total = 0
for fpath in files_to_fix:
    if os.path.exists(fpath):
        n = process_file(fpath)
        print(f'{os.path.basename(fpath)}: {n} error_push_ex added')
        grand_total += n
    else:
        print(f'{fpath}: FILE NOT FOUND')

print(f'\nTotal: {grand_total} error_push_ex added across {len(files_to_fix)} files')
