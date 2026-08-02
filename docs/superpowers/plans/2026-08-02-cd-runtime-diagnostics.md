# CD Source-Backed Runtime Diagnostics Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Prove that LLVM-produced direct and machine `cdbc 0.1` artifacts preserve source-backed divide-by-zero diagnostics and nested call stacks.

**Architecture:** Reuse the existing `!cd.sources` plus `DILocation` lowering and the Rust VM's existing `debug_locations` runtime contract. Add one LLVM IR fixture with a failing nested function, assert the emitted sparse tables with FileCheck, and add it to the parity harness as a `runtime-error` case. This slice changes no artifact syntax, VM code, or nested checkout.

**Tech Stack:** LLVM 24 target emitter, LLVM lit/FileCheck, `cdbc 0.1`, `cd_bytecode_parity.py`, and the sibling Rust VM executable.

---

### Task 1: Add the source-backed runtime-error fixture

**Files:**
- Create: `llvm/test/CodeGen/CD/cdbc-debug-runtime.ll`

- [x] **Step 1: Add the failing nested-function IR fixture and direct/machine checks**

Create `llvm/test/CodeGen/CD/cdbc-debug-runtime.ll` with this exact input:

```llvm
; RUN: llc -mtriple=cd-unknown-unknown %s -o %t.direct
; RUN: FileCheck --check-prefix=DIRECT %s < %t.direct
; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o %t.machine
; RUN: FileCheck --check-prefix=MACHINE %s < %t.machine

define i32 @main() !dbg !5 {
entry:
  %result = call i32 @fail(), !dbg !10
  ret i32 %result, !dbg !11
}

define i32 @fail() !dbg !6 {
entry:
  %bad = sdiv i32 1, 0, !dbg !12
  ret i32 %bad, !dbg !13
}

!cd.sources = !{!20}
!20 = !{!"runtime.cd", !"fun fail() { return 1 / 0; }\0Afail();\0A"}
!llvm.module.flags = !{!30}
!llvm.dbg.cu = !{!0}
!0 = distinct !DICompileUnit(language: DW_LANG_C, file: !1,
    producer: "cd", isOptimized: false, emissionKind: FullDebug)
!1 = !DIFile(filename: "runtime.cd", directory: "")
!2 = !{}
!3 = !DISubroutineType(types: !2)
!5 = distinct !DISubprogram(name: "main", scope: !1, file: !1, line: 2,
    type: !3, unit: !0, retainedNodes: !2)
!6 = distinct !DISubprogram(name: "fail", scope: !1, file: !1, line: 1,
    type: !3, unit: !0, retainedNodes: !2)
!10 = !DILocation(line: 2, column: 1, scope: !5)
!11 = !DILocation(line: 2, column: 1, scope: !5)
!12 = !DILocation(line: 1, column: 22, scope: !6)
!13 = !DILocation(line: 1, column: 14, scope: !6)
!30 = !{i32 2, !"Debug Info Version", i32 3}

; DIRECT: debug_locations:
; DIRECT: main 0 = s0:2:1
; DIRECT: main 1 = s0:2:1
; DIRECT: main 2 = s0:2:1
; DIRECT: function f0 2 = s0:1:22
; DIRECT: function f0 3 = s0:1:14
; MACHINE: debug_locations:
; MACHINE: main 1 = s0:2:1
; MACHINE: main 2 = s0:2:1
; MACHINE: function f0 2 = s0:1:22
; MACHINE: function f0 3 = s0:1:14
```

- [x] **Step 2: Run the focused fixture**

Run:

```sh
build-cd/bin/llvm-lit -sv llvm/test/CodeGen/CD/cdbc-debug-runtime.ll
```

Expected: `1` test passed for both direct and machine FileCheck runs.

### Task 2: Add direct/machine runtime-error parity

**Files:**
- Modify: `llvm/test/CodeGen/CD/cdbc-machine-parity.list`

- [x] **Step 1: Add the exact VM diagnostic expectation**

Append this line:

```text
runtime-error llvm/test/CodeGen/CD/cdbc-debug-runtime.ll "Runtime error at runtime.cd:1:22: division by zero"
```

