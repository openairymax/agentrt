#!/usr/bin/env python3
"""Fix indentation inside newly added braces and fix remaining wrong error codes - final."""

import re
import os

BASE_DIR = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

def fix_indentation_and_codes(filepath):
    with open(filepath, 'r', encoding='utf-8') as f:
        lines = f.readlines()

    indent_fixes = 0
    code_fixes = 0
    modified = False

    i = 0
    while i < len(lines):
        line = lines[i]

        # Part 1: Fix indentation in if-blocks with braces
        if_match = re.match(r'^(\s*)if\s*\(.+\)\s*\{\s*$', line)
        if if_match:
            if_indent = if_match.group(1)
            expected_indent = if_indent + '    '
            
            # Find closing brace
            if i + 1 < len(lines):
                # Check if next line is agentrt_error_push_ex at wrong indent
                next_indent_match = re.match(r'^(\s*)agentrt_error_push_ex\(', lines[i + 1])
                if next_indent_match:
                    content_indent = next_indent_match.group(1)
                    if content_indent != expected_indent:
                        # Find matching }
                        close_idx = None
                        brace_count = 1
                        for j in range(i + 1, len(lines)):
                            if '{' in lines[j]:
                                inner_match = re.match(r'^\s*\{', lines[j])
                                if not inner_match:
                                    brace_count += 1
                            if '}' in lines[j]:
                                brace_count -= 1
                                if brace_count == 0:
                                    close_idx = j
                                    break
                        
                        if close_idx and close_idx > i + 2:
                            for j in range(i + 1, close_idx):
                                lines[j] = expected_indent + lines[j].lstrip()
                            indent_fixes += 1
                            modified = True

        # Part 2: Fix error codes
        # Fix >= MAX capacity checks that were mapped to STATE_ERROR
        if 'agentrt_error_push_ex(AGENTRT_ERR_STATE_ERROR' in line:
            ctx_start = max(0, i - 3)
            context = ' '.join(l.strip() for l in lines[ctx_start:i])
            if re.search(r'>=.*(MAX|COUNT|CAPACITY)', context):
                # Fix the push line
                old_line = line
                line = line.replace('AGENTRT_ERR_STATE_ERROR', 'AGENTRT_ERR_BUFFER_TOO_SMALL')
                line = line.replace('"not initialized"', '"capacity exceeded"')
                lines[i] = line
                # Fix the return line
                if i + 1 < len(lines) and 'return AGENTRT_ERR_STATE_ERROR;' in lines[i + 1]:
                    lines[i + 1] = lines[i + 1].replace('AGENTRT_ERR_STATE_ERROR', 'AGENTRT_ERR_BUFFER_TOO_SMALL')
                code_fixes += 1
                modified = True

        i += 1

    if modified:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.writelines(lines)

    return indent_fixes, code_fixes


files_to_check = []
for root_dir in [
    os.path.join(BASE_DIR, 'agentos', 'protocols'),
    os.path.join(BASE_DIR, 'agentos', 'gateway'),
]:
    for dirpath, _, filenames in os.walk(root_dir):
        for fname in filenames:
            if fname.endswith('.c'):
                files_to_check.append(os.path.join(dirpath, fname))

total_indent = 0
total_codes = 0
for fpath in sorted(files_to_check):
    indent, codes = fix_indentation_and_codes(fpath)
    if indent > 0 or codes > 0:
        relpath = os.path.relpath(fpath, BASE_DIR)
        print(f'{relpath}: {indent} indent fixes, {codes} code fixes')
        total_indent += indent
        total_codes += codes

print(f'\nTotal: {total_indent} indent fixes, {total_codes} code fixes')