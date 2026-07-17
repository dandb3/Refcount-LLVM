#!/usr/bin/env python3
"""build.sh (KernelBitcode.go 산출물) 의 clang 컴파일 줄만 병렬 실행한다.

llvm-link 줄은 .bc 를 합칠 뿐 refcount 분석과 무관하므로 제외한다.
제외하면 줄 사이 의존성이 사라져 전부 동시에 돌릴 수 있다.
refcount 로그는 clang 이 컴파일 중에 REFID_LOG_DIR 로 직접 떨군다.

사용법:  ./run_bc_parallel.py [-j 32] [--dry-run]
"""

import argparse
import os
import re
import subprocess
import sys
import threading
from concurrent.futures import ThreadPoolExecutor

# build.sh 의 명령은 "... -o <경로>.bc <경로>.c" 로 끝난다.
SRC_RE = re.compile(r"(\S+\.c)\s*$")

lock = threading.Lock()
done = 0
total = 0
failed = []           # (소스, 출력) — 컴파일 실패
aborted = []          # (소스, 출력) — refcount 패스가 exit(1)
running = {}          # 실행 중인 Popen. Ctrl+C 때 죽이려고 들고 있는다.
stopping = False

# refcount 패스가 exit(1) 로 중단될 때 남기는 메시지. 걸리면 그 파일은 로그가 없다.
ABORT_SIGNS = (
    "anonymous struct has too many parents",
    "anonymous struct has no parents",
    "StructType::getTypeByName",
    "RefOpID Not Exist",
    "MDNode Not Exist",
    "Metadata Not Set",
)


def source_of(cmd):
    m = SRC_RE.search(cmd)
    return m.group(1) if m else cmd[-60:]


def run_one(item):
    global done
    idx, cmd = item
    if stopping:
        return

    src = source_of(cmd)
    try:
        p = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE,
                             stderr=subprocess.STDOUT, text=True)
        with lock:
            running[idx] = p
        out = p.communicate()[0] or ""
        rc = p.returncode
    except Exception as e:                      # noqa: BLE001
        out, rc = str(e), -1
    finally:
        with lock:
            running.pop(idx, None)

    with lock:
        done += 1
        if rc != 0:
            failed.append((src, out))
        if any(s in out for s in ABORT_SIGNS):
            aborted.append((src, out))
        # 진행률은 한 줄에 덮어쓴다. 실패는 그 위에 남긴다.
        if rc != 0:
            print(f"\r{'':<100}\rFAIL {src}", flush=True)
        pct = done * 100 // total
        print(f"\r[{done}/{total}] {pct:3d}%  {src[:70]:<70}", end="", flush=True)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("-j", "--jobs", type=int, default=32)
    ap.add_argument("--build-sh", default=None)
    ap.add_argument("--dry-run", action="store_true", help="추출만 하고 실행하지 않는다")
    args = ap.parse_args()

    kdir = os.path.dirname(os.path.abspath(__file__))
    build_sh = args.build_sh or os.path.join(kdir, "build.sh")
    if not os.path.isfile(build_sh):
        sys.exit(f"ERROR: build.sh 가 없습니다: {build_sh}")

    # build.sh 안의 명령이 -I./arch/... 처럼 상대경로를 쓰므로 커널 트리에서 실행해야 한다.
    os.chdir(kdir)

    with open(build_sh, encoding="utf-8", errors="replace") as f:
        cmds = [ln.strip() for ln in f if "-emit-llvm" in ln]

    global total
    total = len(cmds)
    if total == 0:
        sys.exit("ERROR: build.sh 에 -emit-llvm 줄이 없습니다")

    print(f"대상 {total} 개, 병렬 {args.jobs}, cwd {kdir}")
    if args.dry_run:
        for c in cmds[:3]:
            print(f"  {source_of(c)}")
        print(f"  ... (총 {total})")
        return

    ex = ThreadPoolExecutor(max_workers=args.jobs)
    futures = [ex.submit(run_one, it) for it in enumerate(cmds)]
    try:
        for fu in futures:
            fu.result()
        ex.shutdown()
    except KeyboardInterrupt:
        global stopping
        stopping = True
        print("\n중단 중... 실행 중인 clang 을 종료합니다")
        with lock:
            for p in running.values():
                p.kill()
        ex.shutdown(wait=False, cancel_futures=True)
        print(f"[{done}/{total}] 에서 중단됨")
        sys.exit(130)

    print(f"\r{'':<100}\r완료 [{done}/{total}]")
    print(f"  컴파일 실패     : {len(failed)}")
    print(f"  패스 abort      : {len(aborted)}")

    for src, out in aborted[:5]:
        sign = next((s for s in ABORT_SIGNS if s in out), "?")
        print(f"    [abort] {src}: {sign}")

    if failed:
        print("\n실패 상위:")
        for src, out in failed[:5]:
            first = next((l for l in out.splitlines() if "error:" in l), out.splitlines()[0] if out else "")
            print(f"  {src}\n    {first[:110]}")

    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
