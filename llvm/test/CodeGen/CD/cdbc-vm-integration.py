# REQUIRES: cd-vm
# RUN: %python %s

import os
import subprocess
import sys
from pathlib import Path


def main():
    compiler_root = os.environ.get("CD_COMPILER_ROOT")
    if not compiler_root:
        print("CD_COMPILER_ROOT is required for CD VM integration", file=sys.stderr)
        return 2

    vm_root = Path(compiler_root).resolve() / "vm-rs"
    if not (vm_root / "Cargo.toml").is_file():
        print(f"CD VM checkout is missing: {vm_root}", file=sys.stderr)
        return 2

    harness = Path(__file__).resolve().parents[3] / "utils" / "cd_module_link.py"
    result = subprocess.run(
        [sys.executable, str(harness), "--llc", "llc", "--vm", str(vm_root)],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if result.returncode != 0:
        sys.stderr.write(result.stderr)
        sys.stderr.write(result.stdout)
        return result.returncode
    if result.stdout != (
        "direct module-link integration: valid and rejected graphs\n"
        "machine module-link integration: valid and rejected graphs\n"
    ):
        print(f"unexpected harness output: {result.stdout!r}", file=sys.stderr)
        return 1
    if result.stderr:
        print(f"unexpected harness stderr: {result.stderr!r}", file=sys.stderr)
        return 1
    return 0


raise SystemExit(main())
