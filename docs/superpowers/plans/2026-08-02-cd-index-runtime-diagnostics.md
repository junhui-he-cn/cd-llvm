# CD Index Runtime Diagnostics Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Cover a source-backed out-of-range array index through both LLVM CD artifact paths and Rust VM runtime-error parity.

**Architecture:** Reuse the existing `llvm.cd.array` and `llvm.cd.index` lowering, explicit `!cd.sources`, and `DILocation` mapping. Add one focused fixture and a runtime-error parity entry; do not add an opcode, change the artifact grammar, or modify the nested checkout.

**Tech Stack:** LLVM 24 target emitter, LLVM lit/FileCheck, `cdbc 0.1`, `cd_bytecode_parity.py`, and the sibling Rust VM executable.

---

### Task 1: Add the invalid-index fixture

**Files:**
- Create: `llvm/test/CodeGen/CD/cdbc-debug-index-runtime.ll`

- [x] **Step 1: Add the direct/machine IR and debug-table checks**

Create the fixture with this exact input:

```llvm
; RUN: llc -mtriple=cd-unknown-unknown %s -o %t.direct
; RUN: FileCheck --check-prefix=DIRECT %s < %t.direct
; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o %t.machine
; RUN: FileCheck --check-prefix=MACHINE %s < %t.machine

declare ptr @llvm.cd.array(i32, ...)
declare ptr @llvm.cd.index(ptr, double)

define i32 @main() !dbg !5 {
entry:
  %array = call ptr (i32, ...) @llvm.cd.array(i32 1, i64 7)
  %value = call ptr @llvm.cd.index(ptr %array, double 1.0), !dbg !10
  ret i32 0, !dbg !11
}

!cd.sources = !{!20}
!20 = !{!"index.cd", !"let x = [7][1];\0A"}
!llvm.module.flags = !{!30}
!llvm.dbg.cu = !{!0}
!0 = distinct !DICompileUnit(language: DW_LANG_C, file: !1,
    producer: "cd", isOptimized: false, emissionKind: FullDebug)
!1 = !DIFile(filename: "index.cd", directory: "")
!2 = !{}
!3 = !DISubroutineType(types: !2)
!5 = distinct !DISubprogram(name: "main", scope: !1, file: !1, line: 1,
    type: !3, unit: !0, retainedNodes: !2)
!10 = !DILocation(line: 1, column: 14, scope: !5)
!11 = !DILocation(line: 1, column: 1, scope: !5)
!30 = !{i32 2, !"Debug Info Version", i32 3}

; DIRECT: debug_locations:
; DIRECT: main 3 = s0:1:14
; DIRECT: main 5 = s0:1:1
; MACHINE: debug_locations:
; MACHINE: main 3 = s0:1:14
; MACHINE: main 5 = s0:1:1
```

- [x] **Step 2: Run the focused fixture**

Run:

```sh
build-cd/bin/llvm-lit -sv llvm/test/CodeGen/CD/cdbc-debug-index-runtime.ll
```

Expected: `1` test passed.

### Task 2: Add index runtime-error parity

**Files:**
- Modify: `llvm/test/CodeGen/CD/cdbc-machine-parity.list`

- [x] **Step 1: Add the exact expected diagnostic**

Append:

```text
runtime-error llvm/test/CodeGen/CD/cdbc-debug-index-runtime.ll "Runtime error at index.cd:1:14: array index out of range"
```

- [x] **Step 2: Run the complete manifest**

Run:

```sh
python3 llvm/utils/cd_bytecode_parity.py \
  --llc build-cd/bin/llc \
  --vm cd-compiler/vm-rs/target/debug/compiler-design-vm \
  --manifest llvm/test/CodeGen/CD/cdbc-machine-parity.list
```

Expected: `38/38`, with identical direct/machine source-backed index errors.

### Task 3: Update the M5 boundary and previous plans

**Files:**
- Modify: `docs/cd-bytecode-llvm-abi.md`
- Modify: `docs/cd-bytecode-machine-backend.md`
- Modify: `llvm/lib/Target/CD/README.md`
- Modify: `docs/superpowers/plans/2026-07-31-cd-bytecode-roadmap.md`
- Modify: `docs/superpowers/plans/2026-08-02-cd-runtime-diagnostics.md`
- Modify: `docs/superpowers/plans/2026-08-02-cd-native-runtime-diagnostics.md`

- [x] **Step 1: Record invalid-index coverage**

State that source-backed divide-by-zero, failed native `sqrt(-1)`, and
out-of-range array-index diagnostics now have direct/machine parity. Keep
`debug_ranges` and debugger behavior deferred.

- [x] **Step 2: Update roadmap and plan future-work notes**

Mark invalid-index verification complete in the roadmap and change both earlier
plan self-reviews so only `debug_ranges` and debugger behavior remain future
M5 work.

- [x] **Step 3: Run documentation hygiene**

Run:

```sh
git diff --check
git -C cd-compiler status --short --branch
```

Expected: no diff errors and the nested checkout remains `master` clean.

### Task 4: Verify and commit

**Files:**
- Modify: `docs/superpowers/plans/2026-08-02-cd-index-runtime-diagnostics.md`

- [x] **Step 1: Run the full local gates**

Run:

```sh
ninja -C build-cd llc
cargo test --manifest-path cd-compiler/vm-rs/Cargo.toml
build-cd/bin/llvm-lit -sv llvm/test/CodeGen/CD/cdbc-debug-index-runtime.ll
build-cd/bin/llvm-lit -sv llvm/test/CodeGen/CD
cargo build --manifest-path cd-compiler/vm-rs/Cargo.toml --quiet
python3 llvm/utils/cd_bytecode_parity.py \
  --llc build-cd/bin/llc \
  --vm cd-compiler/vm-rs/target/debug/compiler-design-vm \
  --manifest llvm/test/CodeGen/CD/cdbc-machine-parity.list
```

Expected: Rust VM `73 + 3 + 8`, focused lit `1/1`, full CD lit `61/61`, and
parity `38/38` pass.

- [x] **Step 2: Record exact counts and commit only outer files**

Update this plan with fresh counts, run `git diff --check`, stage only the
fixture, manifest, documentation, and plan files, and commit with:

```sh
git add llvm/test/CodeGen/CD/cdbc-debug-index-runtime.ll \
  llvm/test/CodeGen/CD/cdbc-machine-parity.list \
  docs/cd-bytecode-llvm-abi.md \
  docs/cd-bytecode-machine-backend.md \
  llvm/lib/Target/CD/README.md \
  docs/superpowers/plans/2026-07-31-cd-bytecode-roadmap.md \
  docs/superpowers/plans/2026-08-02-cd-runtime-diagnostics.md \
  docs/superpowers/plans/2026-08-02-cd-native-runtime-diagnostics.md \
  docs/superpowers/plans/2026-08-02-cd-index-runtime-diagnostics.md
git diff --cached --check
git commit -m "test(cd): cover index runtime diagnostics"
```

Do not push or modify the nested checkout.

Verification on 2026-08-02: LLVM `llc` rebuilt successfully, focused CD lit
`1/1`, complete CD lit `61/61`, Rust VM cargo tests `73 + 3 + 8`, and full
direct/machine parity `38/38` all passed. The nested checkout remained clean.

## Self-review

- This slice consumes existing array/index and debug-location contracts only.
- The next `debug_ranges` slice still needs an explicit source-byte-offset
  metadata decision and is intentionally outside this plan.
