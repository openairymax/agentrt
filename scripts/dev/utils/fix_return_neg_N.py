#!/usr/bin/env python3
"""Fix return -N to AGENTOS_ERR_* + agentos_error_push_ex in protocols/ and gateway/."""

import re
import os
import sys

BASE_DIR = '/home/spharx/SpharxWorks/OpenAirymax/AgentOS'

# Context keywords → error code mapping
# Priority: check preceding lines for these keywords (order matters!)
CONTEXT_MAP = [
    (r'agentos_error_push_ex\(.*AGENTOS_ERR_UNKNOWN.*operation failed',
     'AGENTOS_ERR_OUT_OF_MEMORY'),  # fix the UNKNOWN fallback for allocation contexts
    (r'AGENTOS_CALLOC|AGENTOS_MALLOC|AGENTOS_REALLOC\b|proto_ext_framework_create',
     'AGENTOS_ERR_OUT_OF_MEMORY'),
    (r'out\s*of\s*memory|memory\s*(allocation|exhausted)\s*failed',
     'AGENTOS_ERR_OUT_OF_MEMORY'),
    (r'not\s*initialized|initialized\s*==\s*false|!\s*\w*initialized|NOT_INITIALIZED',
     'AGENTOS_ERR_STATE_ERROR'),
    (r'not\s*found|no\s*such|does\s*n.t\s*exist|could\s*n.t\s*find',
     'AGENTOS_ERR_NOT_FOUND'),
    (r'>=.*MAX|>=.*CAPACITY|too\s*many|full|limit|exceeded|>=.*_COUNT',
     'AGENTOS_ERR_BUFFER_TOO_SMALL'),
    (r'already\s*exists|already\s*registered|duplicate|already\s*loaded',
     'AGENTOS_ERR_ALREADY_EXISTS'),
    (r'timeout|timed\s*out|deadline',
     'AGENTOS_ERR_TIMEOUT'),
    (r'not\s*supported|unsupported|not\s*implemented',
     'AGENTOS_ERR_NOT_SUPPORTED'),
    (r'permission|access\s*denied|forbidden',
     'AGENTOS_ERR_PERMISSION_DENIED'),
    (r'invalid|bad\s*(param|argument|config|input)',
     'AGENTOS_ERR_INVALID_PARAM'),
    (r'read\s*failed|write\s*failed|socket\s*error|network',
     'AGENTOS_ERR_IO'),
    (r'null\s*ptr|null\s*pointer|empty\s*handle|handle\s*is\s*null|returned\s*NULL',
     'AGENTOS_ERR_NULL_POINTER'),
]

# Default mapping by numeric value (fallback)
NUMERIC_MAP = {
    -2: 'AGENTOS_ERR_INVALID_PARAM',
    -3: 'AGENTOS_ERR_NULL_POINTER',
    -4: 'AGENTOS_ERR_OUT_OF_MEMORY',
    -5: 'AGENTOS_ERR_BUFFER_TOO_SMALL',
    -6: 'AGENTOS_ERR_NOT_FOUND',
    -7: 'AGENTOS_ERR_ALREADY_EXISTS',
    -8: 'AGENTOS_ERR_TIMEOUT',
    -9: 'AGENTOS_ERR_NOT_SUPPORTED',
    -10: 'AGENTOS_ERR_PERMISSION_DENIED',
    -11: 'AGENTOS_ERR_IO',
    -12: 'AGENTOS_ERR_PARSE_ERROR',
    -13: 'AGENTOS_ERR_STATE_ERROR',
    -14: 'AGENTOS_ERR_OVERFLOW',
    -15: 'AGENTOS_ERR_UNDERFLOW',
    -16: 'AGENTOS_ERR_CANCELED',
    -17: 'AGENTOS_ERR_BUSY',
}

def determine_error_code(lines_before, neg_value):
    """Determine best error code from context lines."""
    context = ' '.join(lines_before[-5:])
    
    for pattern, code in CONTEXT_MAP:
        if re.search(pattern, context, re.IGNORECASE):
            return code
    
    # Fallback to numeric map
    return NUMERIC_MAP.get(abs(neg_value), 'AGENTOS_ERR_UNKNOWN')

def make_context_description(lines_before, neg_value, error_code):
    """Create a description string from context."""
    context = ' '.join(lines_before[-5:]).strip()
    
    # Extract function name
    func_match = re.search(r'int\s+(\w+)\s*\(', context)
    if not func_match:
        func_match = re.search(r'(\w+)\s*\(', context)
    func_name = func_match.group(1) if func_match else ''
    
    # Determine more specific context
    if 'AGENTOS_CALLOC' in context or 'AGENTOS_MALLOC' in context:
        return f'{func_name}: allocation failed' if func_name else 'allocation failed'
    if 'AGENTOS_REALLOC' in context:
        return f'{func_name}: reallocation failed' if func_name else 'reallocation failed'
    if 'initialized' in context.lower() or 'NOT_INITIALIZED' in context:
        return f'{func_name}: not initialized' if func_name else 'not initialized'
    if 'found' in context.lower() or 'not found' in context.lower():
        return f'{func_name}: not found' if func_name else 'not found'
    if '>=' in context and ('MAX' in context or 'COUNT' in context):
        return f'{func_name}: capacity exceeded' if func_name else 'capacity exceeded'
    if 'already' in context.lower() or 'duplicate' in context.lower():
        return f'{func_name}: already exists' if func_name else 'already exists'
    if 'timeout' in context.lower():
        return f'{func_name}: timeout' if func_name else 'timeout'
    if 'socket' in context.lower() or 'read' in context.lower() or 'write' in context.lower():
        return f'{func_name}: IO error' if func_name else 'IO error'
    
    return f'{func_name}: failed' if func_name else 'operation failed'

