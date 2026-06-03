#!/usr/bin/env python3
"""Replace AGENTOS_EFAIL in AGENTOS_CHECK/AGENTOS_ERROR macros with specific AGENTOS_ERR_* codes."""

import re
import os

BASE_DIR = '/home/spharx/SpharxWorks/OpenAirymax/AgentOS'

def determine_error_code_from_message(message):
    """Determine error code based on the message string in AGENTOS_CHECK/AGENTOS_ERROR."""
    if not message:
        return 'AGENTOS_ERR_UNKNOWN'

    msg_lower = message.lower()

    if 'too small' in msg_lower or 'buffer_size' in msg_lower:
        return 'AGENTOS_ERR_BUFFER_TOO_SMALL'
    if 'is null' in msg_lower or 'null pointer' in msg_lower or 'null' in msg_lower.split():
        return 'AGENTOS_ERR_NULL_POINTER'
    if 'not found' in msg_lower or 'missing' in msg_lower or 'no such' in msg_lower:
        return 'AGENTOS_ERR_NOT_FOUND'
    if 'not supported' in msg_lower or 'unsupported' in msg_lower or 'not implemented' in msg_lower:
        return 'AGENTOS_ERR_NOT_SUPPORTED'
    if 'not initialized' in msg_lower or 'state' in msg_lower and 'error' in msg_lower:
        return 'AGENTOS_ERR_STATE_ERROR'
    if 'invalid' in msg_lower or 'not a ' in msg_lower:
        return 'AGENTOS_ERR_INVALID_PARAM'
    if 'not an object' in msg_lower or 'not a json' in msg_lower or 'not a string' in msg_lower:
        return 'AGENTOS_ERR_INVALID_PARAM'
    if 'not an array' in msg_lower or 'is empty' in msg_lower or 'batch is empty' in msg_lower:
        return 'AGENTOS_ERR_INVALID_PARAM'
    if 'version is not' in msg_lower:
        return 'AGENTOS_ERR_NOT_SUPPORTED'
    if 'type is invalid' in msg_lower:
        return 'AGENTOS_ERR_INVALID_PARAM'
    if 'overflow' in msg_lower or 'max' in msg_lower or 'capacity' in msg_lower:
        return 'AGENTOS_ERR_OVERFLOW'
    if 'timeout' in msg_lower or 'expired' in msg_lower:
        return 'AGENTOS_ERR_TIMEOUT'
    if 'not available' in msg_lower:
        return 'AGENTOS_ERR_NOT_SUPPORTED'
    if 'is empty' in msg_lower or 'empty' in msg_lower:
        return 'AGENTOS_ERR_INVALID_PARAM'
    if 'type is not' in msg_lower or 'not http' in msg_lower.lower():
        return 'AGENTOS_ERR_INVALID_PARAM'

    return 'AGENTOS_ERR_UNKNOWN'


def fix_file(filepath):
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    modified = False
    count = 0

    def replace_efail_in_check(match):
        nonlocal modified, count
        full = match.group(0)
        prefix = match.group(1)
        message = match.group(2)
        suffix = match.group(3)

        error_code = determine_error_code_from_message(message)
        replacement = f'{prefix}{error_code}, "{message}"{suffix}'
        count += 1
        modified = True
        return replacement

    def replace_efail_in_error(match):
        nonlocal modified, count
        full = match.group(0)
        prefix = match.group(1)
        message = match.group(2)
        suffix = match.group(3)

        error_code = determine_error_code_from_message(message)
        replacement = f'{prefix}{error_code}, "{message}"{suffix}'
        count += 1
        modified = True
        return replacement

    content = re.sub(
        r'(AGENTOS_CHECK\([^,]+,\s*)AGENTOS_EFAIL\s*,\s*"([^"]*)"(\s*\))',
        replace_efail_in_check,
        content
    )

    content = re.sub(
        r'(AGENTOS_ERROR\(\s*)AGENTOS_EFAIL\s*,\s*"([^"]*)"(\s*\))',
        replace_efail_in_error,
        content
    )

    if modified:
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
            if fname.endswith('.c') and 'test' not in fname.lower():
                files_to_check.append(os.path.join(dirpath, fname))

total_fixes = 0
for fpath in sorted(files_to_check):
    n = fix_file(fpath)
    if n > 0:
        relpath = os.path.relpath(fpath, BASE_DIR)
        print(f'{relpath}: {n} fixes')
        total_fixes += n

print(f'\nTotal: {total_fixes} AGENTOS_EFAIL → specific error codes in macros')