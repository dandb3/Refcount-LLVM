import glob
import sys

def create_unified_dictionary(file_pattern):
    """
    주어진 패턴의 모든 로그 파일을 읽어 하나의 통합된 딕셔너리를 생성합니다.

    반환값 (dict):
        {
            'ID문자열': ('타입문자열', {('연산1', 값1), ('연산2', 값2), ...}),
            ...
        }
    """
    # 최종 결과를 저장할 통합 딕셔너리
    unified_data = {}
    
    # 주어진 패턴에 맞는 모든 파일 목록을 찾음
    files = sorted(glob.glob(file_pattern))
    
    if not files:
        print(f"오류: '{file_pattern}' 패턴에 해당하는 파일을 찾을 수 없습니다.", file=sys.stderr)
        return unified_data
        
    print(f"총 {len(files)}개의 파일을 병합합니다.")
    
    # 각 파일을 순서대로 처리
    for filename in files:
        print(f"  - 처리 중: {filename}")
        try:
            with open(filename, 'r') as f:
                # 파일을 빈 줄 기준으로 단락(레코드)으로 나눔
                records = f.read().strip().split('\n\n')
                
                for record in records:
                    lines = record.strip().split('\n')
                    
                    # 레코드는 최소 4줄(ID, 부모 struct, 타입, 연산) 이상이어야 유효함
                    # 부모 struct 줄은 Closest-Named-Parent 구현(2026-01-19) 때 추가되었다.
                    if len(lines) < 4:
                        print("ERROR: No Tuple Found!", file=sys.stderr)
                        continue

                    # ID와 타입 추출
                    record_id = lines[0].strip()
                    record_parent = lines[1].strip()
                    record_type = lines[2].strip()

                    # 연산(operation)들을 파싱하여 set으로 만듦
                    current_operations = set()
                    for op_line in lines[3:]:
                        try:
                            op, val = op_line.strip().split(',', 1)
                            current_operations.add((op, val))
                        except (ValueError, IndexError):
                            # 'op,val' 형식이 아닌 라인은 무시
                            continue
                    
                    # 통합 딕셔너리에 데이터 병합
                    if record_id in unified_data:
                        # ID가 이미 존재하면, 기존 타입과 일치하는지 확인하고 연산 set을 합침(union)
                        existing_type, existing_ops = unified_data[record_id]
                        
                        if existing_type != record_type:
                            print(f"경고: ID '{record_id}'의 타입 불일치. "
                                  f"기존 '{existing_type}', 신규 '{record_type}'", file=sys.stderr)
                        
                        existing_ops.update(current_operations)
                    else:
                        # 새로운 ID이면, 딕셔너리에 새로 추가
                        unified_data[record_id] = (record_type, current_operations)

        except Exception as e:
            print(f"오류: '{filename}' 파일 처리 중 에러 발생: {e}", file=sys.stderr)
            
    return unified_data

def filter_dictionary_with_flags(data_dict):
    """
    주어진 딕셔너리를 6개의 boolean flag를 사용해 명시적으로 필터링합니다.
    """
    filtered_dict = {
        "atomic_t": [],
        "atomic_long_t": [],
        "atomic64_t": [],
        "refcount_t": [],
        "kref": []
    }

    # 입력 딕셔너리의 각 아이템을 순회
    for key, (item_type, op_set) in data_dict.items():
        
        # --- 6가지 조건을 체크하기 위한 플래그 초기화 ---
        set_exist = False
        diff_positive_exist = False  # diff, value >= 0
        diff_negative_exist = False  # diff, value < 0
        set_only_one = True          # 모든 set의 값이 '1'인가? (set이 없으면 의미 없음)
        diff_plus_one_exist = False  # diff, 1
        diff_minus_one_exist = False # diff, -1

        # set 내부의 모든 (연산, 값) 튜플을 확인하여 플래그를 업데이트
        for op, val_str in op_set:
            try:
                # diff 연산의 숫자 비교를 위해 정수로 변환
                val_int = int(val_str)

                if op == 'set':
                    set_exist = True
                    # set 연산의 값이 '1'이 아닌 경우가 하나라도 발견되면 False로 변경
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
                # 'op,val' 형식이 아니거나, val이 정수가 아닌 경우는 무시
                continue

        # --- 모든 플래그를 조합하여 최종 조건 확인 ---
        if (set_exist and
            diff_positive_exist and
            diff_negative_exist and
            set_only_one and
            diff_plus_one_exist and
            diff_minus_one_exist):
            
            # 모든 조건을 통과했다면 필터링된 딕셔너리에 현재 아이템을 추가
            filtered_dict[item_type].append(key)

    return filtered_dict

if __name__ == "__main__":
    # 분석할 파일들의 경로 패턴을 지정합니다.
    log_file_pattern = "../log/linux/*.fieldvaluemap.log"
    
    # 메인 함수를 호출하여 통합 딕셔너리를 생성합니다.
    final_dictionary = create_unified_dictionary(log_file_pattern)
    
    print("\n--- 작업 완료 ---")
    print(f"총 {len(final_dictionary)}개의 고유한 ID가 통합되었습니다.")
    
    # 결과 확인을 위해 2개의 샘플 항목 출력
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