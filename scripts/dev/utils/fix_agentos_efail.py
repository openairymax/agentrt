#!/usr/bin/env python3
"""Replace return AGENTOS_EFAIL with specific AGENTOS_ERR_* + agentos_error_push_ex."""

import re
import os

BASE_DIR = '/home/spharx/SpharxWorks/OpenAirymax/AgentOS'

# Context keywords → error code mapping (order matters - first match wins)
CONTEXT_MAP = [
    (r'AGENTOS_CALLOC|AGENTOS_MALLOC|AGENTOS_REALLOC\b(?!\s*\()|AGENTOS_STRDUP',
     'AGENTOS_ERR_OUT_OF_MEMORY'),
    (r'out\s*of\s*memory|memory\s*allocation\s*failed|OOM',
     'AGENTOS_ERR_OUT_OF_MEMORY'),
    (r'not\s*initialized|initialized\s*==\s*false|!\s*\w*initialized|NOT_INITIALIZED',
     'AGENTOS_ERR_STATE_ERROR'),
    (r'not\s*found|no\s*such|doesn\'t\s*exist|couldn\'t\s*find|not\s*active',
     'AGENTOS_ERR_NOT_FOUND'),
    (r'>=.*MAX|>=.*CAPACITY|too\s*many|full\b(?!\s*path)|limit.*reached|>=.*_COUNT|>=.*limit',
     'AGENTOS_ERR_BUFFER_TOO_SMALL'),
    (r'already\s*exists|already\s*registered|duplicate|already\s*loaded|already\s*open',
     'AGENTOS_ERR_ALREADY_EXISTS'),
    (r'timeout|timed\s*out|deadline|expired',
     'AGENTOS_ERR_TIMEOUT'),
    (r'not\s*supported|unsupported|not\s*implemented|unknown.*(action|method|protocol)',
     'AGENTOS_ERR_NOT_SUPPORTED'),
    (r'permission|access\s*denied|forbidden|unauthorized',
     'AGENTOS_ERR_PERMISSION_DENIED'),
    (r'invalid\s*(param|argument|config|input|header|request|state|format|JSON|json|url|endpoint|method)',
     'AGENTOS_ERR_INVALID_PARAM'),
    (r'bad\s*(param|argument|config|input|state|format)',
     'AGENTOS_ERR_INVALID_PARAM'),
    (r'read\s*failed|write\s*failed|socket\s*error|send\s*failed|recv\s*failed|connect\s*failed|network\s*error',
     'AGENTOS_ERR_IO'),
    (r'parse\s*(error|failed|failure)|malformed|invalid\s*JSON|JSON\s*parse\s*failed',
     'AGENTOS_ERR_PARSE_ERROR'),
    (r'mismatch|version\s*mismatch|type\s*mismatch',
     'AGENTOS_ERR_INVALID_PARAM'),
    (r'!\s*\w+\s*$|==\s*NULL|null\s*ptr|null\s*pointer|empty\s*handle|ret.*NULL',
     'AGENTOS_ERR_NULL_POINTER'),
]

def determine_error_code(lines_before, neg_value=-1):
    """Determine best error code from context lines."""
    context = ' '.join(lines_before[-5:])
    
    for pattern, code in CONTEXT_MAP:
        if re.search(pattern, context, re.IGNORECASE):
            return code
    
    return 'AGENTOS_ERR_UNKNOWN'

