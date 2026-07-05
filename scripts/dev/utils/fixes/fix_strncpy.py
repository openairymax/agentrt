import re, glob

fixed = 0
for f in sorted(glob.glob('agentos/**/*.c', recursive=True)):
    if '/test_' in f or '/tests/' in f:
        continue
    try:
        with open(f, 'r') as fh:
            content = fh.read()
    except:
        continue

    original = content

    # Pattern: strncpy((dst),(src),(sizeof(var)-1));  with possible extra )'s
    # Replace ALL occurrences with clean version
    def fix_strncpy_match(m):
        full = m.group(0)
        indent = len(full) - len(full.lstrip())
        prefix = ' ' * indent

        # Extract dst: first arg inside ((...))
        dst_m = re.search(r'strncpy\(\(([^)]+)\)', full)
        src_m = re.search(r'\),\(\([^)]+)\),', full)
        sz_m = re.search(r'sizeof\(([^)]+)\)', full)

        if not dst_m or not sz_m:
            return full  # can't parse, leave alone

        dst = dst_m.group(1)
        src = src_m.group(1) if src_m else '"?"'
        sz_var = sz_m.group(1)

        result = '%sstrncpy(%s, %s, sizeof(%s) - 1);\n%s(%s)[sizeof(%s) - 1] = \'\\0\';' % (
            prefix, dst, src, sz_var, prefix, dst, sz_var)
        return result

    # Match strncpy with AGENTRT_STRNCPY_TERM pattern (parenthesized args + sizeof-1)
    new_content = re.sub(
        r'^(\s*)strncpy\(\([^)]+\)\),\([^)]+\),\(sizeof\([^)]+\)-1\)\);',
        fix_strncpy_match,
        content,
        flags=re.MULTILINE
    )

    if new_content != original:
        with open(f, 'w') as fh:
            fh.write(new_content)
        fixed += 1
        print('Fixed: %s' % f)

print('\nTotal files fixed: %d' % fixed)
