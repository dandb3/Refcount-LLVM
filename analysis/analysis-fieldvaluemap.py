import glob
import sys

def create_unified_dictionary(file_pattern):
    """
    Read every log file matching the pattern and build a single unified dictionary.

    Return value (dict):
        {
            'id_string': ('type_string', {('op1', val1), ('op2', val2), ...}),
            ...
        }
    """
    # The unified dictionary that holds the final result.
    unified_data = {}

    # Find every file matching the given pattern.
    files = sorted(glob.glob(file_pattern))

    if not files:
        print(f"Error: no files match the pattern '{file_pattern}'.", file=sys.stderr)
        return unified_data

    print(f"Merging {len(files)} files.")

    # Process each file in order.
    for filename in files:
        print(f"  - processing: {filename}")
        try:
            with open(filename, 'r') as f:
                # Split the file into records (paragraphs) on blank lines.
                records = f.read().strip().split('\n\n')

                for record in records:
                    lines = record.strip().split('\n')

                    # A record needs at least 4 lines (ID, parent struct, type, op) to be valid.
                    # The parent struct line was added with the Closest-Named-Parent work (2026-01-19).
                    if len(lines) < 4:
                        print("ERROR: No Tuple Found!", file=sys.stderr)
                        continue

                    # Extract ID and type.
                    record_id = lines[0].strip()
                    record_parent = lines[1].strip()
                    record_type = lines[2].strip()

                    # Parse the operations into a set.
                    current_operations = set()
                    for op_line in lines[3:]:
                        try:
                            op, val = op_line.strip().split(',', 1)
                            current_operations.add((op, val))
                        except (ValueError, IndexError):
                            # Ignore lines that are not in 'op,val' form.
                            continue

                    # Merge the data into the unified dictionary.
                    if record_id in unified_data:
                        # If the ID already exists, check the type matches and union the op sets.
                        existing_type, existing_ops = unified_data[record_id]

                        if existing_type != record_type:
                            print(f"Warning: type mismatch for ID '{record_id}'. "
                                  f"existing '{existing_type}', new '{record_type}'", file=sys.stderr)

                        existing_ops.update(current_operations)
                    else:
                        # A new ID: add it to the dictionary.
                        unified_data[record_id] = (record_type, current_operations)

        except Exception as e:
            print(f"Error: failure while processing '{filename}': {e}", file=sys.stderr)

    return unified_data

def filter_dictionary_with_flags(data_dict):
    """
    Filter the given dictionary explicitly using 6 boolean flags.
    """
    filtered_dict = {
        "atomic_t": [],
        "atomic_long_t": [],
        "atomic64_t": [],
        "refcount_t": [],
        "kref": []
    }

    # Iterate over each item of the input dictionary.
    for key, (item_type, op_set) in data_dict.items():

        # --- Initialize the flags used to check the 6 conditions ---
        set_exist = False
        diff_positive_exist = False  # diff, value >= 0
        diff_negative_exist = False  # diff, value < 0
        set_only_one = True          # are all set values '1'? (meaningless if there is no set)
        diff_plus_one_exist = False  # diff, 1
        diff_minus_one_exist = False # diff, -1

        # Inspect every (op, val) tuple in the set to update the flags.
        for op, val_str in op_set:
            try:
                # Convert to int for the numeric comparison of the diff op.
                val_int = int(val_str)

                if op == 'set':
                    set_exist = True
                    # If any set op has a value other than '1', flip this to False.
                    if val_str != '1':
                        set_only_one = False

                elif op == 'diff':
                    if val_int >= 0:
                        diff_positive_exist = True
                    if val_int < 0:
                        diff_negative_exist = True
                    if val_str == '1':
                        diff_plus_one_exist = True
                    if val_str == '-1':
                        diff_minus_one_exist = True

            except (ValueError, IndexError):
                # Ignore lines not in 'op,val' form, or where val is not an integer.
                continue

        # --- Combine all flags to check the final condition ---
        if (set_exist and
            diff_positive_exist and
            diff_negative_exist and
            set_only_one and
            diff_plus_one_exist and
            diff_minus_one_exist):

            # If all conditions pass, add the current item to the filtered dictionary.
            filtered_dict[item_type].append(key)

    return filtered_dict

if __name__ == "__main__":
    # Path pattern of the files to analyze.
    log_file_pattern = "../log/linux/*.fieldvaluemap.log"

    # Call the main function to build the unified dictionary.
    final_dictionary = create_unified_dictionary(log_file_pattern)

    print("\n--- done ---")
    print(f"{len(final_dictionary)} unique IDs were unified.")

    # Write out the results.
    with open("result.fieldvaluemap.log", "w") as f:
        for key, value in final_dictionary.items():
            f.write(key + "\n")
            f.write(value[0] + "\n")
            for op in value[1]:
                f.write(op[0] + "," + op[1] + "\n")
            f.write("\n")

    stat_dictionary = filter_dictionary_with_flags(final_dictionary)
    for key, value in stat_dictionary.items():
        print(f"{key}: {len(value)}")

    with open("result.refcount-field.log", "w") as f:
        for key, value in stat_dictionary.items():
            for item in value:
                f.write(item + "\n")

    # for key, value in stat_dictionary.items():
    #     print(f"<{key}>")
    #     print(f"    {value}")
