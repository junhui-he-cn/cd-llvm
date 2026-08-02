# CD Debug Ranges Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Emit validated source-local half-open byte ranges in `debug_ranges` for LLVM-produced `cdbc 0.1` artifacts when explicit LLVM metadata supplies exact offsets.

**Architecture:** Keep `!cd.sources` as the source-byte owner and add a module-level `!cd.ranges` table whose records identify a `DILocation` and two non-negative byte offsets. The shared debug resolver validates each record against the matched source and attaches the range to the existing typed `CDDebugLocation`; both direct and machine emitters then serialize the same artifact-level range section.

**Tech Stack:** LLVM 24 IR metadata, the CD typed artifact model, LLVM lit/FileCheck, and the existing Rust `cdbc 0.1` parser/VM parity harness.

---

### Task 1: Add the positive range fixture

**Files:**
- Create: `llvm/test/CodeGen/CD/cdbc-debug-ranges.ll`

- [x] **Step 1: Write the failing direct/machine checks first**

Use `!cd.sources` plus `!cd.ranges` records keyed by the `DILocation` nodes used by the divide, print, and helper-function instructions. Check the exact sparse `debug_ranges` indexes for both emitters, with the divide mapped to `s0:6:11` in `print 4 / 2;\n`.

- [x] **Step 2: Run the fixture against the current target**

Run:

```sh
build-cd/bin/llvm-lit -sv llvm/test/CodeGen/CD/cdbc-debug-ranges.ll
```

Observed: FAIL because the current LLVM artifact serializer had no
`debug_ranges` section.

### Task 2: Extend the typed artifact model and serializer

**Files:**
- Modify: `llvm/lib/Target/CD/CDBytecodeFormat.h`
- Modify: `llvm/lib/Target/CD/CDBytecodeFormat.cpp`

- [x] **Step 1: Add the range value and location association**

Add a `CDDebugRange` with source index and 64-bit byte offsets, then add an optional range to `CDDebugLocation`.

- [x] **Step 2: Validate range invariants before serialization**

Require a valid source index, matching location/source identity, `start <= end`, and `end <= debug_sources[sN].text.size()`. Preserve metadata-free artifacts and location-only artifacts.

- [x] **Step 3: Serialize the optional section**

Emit `debug_ranges:` after `debug_locations:` and write sparse `main` and `function fN` records in artifact instruction order.

### Task 3: Parse and resolve explicit LLVM range metadata

**Files:**
- Modify: `llvm/lib/Target/CD/CDDebugInfo.h`
- Modify: `llvm/lib/Target/CD/CDDebugInfo.cpp`
- Modify: `llvm/lib/Target/CD/CDBytecodeEmitter.cpp`
- Modify: `llvm/lib/Target/CD/CDMachineBytecodeEmitter.cpp`

- [x] **Step 1: Parse `!cd.ranges` records**

Accept exactly `!{!DILocation, integer start, integer end}` records. Reject malformed records, negative or wider-than-64-bit offsets, duplicate location records, ranges without a source-backed positive location, reversed ranges, and ranges outside the supplied source bytes.

- [x] **Step 2: Resolve ranges by `DILocation` identity**

Keep the mapping in the shared debug helper and apply a range only to an instruction whose resolved location has the same source. Do not derive offsets from line/column.

- [x] **Step 3: Wire both artifact paths**

Parse the table once per module, pass it through direct and machine emitters, and keep synthetic constants or location-free instructions sparse.

### Task 4: Add malformed-input coverage and parity

**Files:**
- Create: `llvm/test/CodeGen/CD/cdbc-debug-range-errors.ll`
- Modify: `llvm/test/CodeGen/CD/cdbc-machine-parity.list`

- [x] **Step 1: Cover stable diagnostics**

Exercise wrong record shape, non-location key, non-integer offsets, negative offsets, reversed ranges, source overflow, missing source-backed location, and duplicate location records through direct and machine `llc` runs.

- [x] **Step 2: Add one positive Rust VM parity case**

Require direct and machine `dump`/`run` behavior to match for the range fixture while leaving runtime semantics unchanged.

### Task 5: Document and verify the boundary

**Files:**
- Modify: `docs/cd-bytecode-llvm-abi.md`
- Modify: `docs/cd-bytecode-machine-backend.md`
- Modify: `llvm/lib/Target/CD/README.md`
- Modify: `docs/superpowers/plans/2026-07-31-cd-bytecode-roadmap.md`

- [x] **Step 1: Document the metadata contract**

State that `!cd.ranges` supplies exact source-local byte offsets keyed by `DILocation`; ordinary LLVM line/column metadata never becomes an inferred range.

- [x] **Step 2: Run focused and full gates**

Run the focused range lit tests, complete CD lit suite, Rust cargo tests, direct/machine parity, explicit Rust dump checks, `git diff --check`, and nested-checkout status without modifying `cd-compiler/`.

- [ ] **Step 3: Commit the validated slice**

```sh
git add docs/cd-bytecode-llvm-abi.md docs/cd-bytecode-machine-backend.md \
  docs/superpowers/plans/2026-07-31-cd-bytecode-roadmap.md \
  docs/superpowers/plans/2026-08-02-cd-debug-ranges.md \
  llvm/lib/Target/CD llvm/test/CodeGen/CD/cdbc-debug-range*.ll \
  llvm/test/CodeGen/CD/cdbc-machine-parity.list
git commit -m "feat(cd): add debug ranges"
```
