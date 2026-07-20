#!/usr/bin/env python3

import sys
import os
from typing import Set, List, Tuple


def load_valid_fields(log_b_file: str) -> Set[str]:
    """
    Read log B (the allow list) and return the set of valid fields.
    (unchanged logic)
    """
    valid_fields = set()
    try:
        with open(log_b_file, 'r', encoding='utf-8') as f:
            for line in f:
                stripped_line = line.strip()
                if stripped_line:
                    valid_fields.add(stripped_line)
    except FileNotFoundError:
        print(f"Error: {log_b_file} not found.", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(f"Error: failure while reading {log_b_file}: {e}", file=sys.stderr)
        sys.exit(1)

    return valid_fields


def collect_filtered_entries(
    log_a_file: str,
    valid_fields: Set[str],
) -> List[Tuple[str, str, List[str]]]:
    """
    First pass: iterate over log_a_file and, for each Target
    (filepath:func_sig) block that has at least one valid field,
    store a (filepath, func_sig, [valid_fields...]) tuple in the entries list.

    Indices are not assigned here (the global index is assigned in bulk after
    this list is built).
    """
    entries: List[Tuple[str, str, List[str]]] = []

    try:
        with open(log_a_file, 'r', encoding='utf-8') as f_in:
            current_filepath = None
            current_func_sig = None
            current_valid_values: List[str] = []

            for line in f_in:
                stripped_line = line.strip()
                is_indented = line.startswith((' ', '\t'))

                # Target line
                if not is_indented and stripped_line:
                    # Decide whether to save the previous block
                    if current_filepath and current_valid_values:
                        # Add to entries if it has at least one valid field
                        entries.append(
                            (current_filepath,
                             current_func_sig if current_func_sig else "unknown",
                             current_valid_values)
                        )

                    # Reset for the new block
                    current_valid_values = []

                    parts = stripped_line.split(':', 1)
                    if len(parts) == 2:
                        current_filepath = parts[0]
                        current_func_sig = parts[1]
                    else:
                        current_filepath = stripped_line
                        current_func_sig = "unknown"
                        print(f"Error: No colon in line! -> {stripped_line}",
                              file=sys.stderr)

                # Field line
                elif is_indented and stripped_line:
                    if stripped_line in valid_fields:
                        current_valid_values.append(stripped_line)

                # Blank line
                elif not stripped_line:
                    pass

            # Handle the last block when the end of file is reached
            if current_filepath and current_valid_values:
                entries.append(
                    (current_filepath,
                     current_func_sig if current_func_sig else "unknown",
                     current_valid_values)
                )

    except FileNotFoundError:
        print(f"Error: {log_a_file} not found.", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(f"Error while processing file: {e}", file=sys.stderr)
        sys.exit(1)

    return entries


def write_entry_with_index(
    output_dir: str,
    filepath_part: str,
    func_sig_part: str,
    valid_values: List[str],
    index: int,
) -> None:
    """
    Called during the second pass.
    Filename: filepath with '/' replaced by '+'.
    Content format: FuncSig----Val1,Val2,...----Index
    """
    if not valid_values:
        return

    safe_filename = filepath_part.replace('/', '+')
    full_output_path = os.path.join(output_dir, safe_filename)

    values_str = ",".join(valid_values)
    line = f"{func_sig_part}----{values_str}----{index}\n"

    try:
        with open(full_output_path, 'a', encoding='utf-8') as f:
            f.write(line)
    except Exception as e:
        print(f"Error: failed to write {safe_filename}: {e}", file=sys.stderr)


def process_and_split_files_with_global_index(
    log_a_file: str,
    valid_fields: Set[str],
    output_dir: str,
) -> None:
    """
    Full logic:
      1) Collect only the entries that have a valid field from log_a_file into
         the entries list.
      2) Iterate over entries assigning a global index (starting at 0) and
         split the output into files keyed by filepath.
    """

    if not os.path.isdir(output_dir):
        print(
            f"Error: output directory '{output_dir}' does not exist. Please create it first.",
            file=sys.stderr,
        )
        sys.exit(1)

    # First pass: filter and collect entries
    entries = collect_filtered_entries(log_a_file, valid_fields)
    total = len(entries)
    print(f"   -> filtered entry count: {total}", file=sys.stderr)

    # Second pass: assign global index + save to files
    for idx, (filepath, func_sig, valid_vals) in enumerate(entries):
        write_entry_with_index(
            output_dir=output_dir,
            filepath_part=filepath,
            func_sig_part=func_sig,
            valid_values=valid_vals,
            index=idx,  # global scope index
        )


# --- main entry point ---
if __name__ == "__main__":

    if len(sys.argv) != 4:
        print("Usage: python split_filter.py <log_A_file> <log_B_file> <output_dir>",
              file=sys.stderr)
        print("  e.g.: python split_filter.py log_a.txt log_b.txt ./output_result/",
              file=sys.stderr)
        sys.exit(1)

    log_a_file = sys.argv[1]
    log_b_file = sys.argv[2]
    output_dir = sys.argv[3]

    print(f"1. Loading valid fields (from {log_b_file})...", file=sys.stderr)
    valid_fields_set = load_valid_fields(log_b_file)
    print(f"   -> {len(valid_fields_set)} valid fields loaded.", file=sys.stderr)

    print(f"2. Filtering the log, assigning indices, and writing splits (to {output_dir})...",
          file=sys.stderr)
    process_and_split_files_with_global_index(log_a_file, valid_fields_set, output_dir)

    print(f"\nDone! Result saved to the '{output_dir}' directory.", file=sys.stderr)
