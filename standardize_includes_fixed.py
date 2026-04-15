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

def process_file(filepath):
    """Process a single file, replacing header includes."""
    try:
        content = filepath.read_text(encoding='utf-8')
        original = content
        changed = False
        
        # Replace specific header includes
        for old, new in HEADER_MAPPINGS.items():
            if old in content:
                content = content.replace(old, new)
                changed = True
        
        if changed:
            # Write updated content
            filepath.write_text(content, encoding='utf-8')
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
    
    return 0

if __name__ == '__main__':
    sys.exit(main())