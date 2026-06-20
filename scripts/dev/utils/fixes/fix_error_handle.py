#!/usr/bin/env python3
"""Replace AGENTOS_ERROR_HANDLE(code, msg); return code; with AGENTOS_ERROR(code, msg);"""

import re
import os

BASE_DIR = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

def fix_error_handle_pattern(filepath):
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    original = content
    count = 0

    # Pattern: AGENTOS_ERROR_HANDLE(CODE, "msg");
    #          <same indent> return CODE;
    # Replace with: AGENTOS_ERROR(CODE, "msg");
    #
    # Handle both whitespace between them
    pattern = re.compile(
        r'(\s*)AGENTOS_ERROR_HANDLE\((AGENTOS_ERR_\w+),\s*"([^"]*)"\);\s*\n'
        r'\s*return\s+\2\s*;',
        re.MULTILINE
    )

    def replacer(m):
        nonlocal count
        count += 1
        indent = m.group(1)
        code = m.group(2)
        msg = m.group(3)
        return f'{indent}AGENTOS_ERROR({code}, "{msg}");'

    content = pattern.sub(replacer, content)

    if content != original:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(content)

    return count


files_to_check = []
for root_dir in [
    os.path.join(BASE_DIR, 'agentos', 'protocols'),
    os.path.join(BASE_DIR, 'agentos', 'gateway'),
]:
    for dirpath, _, filenames in os.walk(root_dir):
        for fname in filenames:
            if fname.endswith('.c'):
                files_to_check.append(os.path.join(dirpath, fname))

total = 0
for fpath in sorted(files_to_check):
    n = fix_error_handle_pattern(fpath)
    if n > 0:
        relpath = os.path.relpath(fpath, BASE_DIR)
        print(f'{relpath}: {n} fixes')
        total += n

print(f'\nTotal: {total} AGENTOS_ERROR_HANDLE → AGENTOS_ERROR replacements')