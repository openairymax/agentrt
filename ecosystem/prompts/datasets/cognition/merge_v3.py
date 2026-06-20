import json
import os
import sys

# Set working directory to the script location
os.chdir(os.path.dirname(os.path.abspath(__file__)))

# Import all part scripts
from gen_v3_p1 import entries as p1
from gen_v3_p2 import entries as p2
from gen_v3_p3 import entries as p3
from gen_v3_p4 import entries as p4
from gen_v3_p5 import entries as p5
from gen_v3_p6 import entries as p6
from gen_v3_p7 import entries as p7
from gen_v3_p8 import entries as p8
from gen_v3_p9 import entries as p9
from gen_v3_p10 import entries as p10

all_entries = p1 + p2 + p3 + p4 + p5 + p6 + p7 + p8 + p9 + p10

# Separate by difficulty
easy_entries = [e for e in all_entries if e["difficulty"] == "easy"]
medium_entries = [e for e in all_entries if e["difficulty"] == "medium"]
hard_entries = [e for e in all_entries if e["difficulty"] == "hard"]

print(f"Available: Easy={len(easy_entries)}, Medium={len(medium_entries)}, Hard={len(hard_entries)}")

# Target: 190 entries with ~40% easy, ~35% medium, ~25% hard
target_easy = 76
target_medium = 67
target_hard = 47

# Take all easy entries (we have exactly 76)
selected_easy = easy_entries[:target_easy]

# Take all medium entries (we have exactly 67)
selected_medium = medium_entries[:target_medium]

# Take hard entries: prioritize p1-p8 hard entries, then fill from p9
# p1-p8 hard entries should come first in the combined list
p1_to_p8_hard = [e for e in (p1 + p2 + p3 + p4 + p5 + p6 + p7 + p8) if e["difficulty"] == "hard"]
p9_hard = [e for e in p9 if e["difficulty"] == "hard"]

# Count how many hard entries from p1-p8
p1_p8_hard_count = len(p1_to_p8_hard)
print(f"Hard from p1-p8: {p1_p8_hard_count}")

# We need target_hard total hard entries
# Take all from p1-p8, then fill remaining from p9
remaining_hard = target_hard - p1_p8_hard_count
if remaining_hard < 0:
    selected_hard = p1_to_p8_hard[:target_hard]
else:
    selected_hard = p1_to_p8_hard + p9_hard[:remaining_hard]

print(f"Selected hard: {len(selected_hard)} (p1-p8: {min(p1_p8_hard_count, target_hard)}, p9: {max(0, remaining_hard)})")

# Combine all selected entries
# Shuffle within each difficulty to mix domains, then concatenate
import random
random.seed(42)
random.shuffle(selected_easy)
random.shuffle(selected_medium)
random.shuffle(selected_hard)

final_entries = selected_easy + selected_medium + selected_hard

print(f"Total entries: {len(final_entries)}")
print(f"Easy: {len(selected_easy)} ({len(selected_easy)/len(final_entries)*100:.1f}%)")
print(f"Medium: {len(selected_medium)} ({len(selected_medium)/len(final_entries)*100:.1f}%)")
print(f"Hard: {len(selected_hard)} ({len(selected_hard)/len(final_entries)*100:.1f}%)")

# ============================================================
# Validation
# ============================================================

# Validate all entries are valid JSON
for i, entry in enumerate(final_entries):
    try:
        json.dumps(entry)
    except Exception as err:
        print(f"ERROR at entry {i}: {err}")
        sys.exit(1)

# Validate each entry has required fields
for i, entry in enumerate(final_entries):
    for field in ["input", "expected_output", "category", "difficulty"]:
        if field not in entry:
            print(f"ERROR: entry {i} missing field '{field}'")
            sys.exit(1)
    if entry["category"] != "entity_extract":
        print(f"ERROR: entry {i} has wrong category: {entry['category']}")
        sys.exit(1)
    if entry["difficulty"] not in ("easy", "medium", "hard"):
        print(f"ERROR: entry {i} has invalid difficulty: {entry['difficulty']}")
        sys.exit(1)

# Validate expected_output entities
for i, entry in enumerate(final_entries):
    for j, ent in enumerate(entry["expected_output"]):
        for field in ["name", "type", "context", "attributes", "confidence"]:
            if field not in ent:
                print(f"ERROR: entry {i}, entity {j} missing field '{field}'")
                sys.exit(1)
        if not (0.85 <= ent["confidence"] <= 0.99):
            print(f"ERROR: entry {i}, entity {j} confidence out of range: {ent['confidence']}")
            sys.exit(1)

# Validate entity count per difficulty
for i, entry in enumerate(final_entries):
    n_entities = len(entry["expected_output"])
    diff = entry["difficulty"]
    if diff == "easy" and n_entities < 3:
        print(f"WARNING: easy entry {i} has only {n_entities} entities (need >= 3)")
    if diff == "medium" and n_entities < 5:
        print(f"WARNING: medium entry {i} has only {n_entities} entities (need >= 5)")
    if diff == "hard" and n_entities < 7:
        print(f"WARNING: hard entry {i} has only {n_entities} entities (need >= 7)")

# Write JSONL file
output_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "dataset_v3.jsonl")
with open(output_path, "w", encoding="utf-8") as f:
    for entry in final_entries:
        f.write(json.dumps(entry, ensure_ascii=False) + "\n")

print(f"\nWritten to: {output_path}")

# Verify the file can be read back
with open(output_path, "r", encoding="utf-8") as f:
    lines = f.readlines()
    for i, line in enumerate(lines):
        line = line.strip()
        if not line:
            continue
        try:
            json.loads(line)
        except json.JSONDecodeError as err:
            print(f"ERROR: Line {i+1} is not valid JSON: {err}")
            sys.exit(1)

print(f"Verified: {len(lines)} lines, all valid JSON")
print("Done!")