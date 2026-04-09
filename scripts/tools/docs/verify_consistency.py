#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Final consistency verification for V1.9 documentation update
"""

import os
import re
from pathlib import Path
from datetime import datetime

def verify_file(file_path):
    """Verify a file meets V1.9 standards"""
    issues = []

    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            content = f.read()
            lines = content.split('\n')

        # Check 1: Copyright statement
        if not content.startswith('Copyright (c) 2026 SPHARX Ltd.'):
            if content.startswith('Copyright'):
                issues.append("⚠️  Old copyright format (SPHARX without Ltd.)")
            else:
                issues.append("❌ Missing copyright statement")

        # Check 2: Tagline
        if '"From data intelligence emerges."' not in lines[:3]:
            issues.append("⚠️  Missing or incorrect tagline")

        # Check 3: Version format (check first 30 lines)
        header = '\n'.join(lines[:30])
        version_match = re.search(r'\*\*版本\*\*:\s*Doc\s*V1\.[89]', header)
        if not version_match:
            version_match_old = re.search(r'\*\*版本\*\*:\s*(?:Doc\s+)?V?\d+\.\d+', header)
            if version_match_old:
                issues.append(f"⚠️  Version not updated to V1.8/V1.9: {version_match_old.group()}")
            else:
                issues.append("⚠️  No version information found in header")

        # Check 4: Date
        date_match = re.search(r'2026-04-09', header)
        if not date_match:
            old_date = re.search(r'2026-\d{2}-\d{2}', header)
            if old_date:
                issues.append(f"⚠️  Date not updated to 2026-04-09: {old_date.group()}")

        # Check 5: Team authorship
        if '**作者**: Team' in header or '**维护者**: Team' in header:
            pass  # Good
        else:
            author_match = re.search(r'\*\*(?:作者|维护者)\*\*:\s*\w+$', header, re.MULTILINE)
            if author_match:
                issues.append(f"⚠️  Individual author instead of Team: {author_match.group().strip()}")

        # Check 6: UTF-8 encoding (no BOM)
        with open(file_path, 'rb') as f:
            raw = f.read(3)
            if raw == b'\xef\xbb\xbf':
                issues.append("🔴 File has UTF-8 BOM")

        return len(issues) == 0, issues

    except Exception as e:
        return False, [f"❌ Error reading file: {e}"]

def main():
    docs_dir = Path(r'd:\SPHARX-CN\SpharxWorks\AgentOS\docs')

    # Exclude Basic_Theories as per user request
    exclude_dirs = {'Basic_Theories', '.git'}

    print("=" * 80)
    print("📋 AgentOS Documentation V1.9 Consistency Verification")
    print(f"⏰ Time: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print("=" * 80)

    total_files = 0
    passed_files = 0
    failed_files = 0
    warning_files = 0
    all_issues = []

    for root, dirs, files in os.walk(docs_dir):
        # Skip excluded directories
        dirs[:] = [d for d in dirs if d not in exclude_dirs and not d.startswith('.')]

        for file in files:
            if file.endswith('.md'):
                file_path = Path(root) / file
                rel_path = file_path.relative_to(docs_dir)
                total_files += 1

                passed, issues = verify_file(file_path)

                if passed:
                    passed_files += 1
                    print(f"✅ {rel_path}")
                elif any("🔴" in issue for issue in issues):
                    failed_files += 1
                    print(f"🔴 {rel_path}")
                    for issue in issues:
                        print(f"   {issue}")
                        all_issues.append(f"{rel_path}: {issue}")
                else:
                    warning_files += 1
                    print(f"⚠️  {rel_path}")
                    for issue in issues[:3]:  # Show max 3 warnings
                        print(f"   {issue}")

    print("\n" + "=" * 80)
    print("📊 VERIFICATION SUMMARY")
    print("=" * 80)
    print(f"Total files scanned:     {total_files}")
    print(f"✅ Fully compliant:      {passed_files} ({passed_files/total_files*100:.1f}%)")
    print(f"⚠️  With warnings:       {warning_files} ({warning_files/total_files*100:.1f}%)")
    print(f"🔴 With errors:          {failed_files} ({failed_files/total_files*100:.1f}%)")
    print("=" * 80)

    if failed_files == 0 and warning_files < 5:
        print("\n🎉 SUCCESS! Documentation is consistent with V1.9 standards.")
        print("All documents have been successfully updated.")
    elif failed_files > 0:
        print(f"\n⚠️  ATTENTION: {failed_files} files have critical issues that need manual review.")
        print("Please check the issues listed above.")
    else:
        print(f"\n✅ Good: Only minor warnings in {warning_files} files.")

if __name__ == '__main__':
    main()
