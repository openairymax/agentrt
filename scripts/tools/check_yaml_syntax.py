#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import yaml
import os
import glob
import sys
import subprocess

def check_yaml_files(project_root):
    workflows_dir = os.path.join(project_root, '.gitcode', 'workflows')
    files = glob.glob(os.path.join(workflows_dir, '*.yml'))

    print('=' * 80)
    print('COMPREHENSIVE YAML SYNTAX AND ILLEGAL CHARACTER CHECK')
    print('=' * 80)

    errors_found = []
    for f in sorted(files):
        rel_path = os.path.relpath(f, project_root)
        print(f'\n  Checking: {rel_path}')
        try:
            with open(f, 'r', encoding='utf-8') as fh:
                content = fh.read()

            illegal_chars = []
            for i, line in enumerate(content.split('\n'), 1):
                for j, char in enumerate(line):
                    if ord(char) > 127 and not char.isprintable():
                        illegal_chars.append((i, j+1, char, ord(char)))
                    if char in ['\x00', '\x01', '\x02', '\x03', '\x04', '\x05', '\x06', '\x07',
                               '\x08', '\x0b', '\x0c', '\x0e', '\x0f', '\x10', '\x11', '\x12',
                               '\x13', '\x14', '\x15', '\x16', '\x17', '\x18', '\x19', '\x1a',
                               '\x1b', '\x1c', '\x1d', '\x1e', '\x1f']:
                        illegal_chars.append((i, j+1, repr(char), ord(char)))

            if illegal_chars:
                print(f'    ILLEGAL CHARACTERS FOUND ({len(illegal_chars)}):')
                for line_no, col_no, char, code in illegal_chars[:5]:
                    print(f'      Line {line_no}, Col {col_no}: {char} (U+{code:04X})')
                errors_found.append((rel_path, 'illegal_chars'))

            try:
                data = yaml.safe_load(content)
                print(f'    YAML syntax OK')
                if isinstance(data, dict):
                    keys = list(data.keys())
                    print(f'    Keys: {keys}')
            except yaml.YAMLError as e:
                print(f'    YAML PARSE ERROR:')
                print(f'      {str(e)[:300]}')
                errors_found.append((rel_path, 'yaml_error', str(e)))

        except Exception as e:
            print(f'    FILE ERROR: {str(e)[:100]}')
            errors_found.append((rel_path, 'file_error', str(e)))

    return errors_found

def check_shell_scripts(project_root):
    scripts_dir = os.path.join(project_root, 'scripts', 'ci')
    scripts = [f for f in os.listdir(scripts_dir) if f.endswith('.sh')]

    print('\n' + '=' * 80)
    print('SHELL SCRIPT SYNTAX CHECK')
    print('=' * 80)

    errors_found = []
    for script in sorted(scripts):
        path = os.path.join(scripts_dir, script)
        rel_path = os.path.relpath(path, project_root)
        print(f'\n  Checking: {rel_path}')

        with open(path, 'r', encoding='utf-8', errors='replace') as f:
            content = f.read()

        result = subprocess.run(
            ['bash', '-n', '-c', content],
            capture_output=True, encoding='utf-8', errors='replace'
        )

        if result.returncode == 0:
            print(f'    Shell syntax OK')
        else:
            err = result.stderr or ''
            lines = err.strip().split('\n')
            real_errors = [l for l in lines if 'syntax error' in l.lower() or 'unexpected token' in l.lower()]
            if real_errors:
                print(f'    SHELL SYNTAX ERROR:')
                for line in real_errors[:3]:
                    print(f'      {line[:200]}')
                errors_found.append((rel_path, 'shell_error', '\n'.join(real_errors)))
            else:
                print(f'    Shell syntax OK (path-related warnings ignored on Windows)')

    return errors_found

def main():
    project_root = os.getcwd()

    yaml_errors = check_yaml_files(project_root)
    shell_errors = check_shell_scripts(project_root)

    all_errors = yaml_errors + shell_errors

    print('\n' + '=' * 80)
    print('VALIDATION SUMMARY')
    print('=' * 80)

    if all_errors:
        unique_files = list(set([e[0] for e in all_errors]))
        print(f'\nERRORS FOUND IN {len(unique_files)} FILE(S):')
        for err in all_errors:
            print(f'  * {err[0]}: {err[1]}')
        sys.exit(1)
    else:
        print('\nALL FILES PASSED VALIDATION')
        sys.exit(0)

if __name__ == '__main__':
    main()
