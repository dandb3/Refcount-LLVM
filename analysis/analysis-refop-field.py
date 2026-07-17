import glob
from collections import defaultdict
import sys

# 1. 데이터를 저장할 딕셔너리(사전) 생성
#    defaultdict(list)는 키가 없을 때 자동으로 빈 리스트를 생성해 줍니다.
target_map = defaultdict(list)

# 2. 현재 처리 중인 <Target>을 추적할 변수
current_target = None

# 3. glob을 사용해 현재 디렉터리에서 모든 *.refopfieldmap.log 파일을 찾습니다.
file_list = glob.glob("../log/linux/*.refopfieldmap.log")

if not file_list:
    print("오류: '*.refopfieldmap.log' 파일을 찾을 수 없습니다.", file=sys.stderr)
    sys.exit(1)

print(f"총 {len(file_list)}개의 파일을 처리합니다...")

# 4. 찾은 파일들을 하나씩 엽니다.
for filepath in file_list:
    with open(filepath, 'r') as f:
        # 5. 파일을 한 줄씩 읽습니다.
        for line in f:
            # 오른쪽 끝의 공백/줄바꿈만 제거 (들여쓰기 유지를 위해)
            line = line.rstrip() 
            
            # 빈 줄은 건너뜁니다.
            if not line.strip():
                continue

            # 6. 들여쓰기 확인
            if line.startswith(' ') or line.startswith('\t'):
                # 들여쓰기가 있으면 <Elem> 라인입니다.
                if current_target:
                    # 현재 <Target>의 리스트에 이 <Elem>을 추가합니다.
                    target_map[current_target].append(line)
                else:
                    # <Target>이 정의되기 전에 <Elem>이 나온 경우 (예외 처리)
                    print(f"경고: {filepath} 파일에 <Target>이 없는 <Elem> 발견: {line}", file=sys.stderr)
            else:
                # 들여쓰기가 없으면 <Target> 라인입니다.
                # 현재 <Target>을 이 라인으로 교체합니다.
                current_target = line
                # defaultdict이므로, 이 <Target> 키에 대한 리스트가 아직 없었다면
                # 자동으로 생성됩니다. (elem이 없어도 타겟 이름은 출력됨)
                _ = target_map[current_target] 

print("데이터 취합 완료. 'result.refop-field.log' 파일로 저장 중...")

# 7. 취합된 결과를 파일로 씁니다.
output_filename = "./result.refop-field.log"
with open(output_filename, 'w') as out_f:
    # 8. <Target> 이름을 기준으로 알파벳순으로 정렬하여 출력합니다.
    for target in sorted(target_map.keys()):
        out_f.write(f"{target}\n")
        
        elems = target_map[target]
        
        if elems:
            # --- (선택) Elem 중복 제거 ---
            # 만약 <Elem> 리스트에서 중복을 제거하고 싶다면,
            # 아래 3줄의 주석을 풀고, 그 아래 'for elem in elems:' 줄을 주석 처리하세요.
            
            # seen = set()
            # unique_elems = [e for e in elems if not (e in seen or seen.add(e))]
            # for elem in unique_elems:
            
            # --- (기본) 모든 Elem 출력 (중복 포함) ---
            for elem in elems:
                out_f.write(f"{elem}\n")
        
        # <Target> 블록 사이에 빈 줄을 넣어 구분합니다.
        out_f.write("\n")

print(f"# of callsites: {len(target_map.keys())}")
print(f"완료! 결과가 {output_filename} 파일에 저장되었습니다.")