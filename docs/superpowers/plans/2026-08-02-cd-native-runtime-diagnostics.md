# CD Native Runtime Diagnostics Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Cover a source-backed failed native call through both LLVM CD artifact paths and the Rust VM runtime-error parity harness.

**Architecture:** Reuse the existing bounded `llvm.cd.native` lowering, explicit `!cd.sources`, and `DILocation` mapping. Add one `sqrt(-1)` fixture, assert its sparse location in direct and machine output, and compare the complete Rust VM error including source line, caret, and main call stack. No new artifact syntax or nested-checkout changes are needed.

**Tech Stack:** LLVM 24 target emitter, LLVM lit/FileCheck, `cdbc 0.1`, `cd_bytecode_parity.py`, and the sibling Rust VM executable.

---

### Task 1: Add the failed-native-call fixture

**Files:**
- Create: `llvm/test/CodeGen/CD/cdbc-debug-native-runtime.ll`

- [x] **Step 1: Add the direct/machine IR and debug-table checks**

Create the fixture with this exact input:

```llvm
; RUN: llc -mtriple=cd-unknown-unknown %s -o %t.direct
; RUN: FileCheck --check-prefix=DIRECT %s < %t.direct
; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o %t.machine
; RUN: FileCheck --check-prefix=MACHINE %s < %t.machine

@sqrt_name = private unnamed_addr constant [5 x i8] c"sqrt\00"

declare double @llvm.cd.native(ptr, ...)

define i32 @main() !dbg !5 {
entry:
  %value = call double (ptr, ...) @llvm.cd.native(ptr @sqrt_name, double -1.0), !dbg !10
  ret i32 0, !dbg !11
}

!cd.sources = !{!20}
!20 = !{!"native.cd", !"sqrt(-1);\0A"}
!llvm.module.flags = !{!30}
!llvm.dbg.cu = !{!0}
!0 = distinct !DICompileUnit(language: DW_LANG_C, file: !1,
    producer: "cd", isOptimized: false, emissionKind: FullDebug)
!1 = !DIFile(filename: "native.cd", directory: "")
!2 = !{}
!3 = !DISubroutineType(types: !2)
!5 = distinct !DISubprogram(name: "main", scope: !1, file: !1, line: 1,
    type: !3, unit: !0, retainedNodes: !2)
!10 = !DILocation(line: 1, column: 1, scope: !5)
!11 = !DILocation(line: 1, column: 1, scope: !5)
!30 = !{i32 2, !"Debug Info Version", i32 3}

; DIRECT: debug_locations:
; DIRECT: main 1 = s0:1:1
; DIRECT: main 3 = s0:1:1
; MACHINE: debug_locations:
; MACHINE: main 1 = s0:1:1
; MACHINE: main 3 = s0:1:1
```

- [x] **Step 2: Run the focused fixture**

Run:

```sh
build-cd/bin/llvm-lit -sv llvm/test/CodeGen/CD/cdbc-debug-native-runtime.ll
```

Expected: `1` test passed.

### Task 2: Add native runtime-error parity

**Files:**
- Modify: `llvm/test/CodeGen/CD/cdbc-machine-parity.list`

- [x] **Step 1: Add the exact expected diagnostic**

Append:

```text
runtime-error llvm/test/CodeGen/CD/cdbc-debug-native-runtime.ll "Runtime error at native.cd:1:1: sqrt expects non-negative number"
```

- [x] **Step 2: Run the complete manifest**

Run:

```sh
python3 llvm/utils/cd_bytecode_parity.py \
  --llc build-cd/bin/llc \
  --vm cd-compiler/vm-rs/target/debug/compiler-design-vm \
  --manifest llvm/test/CodeGen/CD/cdbc-machine-parity.list
```

Expected: `37/37`, with identical direct/machine native failure text.

### Task 3: Update the M5 boundary documentation

**Files:**
- Modify: `docs/cd-bytecode-llvm-abi.md`
- Modify: `docs/cd-bytecode-machine-backend.md`
- Modify: `llvm/lib/Target/CD/README.md`
- Modify: `docs/superpowers/plans/2026-07-31-cd-bytecode-roadmap.md`
- Modify: `docs/superpowers/plans/2026-08-02-cd-runtime-diagnostics.md`

- [x] **Step 1: Record failed-native-call coverage**

State that the LLVM target now preserves a source-backed `sqrt(-1)` runtime
diagnostic through both direct and machine artifacts, including the native
call's source line, caret, and main call stack. Keep invalid-index diagnostics,
`debug_ranges`, and debugger behavior deferred.

- [x] **Step 2: Keep the earlier plan's future-work note accurate**

Change the earlier runtime-diagnostics plan's self-review from “invalid index,
failed native call, and `debug_ranges` remain future” to “invalid index and
`debug_ranges` remain future; failed native call is covered by this follow-on
fixture.”

- [x] **Step 3: Run documentation hygiene**

Run:

```sh
git diff --check
git -C cd-compiler status --short --branch
```

Expected: no diff errors and the nested checkout remains `master` clean.

### Task 4: Verify and commit

**Files:**
- Modify: `docs/superpowers/plans/2026-08-02-cd-native-runtime-diagnostics.md`

- [x] **Step 1: Run the full local gates**

Run:

```sh
ninja -C build-cd llc
cargo test --manifest-path cd-compiler/vm-rs/Cargo.toml
build-cd/bin/llvm-lit -sv llvm/test/CodeGen/CD/cdbc-debug-native-runtime.ll
build-cd/bin/llvm-lit -sv llvm/test/CodeGen/CD
cargo build --manifest-path cd-compiler/vm-rs/Cargo.toml --quiet
python3 llvm/utils/cd_bytecode_parity.py \
  --llc build-cd/bin/llc \
  --vm cd-compiler/vm-rs/target/debug/compiler-design-vm \
  --manifest llvm/test/CodeGen/CD/cdbc-machine-parity.list
```

Expected: Rust VM `73 + 3 + 8`, focused lit `1/1`, full CD lit `60/60`, and
parity `37/37` pass.

- [x] **Step 2: Record exact counts and commit only outer files**

Update this plan with fresh counts, run `git diff --check`, then stage only the
fixture, manifest, documentation, and plan files. Do not stage `cd-compiler/`.
Commit with:

```sh
git add llvm/test/CodeGen/CD/cdbc-debug-native-runtime.ll \
  llvm/test/CodeGen/CD/cdbc-machine-parity.list \
  docs/cd-bytecode-llvm-abi.md \
  docs/cd-bytecode-machine-backend.md \
  llvm/lib/Target/CD/README.md \
  docs/superpowers/plans/2026-07-31-cd-bytecode-roadmap.md \
  docs/superpowers/plans/2026-08-02-cd-runtime-diagnostics.md \
  docs/superpowers/plans/2026-08-02-cd-native-runtime-diagnostics.md
git diff --cached --check
git commit -m "test(cd): cover native runtime diagnostics"
```

Do not push or modify the nested checkout.

Verification on 2026-08-02: LLVM `llc` rebuilt successfully, focused CD lit
`1/1`, complete CD lit `60/60`, Rust VM cargo tests `73 + 3 + 8`, and full
direct/machine parity `37/37` all passed. The nested checkout remained clean.

## Self-review

- This plan only consumes the existing native-call and debug-location
  contracts; it introduces no new opcode or metadata format.
- The fixture uses the exact `sqrt` failure already covered without metadata,
  adding only source-backed rendering and direct/machine parity.
- Invalid-index diagnostics, `debug_ranges`, and debugger behavior remain
  explicit later M5 boundaries.
