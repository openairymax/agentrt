#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import yaml
import os
import glob
import sys

workflows_dir = '.gitcode/workflows'
files = glob.glob(f'{workflows_dir}/*.yml')

print('=' * 80)
print('COMPREHENSIVE YAML SYNTAX AND ILLEGAL CHARACTER CHECK')
print('=' * 80)

errors_found = []
for f in sorted(files):
    print(f'\n📄 Checking: {f}')
    try:
        with open(f, 'r', encoding='utf-8') as fh:
            content = fh.read()
        
        # Check for illegal characters
        illegal_chars = []
        for i, line in enumerate(content.split('\n'), 1):
            # Check for non-UTF8 characters or problematic chars
            for j, char in enumerate(line):
                if ord(char) > 127 and not char.isprintable():
                    illegal_chars.append((i, j+1, char, ord(char)))
                # Check for common problematic patterns
                if char in ['\x00', '\x01', '\x02', '\x03', '\x04', '\x05', '\x06', '\x07', 
                           '\x08', '\x0b', '\x0c', '\x0e', '\x0f', '\x10', '\x11', '\x12',
                           '\x13', '\x14', '\x15', '\x16', '\x17', '\x18', '\x19', '\x1a',
                           '\x1b', '\x1c', '\x1d', '\x1e', '\x1f']:
                    illegal_chars.append((i, j+1, repr(char), ord(char)))
        
        if illegal_chars:
            print(f'  ❌ ILLEGAL CHARACTERS FOUND ({len(illegal_chars)}):')
            for line_no, col_no, char, code in illegal_chars[:5]:  # Show first 5
                print(f'     Line {line_no}, Col {col_no}: {char} (U+{code:04X})')
            errors_found.append((f, 'illegal_chars'))
        
        # Try to parse YAML
        try:
            data = yaml.safe_load(content)
            print(f'  ✅ YAML syntax OK')
            if isinstance(data, dict):
                keys = list(data.keys())
                print(f'     Keys found: {keys}')
        except yaml.YAMLError as e:
            print(f'  ❌ YAML PARSE ERROR:')
            error_msg = str(e)
            print(f'     {error_msg[:300]}')
            errors_found.append((f, 'yaml_error', error_msg))
            
    except Exception as e:
        print(f'  ❌ FILE ERROR: {str(e)[:100]}')
        errors_found.append((f, 'file_error', str(e)))

print('\n' + '=' * 80)
print('SUMMARY')
print('=' * 80)
if errors_found:
    unique_files = list(set([e[0] for e in errors_found]))
    print(f'\n❌ ERRORS FOUND IN {len(unique_files)} FILE(S):')
    for err in errors_found:
        print(f'   • {err[0]}: {err[1]}')
    sys.exit(1)
else:
    print('\n✅ ALL FILES PASSED VALIDATION')
    sys.exit(0)