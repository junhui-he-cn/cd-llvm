# CD VM Integration Boundary Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox syntax for tracking.

**Goal:** Add a reproducible, explicitly opt-in LLVM lit entry point for the Rust CD VM without discovering or mutating the sibling checkout by default.

**Architecture:** The CD directory will recognize one Python lit test and expose a cd-vm feature only when the caller supplies CD_COMPILER_ROOT. The test delegates artifact emission, dump, link, run, graph rejection, and linked diagnostics to the existing llvm/utils/cd_module_link.py harness; LLVM-only tests remain usable with the environment variable absent. A verification document records the current toolchain, cdbc 0.1 boundary, and exact commands without changing the default LLVM build or artifact version.

**Tech Stack:** LLVM lit, Python 3, LLVM llc, Rust Cargo VM, Markdown.

---

### Task 1: Define the opt-in integration test

**Files:**
- Create: llvm/test/CodeGen/CD/cdbc-vm-integration.py

- [x] **Step 1: Write the failing test contract**

Create a Python lit test whose RUN line executes the test itself. It must fail
with a direct diagnostic when CD_COMPILER_ROOT is absent, resolve the existing
module-link harness from the LLVM source tree, require
$CD_COMPILER_ROOT/vm-rs, and preserve the harness's exact direct/machine
success output:

~~~python
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
~~~

- [x] **Step 2: Run the script without the opt-in path**

Run:

~~~bash
env -u CD_COMPILER_ROOT python3 llvm/test/CodeGen/CD/cdbc-vm-integration.py
~~~

Expected: exit 2 with the explicit missing environment diagnostic; the test
must not search for cd-compiler itself.

- [x] **Step 3: Run the test with a valid explicit path after lit wiring**

Run:

~~~bash
CD_COMPILER_ROOT="$PWD/cd-compiler" build-cd/bin/llvm-lit -sv llvm/test/CodeGen/CD/cdbc-vm-integration.py
~~~

Expected: one passed test and the two exact harness output lines.

### Task 2: Wire the explicit environment into lit

**Files:**
- Create: llvm/test/CodeGen/CD/lit.local.cfg

- [x] **Step 1: Add only the opt-in feature and environment propagation**

~~~python
import os


config.suffixes.add(".py")

compiler_root = os.environ.get("CD_COMPILER_ROOT")
if compiler_root:
    config.available_features.add("cd-vm")
    config.environment["CD_COMPILER_ROOT"] = compiler_root
~~~

This must not set a default path or inspect the sibling directory.

- [x] **Step 2: Verify the default LLVM-only boundary**

Run:

~~~bash
env -u CD_COMPILER_ROOT build-cd/bin/llvm-lit -sv llvm/test/CodeGen/CD
~~~

Expected: all LLVM-only CD tests pass and cdbc-vm-integration.py is reported
unsupported because cd-vm is unavailable.

- [x] **Step 3: Verify the explicit VM boundary**

Run:

~~~bash
CD_COMPILER_ROOT="$PWD/cd-compiler" build-cd/bin/llvm-lit -sv llvm/test/CodeGen/CD/cdbc-vm-integration.py
~~~

Expected: the test passes and uses only the supplied CD_COMPILER_ROOT.

### Task 3: Publish the reproducible verification contract

**Files:**
- Create: docs/cd-bytecode-verification.md
- Modify: llvm/lib/Target/CD/README.md
- Modify: docs/superpowers/plans/2026-07-31-cd-bytecode-roadmap.md

- [x] **Step 1: Record the current toolchain and format boundary**

The verification document must record these observed values and explain that
the artifact contract remains cdbc 0.1:

~~~text
LLVM llc: LLVM 24.0.0git
LLVM lit: lit 24.0.0dev
Rust: rustc 1.94.1 (e408947bf 2026-03-25)
Cargo: cargo 1.94.1 (29ea6fb6a 2026-03-24)
VM checkout: 0295380ce3e29763949c09a815bda96cbed28ee2
~~~

It must include the no-sibling LLVM-only command, the explicit
CD_COMPILER_ROOT integration command, the standalone module-link harness
command, focused/full lit commands, Python unit tests, and git diff --check.

- [x] **Step 2: Document the explicit boundary in the target README**

Add the focused lit command with CD_COMPILER_ROOT="$PWD/cd-compiler" and state
that an absent variable leaves the integration test unsupported rather than
selecting a checkout implicitly.

- [x] **Step 3: Mark only the delivered M7 checks**

Change the roadmap M7 status to in-progress, mark LLVM-only execution and
explicit opt-in integration complete, and leave CI-job creation and the
broader release matrix unchecked.

### Task 4: Verify, review, and commit

- [x] **Step 1: Run the focused and full gates**

Run ninja -C build-cd llc, both no-sibling and explicit-sibling lit commands,
the module-link Python unit tests, the existing standalone module-link harness,
and git diff --check.

- [x] **Step 2: Review the exact diff**

Confirm that only the five M7 implementation files plus the plan are changed, the nested
cd-compiler repository is not staged, and no default compilation or cdbc 0.1
behavior changed.

- [x] **Step 3: Commit the slice**

~~~bash
git add docs/cd-bytecode-verification.md \
  docs/superpowers/plans/2026-07-31-cd-bytecode-roadmap.md \
  docs/superpowers/plans/2026-08-02-cd-vm-integration.md \
  llvm/lib/Target/CD/README.md \
  llvm/test/CodeGen/CD/cdbc-vm-integration.py \
  llvm/test/CodeGen/CD/lit.local.cfg
git commit -m "test(cd): add opt-in VM integration gate"
~~~

**Spec coverage:** This plan covers the explicit CD_COMPILER_ROOT path,
LLVM-only operation without the sibling checkout, exact version/command
recording, and the focused integration gate. CI provider configuration and the
full M7 release matrix remain intentionally outside this slice.
