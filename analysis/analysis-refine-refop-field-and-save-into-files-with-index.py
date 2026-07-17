#!/usr/bin/env python3

import sys
import os
from typing import Set, List, Tuple


def load_valid_fields(log_b_file: str) -> Set[str]:
    """
    로그 B (허용 목록) 파일을 읽어 유효한 field 목록을 set으로 반환합니다.
    (기존 로직 유지)
    """
    valid_fields = set()
    try:
        with open(log_b_file, 'r', encoding='utf-8') as f:
            for line in f:
                stripped_line = line.strip()
                if stripped_line:
                    valid_fields.add(stripped_line)
    except FileNotFoundError:
        print(f"오류: {log_b_file} 파일을 찾을 수 없습니다.", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(f"오류: {log_b_file} 파일 읽기 중 오류 발생: {e}", file=sys.stderr)
        sys.exit(1)

    return valid_fields


def collect_filtered_entries(
    log_a_file: str,
    valid_fields: Set[str],
) -> List[Tuple[str, str, List[str]]]:
    """
    1차 단계: log_a_file을 순회하면서,
      - 각 Target (filepath:func_sig) 블록에서 valid field가 하나라도 있으면
      - (filepath, func_sig, [valid_fields...]) 튜플로 entries 리스트에 저장한다.

    여기서는 아직 index를 부여하지 않는다. (global index는 이 리스트를 만든 뒤에 일괄 부여)
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

                # Target 라인
                if not is_indented and stripped_line:
                    # 이전 블록 저장 여부 판단
                    if current_filepath and current_valid_values:
                        # valid field가 하나 이상 있다면 entries에 추가
                        entries.append(
                            (current_filepath,
                             current_func_sig if current_func_sig else "unknown",
                             current_valid_values)
                        )

                    # 새 블록 초기화
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

                # Field 라인
                elif is_indented and stripped_line:
                    if stripped_line in valid_fields:
                        current_valid_values.append(stripped_line)

                # 빈 줄
                elif not stripped_line:
                    pass

            # 파일 끝에 도달했을 때 마지막 블록 처리
            if current_filepath and current_valid_values:
                entries.append(
                    (current_filepath,
                     current_func_sig if current_func_sig else "unknown",
                     current_valid_values)
                )

    except FileNotFoundError:
        print(f"오류: {log_a_file} 파일을 찾을 수 없습니다.", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(f"파일 처리 중 오류 발생: {e}", file=sys.stderr)
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
    2차 단계에서 호출되는 함수.
    파일명: filepath의 '/'를 '+'로 변경.
    내용 포맷: FuncSig----Val1,Val2,...----Index
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
        print(f"오류: {safe_filename} 파일 쓰기 실패: {e}", file=sys.stderr)


def process_and_split_files_with_global_index(
    log_a_file: str,
    valid_fields: Set[str],
    output_dir: str,
) -> None:
    """
    전체 로직:
      1) log_a_file에서 valid field를 가진 entry들만 모아서 entries 리스트로 만든다.
      2) entries를 순회하면서 global index(0부터)를 부여하고,
         filepath 기준으로 파일을 나누어 저장한다.
    """

    if not os.path.isdir(output_dir):
        print(
            f"오류: 출력 디렉토리 '{output_dir}'가 존재하지 않습니다. 디렉토리를 미리 생성해주세요.",
            file=sys.stderr,
        )
        sys.exit(1)

    # 1차: 필터링 및 entry 수집
    entries = collect_filtered_entries(log_a_file, valid_fields)
    total = len(entries)
    print(f"   -> 필터링된 entry 개수: {total}", file=sys.stderr)

    # 2차: global index 부여 + 파일로 저장
    for idx, (filepath, func_sig, valid_vals) in enumerate(entries):
        write_entry_with_index(
            output_dir=output_dir,
            filepath_part=filepath,
            func_sig_part=func_sig,
            valid_values=valid_vals,
            index=idx,  # global scope index
        )


# --- 메인 실행 로직 ---
if __name__ == "__main__":

    if len(sys.argv) != 4:
        print("사용법: python split_filter.py <로그_A_파일> <로그_B_파일> <출력_디렉토리>",
              file=sys.stderr)
        print("  예: python split_filter.py log_a.txt log_b.txt ./output_result/",
              file=sys.stderr)
        sys.exit(1)

    log_a_file = sys.argv[1]
    log_b_file = sys.argv[2]
    output_dir = sys.argv[3]

    print(f"1. 유효 필드 로드 중 (from {log_b_file})...", file=sys.stderr)
    valid_fields_set = load_valid_fields(log_b_file)
    print(f"   -> 총 {len(valid_fields_set)}개의 유효 필드 로드 완료.", file=sys.stderr)

    print(f"2. 로그 필터링 및 인덱스 부여 후 분할 저장 중 (to {output_dir})...",
          file=sys.stderr)
    process_and_split_files_with_global_index(log_a_file, valid_fields_set, output_dir)

    print(f"\n✅ 작업 완료! 결과가 '{output_dir}' 디렉토리에 저장되었습니다.", file=sys.stderr)