import json
import os

def replace_dot_slash(args, directory):
    new_args = []
    for arg in args:
        if arg.startswith("./"):
            new_arg = os.path.join(directory, arg[2:])
            # print(new_arg)
            new_args.append(new_arg)
        elif arg.startswith("-I./"):
            new_arg = "-I" + os.path.join(directory, arg[4:])
            new_args.append(new_arg)
        else:
            new_args.append(arg)
    return new_args

def process_compile_commands(input_file, output_file):
    # JSON 파일 읽기
    with open(input_file, 'r') as f:
        data = json.load(f)
    
    # 각 엔트리 처리
    modified_data = []
    for entry in data:
        print("directory:", entry["directory"])
        new_entry = {
            "arguments": replace_dot_slash(entry["arguments"], entry["directory"]),
            "directory": entry["directory"],
            "file": entry["file"]
        }
        modified_data.append(new_entry)
    
    # 수정된 내용 저장
    with open(output_file, 'w') as f:
        json.dump(modified_data, f, indent=4)

# 사용 예시
if __name__ == "__main__":
    input_json = "compile_commands.json"   # 원본 파일
    output_json = "compile_commands_modified.json"  # 수정된 파일
    
    process_compile_commands(input_json, output_json)
    print(f"finish!")
