#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
AgentOS Document Encoding Checker and Converter
===============================================
Detects encoding of all text files in the project and converts non-UTF-8 files to UTF-8.

Usage:
    python scripts/check_encoding.py              # Check all files (dry run)
    python scripts/check_encoding.py --convert     # Convert non-UTF-8 files to UTF-8
    python scripts/check_encoding.py --ext md      # Only check .md files
"""

import os
import sys
import chardet
from pathlib import Path

def detect_file_encoding(file_path):
    """Detect the encoding of a file."""
    try:
        with open(file_path, 'rb') as f:
            raw_data = f.read(10240)  # Read first 10KB for detection
            if not raw_data:
                return 'utf-8', 1.0  # Empty file, assume UTF-8
            
            result = chardet.detect(raw_data)
            return result['encoding'], result['confidence']
    except Exception as e:
        return None, 0

def is_text_file(file_path):
    """Check if a file is a text file based on extension."""
    text_extensions = {
        '.md', '.txt', '.rst', '.adoc',
        '.py', '.js', '.ts', '.tsx', '.jsx',
        '.c', '.h', '.cpp', '.hpp', '.cc', '.cxx',
        '.go', '.rs', '.java',
        '.yml', '.yaml', '.toml', '.ini', '.cfg', '.conf',
        '.json', '.xml', '.html', '.css', '.scss', '.less',
        '.sh', '.bash', '.zsh', '.fish', '.bat', '.ps1',
        '.cmake', '.makefile', 'Makefile',
        '.gitignore', '.gitattributes', '.gitmodules',
        '.editorconfig', '.dockerignore',
        '.env.example', '.env'
    }
    
    ext = Path(file_path).suffix.lower()
    if ext in text_extensions:
        return True
    
    # Special filenames without extension or with special extensions
    filename = Path(file_path).name
    special_files = ['Makefile', 'CMakeLists.txt', 'Dockerfile', 'LICENSE', 'README', 'CHANGELOG', 'CONTRIBUTING', 'CODE_OF_CONDUCT']
    if filename in special_files or any(filename.startswith(s) for s in ['LICENSE', 'README', 'CHANGELOG']):
        return True
    
    return False

def find_text_files(root_dir, target_ext=None):
    """Find all text files in the directory."""
    text_files = []
    
    # Directories to skip
    skip_dirs = {
        '.git', '__pycache__', 'node_modules', '.venv', 'venv',
        'build', 'dist', '.tox', '.mypy_cache', '.pytest_cache',
        '*.egg-info', '.next', '.nuxt', 'target', 'vendor'
    }
    
    for root, dirs, files in os.walk(root_dir):
        # Skip unwanted directories
        dirs[:] = [d for d in dirs if d not in skip_dirs and not d.startswith('.')]
        
        for file in files:
            file_path = os.path.join(root, file)
            
            if target_ext:
                if not file.endswith(f'.{target_ext}'):
                    continue
            
            if is_text_file(file_path):
                text_files.append(file_path)
    
    return sorted(text_files)

def convert_to_utf8(file_path, source_encoding):
    """Convert a file from its current encoding to UTF-8."""
    try:
        # Read with detected encoding
        with open(file_path, 'r', encoding=source_encoding) as f:
            content = f.read()
        
        # Write as UTF-8
        with open(file_path, 'w', encoding='utf-8', newline='\n') as f:
            f.write(content)
        
        return True
    except Exception as e:
        print(f"  ❌ Error converting {file_path}: {e}")
        return False

def main():
    import argparse
    
    parser = argparse.ArgumentParser(description='Check and convert document encodings')
    parser.add_argument('--convert', action='store_true', help='Convert non-UTF-8 files to UTF-8')
    parser.add_argument('--ext', type=str, help='Only check files with this extension (e.g., md)')
    args = parser.parse_args()
    
    root_dir = Path(__file__).parent.parent  # AgentOS root directory
    
    print(f"🔍 Scanning documents in: {root_dir}")
    print("=" * 80)
    
    # Find all text files
    text_files = find_text_files(str(root_dir), args.ext)
    print(f"📊 Found {len(text_files)} text files")
    print()
    
    # Categorize by encoding
    utf8_files = []
    non_utf8_files = []
    unknown_files = []
    
    print("📋 Checking file encodings...")
    print("-" * 80)
    
    for i, file_path in enumerate(text_files, 1):
        rel_path = os.path.relpath(file_path, str(root_dir))
        encoding, confidence = detect_file_encoding(file_path)
        
        # Normalize encoding name
        if encoding:
            encoding_lower = encoding.lower()
            if encoding_lower in ['utf-8', 'utf8', 'ascii']:
                utf8_files.append((rel_path, encoding, confidence))
            else:
                non_utf8_files.append((rel_path, encoding, confidence))
        else:
            unknown_files.append(rel_path)
        
        # Show progress every 100 files
        if i % 100 == 0:
            print(f"  ⏳ Checked {i}/{len(text_files)} files...")
    
    print()
    
    # Print results
    print("=" * 80)
    print(f"✅ UTF-8/ASCII files: {len(utf8_files)}")
    print(f"⚠️  Non-UTF-8 files: {len(non_utf8_files)}")
    print(f"❓ Unknown encoding: {len(unknown_files)}")
    print("=" * 80)
    
    if non_utf8_files:
        print("\n⚠️  Files needing conversion:")
        print("-" * 80)
        
        for rel_path, encoding, confidence in non_utf8_files:
            status = "🔴 LOW CONFIDENCE" if confidence < 0.7 else "🟡 MEDIUM" if confidence < 0.9 else "🟢 HIGH"
            print(f"  [{status}] {rel_path}")
            print(f"       Encoding: {encoding} (confidence: {confidence:.2%})")
        
        print()
        
        if args.convert:
            print(f"\n🔄 Converting {len(non_utf8_files)} files to UTF-8...")
            converted = 0
            failed = 0
            
            for rel_path, encoding, confidence in non_utf8_files:
                full_path = os.path.join(str(root_dir), rel_path)
                
                # Use the detected encoding, but try common alternatives if it fails
                encodings_to_try = [encoding]
                if encoding:
                    if 'gb' in encoding.lower():
                        encodings_to_try.extend(['gb18030', 'gbk', 'gb2312'])
                    elif 'big5' in encoding.lower():
                        encodings_to_try.extend(['big5', 'big5hkscs'])
                
                success = False
                for enc in encodings_to_try:
                    if enc and convert_to_utf8(full_path, enc):
                        success = True
                        break
                
                if success:
                    converted += 1
                    print(f"  ✅ Converted: {rel_path}")
                else:
                    failed += 1
                    print(f"  ❌ Failed: {rel_path}")
            
            print()
            print(f"✅ Conversion complete: {converted} converted, {failed} failed")
            
            # Re-check after conversion
            print("\n🔍 Verifying conversions...")
            still_non_utf8 = []
            for rel_path, _, _ in non_utf8_files:
                full_path = os.path.join(str(root_dir), rel_path)
                new_encoding, new_confidence = detect_file_encoding(full_path)
                if new_encoding and new_encoding.lower() not in ['utf-8', 'utf8', 'ascii']:
                    still_non_utf8.append((rel_path, new_encoding, new_confidence))
            
            if still_non_utf8:
                print(f"⚠️  Warning: {len(still_non_utf8)} files still not UTF-8:")
                for rel_path, enc, conf in still_non_utf8:
                    print(f"  - {rel_path} ({enc})")
            else:
                print("✅ All files successfully converted to UTF-8!")
        else:
            print("\n💡 To convert these files, run:")
            print(f"   python {sys.argv[0]} --convert{' --ext ' + args.ext if args.ext else ''}")
    
    if unknown_files:
        print("\n❓ Files with unknown encoding (possibly empty or binary):")
        for rel_path in unknown_files[:20]:  # Show first 20
            print(f"  - {rel_path}")
        if len(unknown_files) > 20:
            print(f"  ... and {len(unknown_files) - 20} more")
    
    print()

if __name__ == '__main__':
    main()
