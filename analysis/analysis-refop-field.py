import glob
from collections import defaultdict
import sys

# 1. Create the dictionary that stores the data.
#    defaultdict(list) automatically creates an empty list when a key is missing.
target_map = defaultdict(list)

# 2. Variable tracking the <Target> currently being processed.
current_target = None

# 3. Use glob to find every *.refopfieldmap.log file.
file_list = glob.glob("../log/linux/*.refopfieldmap.log")

if not file_list:
    print("Error: no '*.refopfieldmap.log' files found.", file=sys.stderr)
    sys.exit(1)

print(f"Processing {len(file_list)} files...")

# 4. Open the found files one by one.
for filepath in file_list:
    with open(filepath, 'r') as f:
        # 5. Read the file line by line.
        for line in f:
            # Strip only trailing whitespace/newline (keep leading indentation).
            line = line.rstrip()

            # Skip blank lines.
            if not line.strip():
                continue

            # 6. Check the indentation.
            if line.startswith(' ') or line.startswith('\t'):
                # Indented lines are <Elem> lines.
                if current_target:
                    # Append this <Elem> to the current <Target>'s list.
                    target_map[current_target].append(line)
                else:
                    # An <Elem> appeared before any <Target> was defined (edge case).
                    print(f"Warning: <Elem> without a <Target> in {filepath}: {line}", file=sys.stderr)
            else:
                # Non-indented lines are <Target> lines.
                # Replace the current <Target> with this line.
                current_target = line
                # Because target_map is a defaultdict, the list for this <Target>
                # key is created automatically if it did not exist yet
                # (so the target name is emitted even without any elem).
                _ = target_map[current_target]

print("Aggregation complete. Saving to 'result.refop-field.log'...")

# 7. Write the aggregated result to a file.
output_filename = "./result.refop-field.log"
with open(output_filename, 'w') as out_f:
    # 8. Output sorted alphabetically by <Target> name.
    for target in sorted(target_map.keys()):
        out_f.write(f"{target}\n")

        elems = target_map[target]

        if elems:
            # --- (optional) deduplicate Elems ---
            # To remove duplicates from the <Elem> list, uncomment the 3 lines
            # below and comment out the 'for elem in elems:' line beneath them.

            # seen = set()
            # unique_elems = [e for e in elems if not (e in seen or seen.add(e))]
            # for elem in unique_elems:

            # --- (default) output every Elem (duplicates included) ---
            for elem in elems:
                out_f.write(f"{elem}\n")

        # Insert a blank line between <Target> blocks as a separator.
        out_f.write("\n")

print(f"# of callsites: {len(target_map.keys())}")
print(f"Done! Result saved to {output_filename}.")
