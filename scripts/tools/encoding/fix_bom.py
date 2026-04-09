#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Ultimate BOM Remover - Remove ALL BOM occurrences from files
===========================================================
Handles multiple BOMs, corrupted files, and ensures clean UTF-8 output.

Usage:
    python scripts/fix_bom_ultimate.py --fix    # Fix all files
"""

import os
import sys
from pathlib import Path

def remove_all_boms(content):
    """Remove all UTF-8 BOM occurrences from content."""
    bom = b'\xef\xbb\xbf'
    
    # Keep removing BOM from start until no more
    while content.startswith(bom):
        content = content[3:]
    
    return content

def fix_file(file_path):
    """Fix a file by removing all BOMs."""
    try:
        with open(file_path, 'rb') as f:
            original_content = f.read()
        
        # Check if file has any BOM
        if not original_content.startswith(b'\xef\xbb\xbf'):
            return False, "no_bom"
        
        # Remove all BOMs
        fixed_content = remove_all_boms(original_content)
        
        # Write back
        with open(file_path, 'wb') as f:
            f.write(fixed_content)
        
        # Calculate stats
        original_size = len(original_content)
        fixed_size = len(fixed_content)
        removed_bytes = original_size - fixed_size
        
        return True, {
            'original_size': original_size,
            'fixed_size': fixed_size,
            'removed_bytes': removed_bytes,
            'bom_count': removed_bytes // 3
        }
        
    except Exception as e:
        return False, str(e)

def find_files_with_bom(root_dir):
    """Find all files that have BOM."""
    skip_dirs = {
        '.git', '__pycache__', 'node_modules', '.venv', 'venv',
        'build', 'dist', '.tox', '.mypy_cache', '.pytest_cache',
        '*.egg-info', '.next', '.nuxt', 'target', 'vendor',
        '.idea', '.vscode'
    }
    
    bom_files = []
    total_files = 0
    
    for root, dirs, files in os.walk(root_dir):
        dirs[:] = [d for d in dirs if d not in skip_dirs and not d.startswith('.')]
        
        for file in files:
            full_path = os.path.join(root, file)
            
            # Skip binary extensions
            ext = Path(file).suffix.lower()
            binary_exts = {'.png', '.jpg', '.jpeg', '.gif', '.ico', '.webp',
                          '.pdf', '.exe', '.dll', '.so', '.zip', '.tar', '.gz'}
            if ext in binary_exts:
                continue
            
            total_files += 1
            
            try:
                with open(full_path, 'rb') as f:
                    first_bytes = f.read(3)
                    if first_bytes == b'\xef\xbb\xbf':
                        rel_path = os.path.relpath(full_path, root_dir)
                        bom_files.append((rel_path, full_path))
            except:
                pass
            
            if total_files % 500 == 0:
                print(f"  ⏳ Scanned {total_files} files...")
    
    return bom_files, total_files

def main():
    import argparse
    
    parser = argparse.ArgumentParser(description='Ultimate BOM remover')
    parser.add_argument('--fix', action='store_true', help='Fix all files with BOM')
    args = parser.parse_args()
    
    root_dir = Path(__file__).parent.parent
    
    print("=" * 80)
    print("🔧 Ultimate BOM Remover")
    print("=" * 80)
    print(f"📂 Scanning: {root_dir}")
    print()
    
    # Find files with BOM
    print("🔍 Scanning for BOM...")
    bom_files, total_files = find_files_with_bom(str(root_dir))
    
    print()
    print("=" * 80)
    print(f"📊 Results:")
    print(f"   Total files scanned: {total_files}")
    print(f"   Files with BOM: {len(bom_files)}")
    print(f"   Clean files: {total_files - len(bom_files)}")
    print("=" * 80)
    
    if not bom_files:
        print("\n✅ Perfect! No files with BOM found.")
        return
    
    print(f"\n📋 Files with BOM ({len(bom_files)}):")
    print("-" * 80)
    
    for rel_path, _ in bom_files:
        print(f"  📄 {rel_path}")
    
    if args.fix:
        print()
        print(f"\n🔧 Fixing {len(bom_files)} files...")
        print("-" * 80)
        
        success_count = 0
        fail_count = 0
        total_removed = 0
        
        for rel_path, full_path in bom_files:
            success, result = fix_file(full_path)
            
            if success:
                success_count += 1
                info = result
                total_removed += info['removed_bytes']
                
                print(f"  ✅ {rel_path}")
                print(f"     Removed {info['bom_count']} BOM(s) ({info['removed_bytes']} bytes)")
                print(f"     Size: {info['original_size']} → {info['fixed_size']} bytes")
            else:
                fail_count += 1
                if result == "no_bom":
                    print(f"  ⚠️  {rel_path}: Already clean (race condition?)")
                else:
                    print(f"  ❌ {rel_path}: {result}")
        
        print()
        print("=" * 80)
        print(f"✅ Fix Complete:")
        print(f"   ✅ Successfully fixed: {success_count}")
        print(f"   ❌ Failed: {fail_count}")
        print(f"   💾 Total bytes removed: {total_removed}")
        print("=" * 80)
        
        # Final verification
        print("\n🔍 Final verification...")
        bom_files_after, _ = find_files_with_bom(str(root_dir))
        
        if bom_files_after:
            print(f"\n⚠️  Warning: {len(bom_files_after)} files still have BOM!")
            for rel_path, _ in bom_files_after[:10]:
                print(f"  - {rel_path}")
            if len(bom_files_after) > 10:
                print(f"  ... and {len(bom_files_after) - 10} more")
        else:
            print("\n🎉 Success! All files are now clean UTF-8 (no BOM)!")
    else:
        print()
        print("\n💡 To fix these files, run:")
        print(f"   python {sys.argv[0]} --fix")

if __name__ == '__main__':
    main()
