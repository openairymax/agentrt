#!/usr/bin/env python3
"""Combine all gen_v3_p*.py into a single dataset_v3.jsonl file."""

import json
import sys
import os
from pathlib import Path

# Import all parts
sys.path.insert(0, str(Path(__file__).parent))
from gen_v3_p1 import entries as p1
from gen_v3_p2 import entries as p2
from gen_v3_p3 import entries as p3
from gen_v3_p4 import entries as p4
from gen_v3_p5 import entries as p5
from gen_v3_p6 import entries as p6
from gen_v3_p7 import entries as p7
from gen_v3_p8 import entries as p8

all_entries = p1 + p2 + p3 + p4 + p5 + p6 + p7 + p8

output_path = Path(__file__).parent / "dataset_v3.jsonl"
with open(output_path, "w", encoding="utf-8") as f:
    for entry in all_entries:
        f.write(json.dumps(entry, ensure_ascii=False) + "\n")

# Verify
easy = sum(1 for e in all_entries if e["difficulty"] == "easy")
medium = sum(1 for e in all_entries if e["difficulty"] == "medium")
hard = sum(1 for e in all_entries if e["difficulty"] == "hard")

print(f"dataset_v3.jsonl generated: {len(all_entries)} entries")
print(f"  Easy: {easy}, Medium: {medium}, Hard: {hard}")

# P3.11.1 requires >= 200 entries
if len(all_entries) >= 200:
    print(f"  Status: PASS (>= 200)")
else:
    print(f"  Status: NEED {200 - len(all_entries)} MORE (target: 200)")