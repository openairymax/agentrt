#!/usr/bin/env python3
"""Scan for files using AGENTOS memory macros but missing memory_compat.h include."""

import re
import os

BASE_DIR = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

MEMORY_MACROS = [
    'AGENTRT_MALLOC', 'AGENTRT_FREE', 'AGENTRT_CALLOC',
    'AGENTRT_REALLOC', 'AGENTRT_STRDUP'
]

def check_file(fpath):
    with open(fpath, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # Check if uses any memory macro
    uses_macro = any(m in content for m in MEMORY_MACROS)
    if not uses_macro:
        return None  # Doesn't use memory macros, no issue
    
    # Check if includes memory_compat.h
    has_include = '#include "memory_compat.h"' in content or '#include <memory_compat.h>' in content
    has_error_h = '#include "error.h"' in content or '#include <error.h>' in content
    
    if not has_include:
        return f'missing memory_compat.h (uses AGENTOS memory macros, has_error_h={has_error_h})'
    
    return None

# Scan protocols and gateway
files_to_check = []
for root_dir in [
    os.path.join(BASE_DIR, 'agentos', 'protocols'),
    os.path.join(BASE_DIR, 'agentos', 'gateway'),
]:
    for dirpath, _, filenames in os.walk(root_dir):
        for fname in filenames:
            if fname.endswith('.c') and 'test' not in fname.lower():
                files_to_check.append(os.path.join(dirpath, fname))

missing = []
for fpath in sorted(files_to_check):
    result = check_file(fpath)
    if result:
        relpath = os.path.relpath(fpath, BASE_DIR)
        print(f'{relpath}: {result}')
        missing.append(fpath)

if not missing:
    print('All files have memory_compat.h! No missing includes.')
else:
    print(f'\n{len(missing)} files missing memory_compat.h')