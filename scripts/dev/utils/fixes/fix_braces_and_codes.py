#!/usr/bin/env python3
"""Fix both: (1) missing braces for if-statements with error_push + return, (2) wrong error codes."""

import re
import os

BASE_DIR = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

def fix_braces_and_error_codes(filepath):
    """Fix missing braces around if-blocks containing error_push_ex + return, and fix wrong error codes."""
    with open(filepath, 'r', encoding='utf-8') as f:
        lines = f.readlines()

    result = []
    i = 0
    brace_fixes = 0
    code_fixes = 0
    modified = False

    while i < len(lines):
        line = lines[i]

        # Check if this line is an if-statement without opening brace
        # Pattern: indent + if (...) + (no {)
        if_match = re.match(r'^(\s*if\s*\([^{]+\))\s*$', line)

        if if_match and i + 2 < len(lines):
            indent = if_match.group(1)
            next_line = lines[i + 1]
            next_next_line = lines[i + 2]

            # Check: next line is agentrt_error_push_ex, next+1 is return CODE;
            push_match = re.match(r'^(\s*)agentrt_error_push_ex\(', next_line)
            ret_match = re.match(r'^(\s*)return\s+(AGENTRT_ERR_\w+)\s*;', next_next_line)

            if push_match and ret_match:
                push_indent = push_match.group(1)
                ret_indent = ret_match.group(1)

                # Both should have same indent, which should be one level deeper than if
                if push_indent == ret_indent:
                    # Add braces!
                    result.append(line.rstrip('\n') + '\n')
                    result.append(push_indent + '{\n')

                    # Check and fix error codes in push line
                    fixed_push = next_line
                    fixed_ret = next_next_line

                    if 'AGENTRT_ERR_NULL_POINTER' in next_line and 'not initialized' in next_line:
                        fixed_push = next_line.replace('AGENTRT_ERR_NULL_POINTER', 'AGENTRT_ERR_STATE_ERROR')
                        fixed_ret = next_next_line.replace('AGENTRT_ERR_NULL_POINTER', 'AGENTRT_ERR_STATE_ERROR')
                        code_fixes += 1

                    if 'AGENTRT_ERR_UNKNOWN' in next_line and 'a2a_timestamp_ms' in next_line:
                        fixed_push = next_line.replace('AGENTRT_ERR_UNKNOWN', 'AGENTRT_ERR_NOT_SUPPORTED')
                        fixed_ret = next_next_line.replace('AGENTRT_ERR_UNKNOWN', 'AGENTRT_ERR_NOT_SUPPORTED')
                        code_fixes += 1

                    if 'AGENTRT_ERR_UNKNOWN' in next_line and 'AGENTRT_STRDUP' in next_line:
                        fixed_push = next_line.replace('AGENTRT_ERR_UNKNOWN', 'AGENTRT_ERR_OUT_OF_MEMORY')
                        fixed_ret = next_next_line.replace('AGENTRT_ERR_UNKNOWN', 'AGENTRT_ERR_OUT_OF_MEMORY')
                        code_fixes += 1

                    if 'AGENTRT_ERR_UNKNOWN' in next_line and 'memset' in next_line:
                        fixed_push = next_line.replace('AGENTRT_ERR_UNKNOWN', 'AGENTRT_ERR_NULL_POINTER')
                        fixed_ret = next_next_line.replace('AGENTRT_ERR_UNKNOWN', 'AGENTRT_ERR_NULL_POINTER')
                        code_fixes += 1

                    result.append(fixed_push)
                    result.append(fixed_ret)
                    result.append(push_indent + '}\n')
                    brace_fixes += 1
                    i += 3
                    modified = True
                    continue
        else:
            # Also check for non-if error_push + return patterns that might be standalone
            push_match2 = re.match(r'^(\s*)agentrt_error_push_ex\((AGENTRT_ERR_UNKNOWN),', line)
            if push_match2 and i + 1 < len(lines):
                ret_match2 = re.match(r'^(\s*)return\s+AGENTRT_ERR_UNKNOWN\s*;', lines[i + 1])
                if ret_match2:
                    indent_p = push_match2.group(1)
                    # Fix UNKNOWN codes based on description
                    if 'a2a_timestamp_ms' in line:
                        line = line.replace('AGENTRT_ERR_UNKNOWN', 'AGENTRT_ERR_NOT_SUPPORTED')
                        lines[i + 1] = lines[i + 1].replace('AGENTRT_ERR_UNKNOWN', 'AGENTRT_ERR_NOT_SUPPORTED')
                        code_fixes += 1
                        modified = True
                    elif 'AGENTRT_STRDUP' in line:
                        line = line.replace('AGENTRT_ERR_UNKNOWN', 'AGENTRT_ERR_OUT_OF_MEMORY')
                        lines[i + 1] = lines[i + 1].replace('AGENTRT_ERR_UNKNOWN', 'AGENTRT_ERR_OUT_OF_MEMORY')
                        code_fixes += 1
                        modified = True
                    elif 'memset' in line:
                        line = line.replace('AGENTRT_ERR_UNKNOWN', 'AGENTRT_ERR_NULL_POINTER')
                        lines[i + 1] = lines[i + 1].replace('AGENTRT_ERR_UNKNOWN', 'AGENTRT_ERR_NULL_POINTER')
                        code_fixes += 1
                        modified = True

        result.append(line)
        i += 1

    if modified:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.writelines(result)

    return brace_fixes, code_fixes


files_to_check = []
for root_dir in [
    os.path.join(BASE_DIR, 'agentos', 'protocols'),
    os.path.join(BASE_DIR, 'agentos', 'gateway'),
]:
    for dirpath, _, filenames in os.walk(root_dir):
        for fname in filenames:
            if fname.endswith('.c'):
                files_to_check.append(os.path.join(dirpath, fname))

total_braces = 0
total_codes = 0
for fpath in sorted(files_to_check):
    b, c = fix_braces_and_error_codes(fpath)
    if b > 0 or c > 0:
        relpath = os.path.relpath(fpath, BASE_DIR)
        print(f'{relpath}: {b} brace fixes, {c} code fixes')
        total_braces += b
        total_codes += c

print(f'\nTotal: {total_braces} brace fixes, {total_codes} code fixes')