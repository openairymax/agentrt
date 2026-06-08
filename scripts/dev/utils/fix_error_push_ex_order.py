#!/usr/bin/env python3
"""Fix agentos_error_push_ex parameter order in all affected files."""

import re
import os

BASE_DIR = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

WRONG_ORDER_RE = re.compile(
    r'(agentos_error_push_ex\()'
    r'(AGENTOS_ERR_\w+|CUPOLAS_ERR_\w+)'
    r',\s*'
    r'("(?:[^"\\]|\\.)*")'
    r',\s*__FILE__\s*,\s*__LINE__\s*,\s*__func__\s*\)'
)

def fix_file(filepath):
    with open(filepath, 'r') as f:
        content = f.read()

    original = content
    count = 0

    def replacer(m):
        nonlocal count
        count += 1
        func_call = m.group(1)
        code = m.group(2)
        message = m.group(3)
        return f'{func_call}{code}, __FILE__, __LINE__, __func__, {message})'

    content = WRONG_ORDER_RE.sub(replacer, content)

    if content != original:
        with open(filepath, 'w') as f:
            f.write(content)
        return count
    return 0


files_to_check = []
for root_dir in [
    os.path.join(BASE_DIR, 'agentos', 'protocols'),
    os.path.join(BASE_DIR, 'agentos', 'gateway'),
]:
    for dirpath, _, filenames in os.walk(root_dir):
        for fname in filenames:
            if fname.endswith('.c') or fname.endswith('.h'):
                files_to_check.append(os.path.join(dirpath, fname))

total_fixes = 0
for fpath in sorted(files_to_check):
    n = fix_file(fpath)
    if n > 0:
        relpath = os.path.relpath(fpath, BASE_DIR)
        print(f'{relpath}: {n} fixes')
        total_fixes += n

print(f'\nTotal: {total_fixes} agentos_error_push_ex calls fixed')