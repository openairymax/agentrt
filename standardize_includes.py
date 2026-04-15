#!/usr/bin/env python3
"""
Standardize AgentOS header includes across the codebase.
Replaces old compatibility header paths with new standard paths.
"""

import os
import re
import sys
from pathlib import Path

# Mapping from old include paths to new standard paths
HEADER_MAPPINGS = {
    # Compatibility headers to standard headers
    "<agentos/memory_compat.h>": "<agentos/memory.h>",
    "<agentos/string_compat.h>": "<agentos/string.h>",
    "<agentos/logging_compat.h>": "<agentos/logging.h>",
    "<agentos/platform_compat.h>": "<agentos/platform.h>",
    "<agentos/error_compat.h>": "<agentos/error.h>",
    "<agentos/types_compat.h>": "<agentos/types.h>",
    "<agentos/check_compat.h>": "<agentos/check.h>",
    
    # Old direct paths (if any) to standard paths
    "<agentos/memory.h>": "<agentos/memory.h>",  # Already standard
    "<agentos/logging.h>": "<agentos/logging.h>",
    "<agentos/platform.h>": "<agentos/platform.h>",
    "<agentos/error.h>": "<agentos/error.h>",
    "<agentos/types.h>": "<agentos/types.h>",
    "<agentos/check.h>": "<agentos/check.h>",
    "<agentos/string.h>": "<agentos/string.h>",
}

# Additional mappings for relative paths that should become standard
RELATIVE_MAPPINGS = {
    r'#include\s+"\.\./\.\./include/agentos/': '#include <agentos/',
    r'#include\s+"\.\./\.\./\.\./include/agentos/': '#include <agentos/',
}

def process_file(filepath):
    """Process a single file, replacing header includes."""
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()
        
        original = content
        changed = False
        
        # Replace relative includes with standard includes
        for pattern, replacement in RELATIVE_MAPPINGS.items():
            new_content = re.sub(pattern, replacement, content)
            if new_content != content:
                changed = True
                content = new_content
        
        # Replace specific header includes
        for old, new in HEADER_MAPPINGS.items():
            if old in content:
                content = content.replace(old, new)
                changed = True
        
        if changed:
            # Backup original
            backup = filepath + '.bak'
            with open(backup, 'w', encoding='utf-8') as f:
                f.write(original)
            
            # Write updated content
            with open(filepath, 'w', encoding='utf-8') as f:
                f.write(content)
            
            print(f"Updated: {filepath}")
            return True
    
    except Exception as e:
        print(f"Error processing {filepath}: {e}")
    
    return False

def main():
    project_root = Path(__file__).parent
    c_files = list(project_root.glob('**/*.c'))
    h_files = list(project_root.glob('**/*.h'))
    
    all_files = c_files + h_files
    
    print(f"Found {len(all_files)} files to process")
    
    updated_count = 0
    for filepath in all_files:
        # Skip files in .git directory and backup files
        if '.git' in str(filepath) or filepath.suffix == '.bak':
            continue
        
        if process_file(filepath):
            updated_count += 1
    
    print(f"\nUpdated {updated_count} files")
    
    # Create standard header files if they don't exist
    standard_headers = [
        'agentos/include/agentos/memory.h',
        'agentos/include/agentos/logging.h',
        'agentos/include/agentos/string.h',
        'agentos/include/agentos/platform.h',
        'agentos/include/agentos/error.h',
        'agentos/include/agentos/types.h',
        'agentos/include/agentos/check.h',
    ]
    
    for header in standard_headers:
        path = project_root / header
        if not path.exists():
            print(f"Warning: Standard header does not exist: {header}")
    
    return 0

if __name__ == '__main__':
    sys.exit(main())