def fix_file(filepath):
    with open(filepath, 'r', encoding='utf-8') as f:
        lines = f.readlines()

    modified = False
    result_lines = []
    i = 0
    count = 0

    while i < len(lines):
        line = lines[i]
        
        # Match: return -N;
        match = re.match(r'^(\s*)(return\s+(-[0-9]+)\s*;)\s*$', line)
        
        if match:
            indent = match.group(1)
            neg_val = int(match.group(3))
            
            # Get preceding lines for context
            ctx_start = max(0, i - 5)
            ctx_lines = [l.strip() for l in lines[ctx_start:i]]
            
            error_code = determine_error_code(ctx_lines, neg_val)
            desc = make_context_description(ctx_lines, neg_val, error_code)
            
            result_lines.append(
                f'{indent}agentos_error_push_ex({error_code}, __FILE__, __LINE__, __func__, '
                f'"{desc}");\n'
            )
            result_lines.append(f'{indent}return {error_code};\n')
            count += 1
            modified = True
        else:
            result_lines.append(line)
        
        i += 1

    if modified:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.writelines(result_lines)
    
    return count

def ensure_error_h_include(filepath):
    """Add #include 'error.h' if not present and AGENTOS_ERR_* is used."""
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()
    
    if '#include "error.h"' in content or '#include <error.h>' in content:
        return False
    
    if 'AGENTOS_ERR_' not in content:
        return False
    
    # Find the last existing #include line
    lines = content.split('\n')
    last_include_idx = -1
    for idx, line in enumerate(lines):
        if re.match(r'^\s*#include\s*["<]', line):
            last_include_idx = idx
    
    if last_include_idx >= 0:
        lines.insert(last_include_idx + 1, '#include "error.h"')
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write('\n'.join(lines))
        return True
    
    return False

def remove_local_compat_defines(filepath):
    """Remove local AGENTOS_EFAIL, AGENTOS_EINVAL, etc. compat defines that conflict with error.h."""
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()
    
    original = content
    
    patterns = [
        r'#ifndef\s+AGENTOS_SUCCESS\s*\n\s*#define\s+AGENTOS_SUCCESS\s+\d+\s*\n\s*#endif\s*\n?',
        r'#ifndef\s+AGENTOS_EFAIL\s*\n\s*#define\s+AGENTOS_EFAIL\s+\(?-?\d+\)?\s*\n\s*#endif\s*\n?',
        r'#ifndef\s+AGENTOS_EINVAL\s*\n\s*#define\s+AGENTOS_EINVAL\s+\(?-?\d+\)?\s*\n\s*#endif\s*\n?',
        r'#ifndef\s+AGENTOS_ENOMEM\s*\n\s*#define\s+AGENTOS_ENOMEM\s+\(?-?\d+\)?\s*\n\s*#endif\s*\n?',
        r'#ifndef\s+AGENTOS_ENOTSUP\s*\n\s*#define\s+AGENTOS_ENOTSUP\s+\(?-?\d+\)?\s*\n\s*#endif\s*\n?',
        r'#ifndef\s+AGENTOS_EACCES\s*\n\s*#define\s+AGENTOS_EACCES\s+\(?-?\d+\)?\s*\n\s*#endif\s*\n?',
    ]
    
    for pattern in patterns:
        content = re.sub(pattern, '', content)
    
    if content != original:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(content)
        return True
    return False


# Process files
files_to_check = []
for root_dir in [
    os.path.join(BASE_DIR, 'agentos', 'protocols'),
    os.path.join(BASE_DIR, 'agentos', 'gateway'),
]:
    for dirpath, _, filenames in os.walk(root_dir):
        for fname in filenames:
            if fname.endswith('.c') and 'test' not in fname.lower():
                files_to_check.append(os.path.join(dirpath, fname))

total_fixes = 0
for fpath in sorted(files_to_check):
    # First remove local compat defines
    remove_local_compat_defines(fpath)
    
    n = fix_file(fpath)
    if n > 0:
        # Ensure error.h is included
        ensure_error_h_include(fpath)
        relpath = os.path.relpath(fpath, BASE_DIR)
        print(f'{relpath}: {n} fixes')
        total_fixes += n

print(f'\nTotal: {total_fixes} return -N fixes')