The parity harness must remain in `runtime-error` mode so it compares the full direct and machine stderr strings, while the expected substring proves the source path, line, column, and runtime message.

- [x] **Step 2: Run the parity case**

Run:

```sh
python3 llvm/utils/cd_bytecode_parity.py \
  --llc build-cd/bin/llc \
  --vm cd-compiler/vm-rs/target/debug/compiler-design-vm \
  --manifest llvm/test/CodeGen/CD/cdbc-machine-parity.list
```

Expected: `36/36` manifest entries complete, including identical direct/machine call-stack diagnostics for the new fixture.

### Task 3: Record the completed boundary

**Files:**
- Modify: `docs/cd-bytecode-llvm-abi.md`
- Modify: `docs/cd-bytecode-machine-backend.md`
- Modify: `llvm/lib/Target/CD/README.md`
- Modify: `docs/superpowers/plans/2026-07-31-cd-bytecode-roadmap.md`

- [x] **Step 1: Document the runtime diagnostic guarantee**

Update the M5 wording to state that the LLVM target now covers a source-backed nested divide-by-zero diagnostic through both artifact paths, including source line/caret and call-stack parity. Keep invalid-index and failed-native-call diagnostics as later M5 coverage, and keep `debug_ranges` deferred because this target still has no exact source-byte-offset metadata input.

- [x] **Step 2: Run documentation hygiene**

Run:

```sh
git diff --check
git -C cd-compiler status --short --branch
```

Expected: no diff errors and the nested checkout remains `master` clean.

### Task 4: Full verification and delivery

**Files:**
- Modify: `docs/superpowers/plans/2026-08-02-cd-runtime-diagnostics.md`

- [x] **Step 1: Run the focused/full verification gates**

Run:

```sh
ninja -C build-cd llc
cargo test --manifest-path cd-compiler/vm-rs/Cargo.toml
build-cd/bin/llvm-lit -sv llvm/test/CodeGen/CD/cdbc-debug-runtime.ll
build-cd/bin/llvm-lit -sv llvm/test/CodeGen/CD
cargo build --manifest-path cd-compiler/vm-rs/Cargo.toml --quiet
python3 llvm/utils/cd_bytecode_parity.py \
  --llc build-cd/bin/llc \
  --vm cd-compiler/vm-rs/target/debug/compiler-design-vm \
  --manifest llvm/test/CodeGen/CD/cdbc-machine-parity.list
```

Expected: Rust VM `73 + 3 + 8`, focused lit `1/1`, full CD lit with the new total, and parity `36/36` all pass.

- [x] **Step 2: Update this plan with exact counts and mark completed tasks**

Record the fresh counts from Step 1, then run `git diff --check` again before staging.

- [x] **Step 3: Commit only the outer-repository slice**

Stage the fixture, parity manifest, four documentation/plan files, and no path under `cd-compiler/`; commit with:

```sh
git add llvm/test/CodeGen/CD/cdbc-debug-runtime.ll \
  llvm/test/CodeGen/CD/cdbc-machine-parity.list \
  docs/cd-bytecode-llvm-abi.md \
  docs/cd-bytecode-machine-backend.md \
  llvm/lib/Target/CD/README.md \
  docs/superpowers/plans/2026-07-31-cd-bytecode-roadmap.md \
  docs/superpowers/plans/2026-08-02-cd-runtime-diagnostics.md
git diff --cached --check
git commit -m "test(cd): cover runtime diagnostics"
```

Do not push or modify the nested checkout.

Verification on 2026-08-02: LLVM `llc` rebuilt successfully, focused CD lit
`1/1`, complete CD lit `59/59`, Rust VM cargo tests `73 + 3 + 8`, and full
direct/machine parity `36/36` all passed. The nested checkout remained clean.

## Self-review

- This plan covers only the already-supported `debug_locations` runtime path;
  it does not invent a `!cd.ranges` metadata contract.
- The fixture checks the direct and machine sparse-table indexes separately
  because the machine path omits the direct path's synthetic `make_function`
  location, while both runtime diagnostics must remain byte-for-byte equal.
- `debug_ranges` and debugger behavior remain explicit future M5 slices;
  failed-native-call and invalid-index coverage are delivered by follow-on
  fixtures.