def make_context_description(lines_before, error_code):
    """Create a description string from context."""
    context = ' '.join(lines_before[-5:]).strip()
    
    func_match = re.search(r'int\s+(\w+)\s*\(', context)
    if not func_match:
        func_match = re.search(r'static\s+\w+\s+(\w+)\s*\(', context)
    if not func_match:
        func_match = re.search(r'\b(\w+)\s*\(', context)
    func_name = func_match.group(1) if func_match else ''
    
    if 'AGENTOS_CALLOC' in context or 'AGENTOS_MALLOC' in context:
        return f'{func_name}: allocation failed' if func_name else 'allocation failed'
    if 'AGENTOS_STRDUP' in context:
        return f'{func_name}: strdup failed' if func_name else 'strdup failed'
    if 'initialized' in context.lower():
        return f'{func_name}: not initialized' if func_name else 'not initialized'
    if 'not found' in context.lower() or 'not active' in context.lower():
        return f'{func_name}: not found' if func_name else 'not found'
    if '>=' in context and ('MAX' in context or 'COUNT' in context):
        return f'{func_name}: capacity exceeded' if func_name else 'capacity exceeded'
    if 'already' in context.lower():
        return f'{func_name}: already exists' if func_name else 'already exists'
    if 'timeout' in context.lower() or 'expired' in context.lower():
        return f'{func_name}: timeout' if func_name else 'timeout'
    if 'not supported' in context.lower() or 'unknown' in context.lower():
        return f'{func_name}: not supported' if func_name else 'not supported'
    if 'invalid' in context.lower():
        return f'{func_name}: invalid parameter' if func_name else 'invalid parameter'
    if 'socket' in context.lower() or 'send' in context.lower() or 'recv' in context.lower() or 'connect' in context.lower():
        return f'{func_name}: IO error' if func_name else 'IO error'
    if 'parse' in context.lower() or 'JSON' in context:
        return f'{func_name}: parse error' if func_name else 'parse error'
    if 'null' in context.lower() or '== NULL' in context:
        return f'{func_name}: null pointer' if func_name else 'null pointer'
    
    return f'{func_name}: failed' if func_name else 'operation failed'

def fix_file(filepath):
    with open(filepath, 'r', encoding='utf-8') as f:
        lines = f.readlines()

    modified = False
    result_lines = []
    i = 0
    count = 0
    skip_next = 0

    while i < len(lines):
        line = lines[i]
        
        # Skip lines that are part of a return replacement we already handled
        if skip_next > 0:
            result_lines.append(line)
            skip_next -= 1
            i += 1
            continue
        
        # Skip if this return AGENTOS_EFAIL already has a preceding agentos_error_push_ex
        # Pattern: line above is already agentos_error_push_ex
        if re.match(r'^\s*return\s+AGENTOS_EFAIL\s*;(.*)$', line):
            prev_line = lines[i-1] if i > 0 else ''
            prev_prev = lines[i-2] if i > 1 else ''
            
            # Check if there's already a push_ex right before (within 2 lines, ignoring blank lines)
            already_has_push = False
            for check_i in range(max(0, i-3), i):
                if 'agentos_error_push_ex(' in lines[check_i]:
                    already_has_push = True
                    break
            
            if already_has_push:
                # Just replace AGENTOS_EFAIL with specific code
                ctx_start = max(0, i - 5)
                ctx_lines = [l.strip() for l in lines[ctx_start:i]]
                error_code = determine_error_code(ctx_lines)
                
                new_line = line.replace('AGENTOS_EFAIL', error_code)
                if new_line != line:
                    result_lines.append(new_line)
                    count += 1
                    modified = True
                    i += 1
                    continue
            
            # No existing push_ex - add one
            indent = re.match(r'^(\s*)', line).group(1)
            
            ctx_start = max(0, i - 5)
            ctx_lines = [l.strip() for l in lines[ctx_start:i]]
            
            error_code = determine_error_code(ctx_lines)
            desc = make_context_description(ctx_lines, error_code)
            
            result_lines.append(
                f'{indent}agentos_error_push_ex({error_code}, __FILE__, __LINE__, __func__, '
                f'"{desc}");\n'
            )
            result_lines.append(f'{indent}return {error_code};\n')
            count += 1
            modified = True
            i += 1
            continue

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
    n = fix_file(fpath)
    if n > 0:
        ensure_error_h_include(fpath)
        relpath = os.path.relpath(fpath, BASE_DIR)
        print(f'{relpath}: {n} fixes')
        total_fixes += n

print(f'\nTotal: {total_fixes} AGENTOS_EFAIL → specific code fixes')