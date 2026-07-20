#!/usr/bin/env python3
"""Run only the clang compile lines of build.sh (a KernelBitcode.go output) in parallel.

The llvm-link lines merely merge .bc files and are irrelevant to refcount
analysis, so they are skipped. Dropping them removes the inter-line
dependencies, so everything can run concurrently. The refcount logs are
written directly to REFID_LOG_DIR by clang during compilation.

Usage:  ./run_bc_parallel.py [-j 32] [--dry-run]
"""

import argparse
import os
import re
import subprocess
import sys
import threading
from concurrent.futures import ThreadPoolExecutor

# Commands in build.sh end with "... -o <path>.bc <path>.c".
SRC_RE = re.compile(r"(\S+\.c)\s*$")

lock = threading.Lock()
done = 0
total = 0
failed = []           # (source, output) — compilation failed
aborted = []          # (source, output) — refcount pass called exit(1)
running = {}          # running Popen objects, kept so we can kill them on Ctrl+C
stopping = False

# Messages the refcount pass leaves when it aborts with exit(1). If hit, that file has no log.
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
        # Overwrite the progress on a single line. Failures are left above it.
        if rc != 0:
            print(f"\r{'':<100}\rFAIL {src}", flush=True)
        pct = done * 100 // total
        print(f"\r[{done}/{total}] {pct:3d}%  {src[:70]:<70}", end="", flush=True)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("-j", "--jobs", type=int, default=32)
    ap.add_argument("--build-sh", default=None)
    ap.add_argument("--dry-run", action="store_true", help="extract only, do not run")
    args = ap.parse_args()

    kdir = os.path.dirname(os.path.abspath(__file__))
    build_sh = args.build_sh or os.path.join(kdir, "build.sh")
    if not os.path.isfile(build_sh):
        sys.exit(f"ERROR: build.sh not found: {build_sh}")

    # Commands in build.sh use relative paths like -I./arch/..., so run from the kernel tree.
    os.chdir(kdir)

    with open(build_sh, encoding="utf-8", errors="replace") as f:
        cmds = [ln.strip() for ln in f if "-emit-llvm" in ln]

    global total
    total = len(cmds)
    if total == 0:
        sys.exit("ERROR: no -emit-llvm lines in build.sh")

    print(f"targets {total}, jobs {args.jobs}, cwd {kdir}")
    if args.dry_run:
        for c in cmds[:3]:
            print(f"  {source_of(c)}")
        print(f"  ... ({total} total)")
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
        print("\nStopping... terminating running clang processes")
        with lock:
            for p in running.values():
                p.kill()
        ex.shutdown(wait=False, cancel_futures=True)
        print(f"Interrupted at [{done}/{total}]")
        sys.exit(130)

    print(f"\r{'':<100}\rdone [{done}/{total}]")
    print(f"  compile failed  : {len(failed)}")
    print(f"  pass abort      : {len(aborted)}")

    for src, out in aborted[:5]:
        sign = next((s for s in ABORT_SIGNS if s in out), "?")
        print(f"    [abort] {src}: {sign}")

    if failed:
        print("\nTop failures:")
        for src, out in failed[:5]:
            first = next((l for l in out.splitlines() if "error:" in l), out.splitlines()[0] if out else "")
            print(f"  {src}\n    {first[:110]}")

    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
