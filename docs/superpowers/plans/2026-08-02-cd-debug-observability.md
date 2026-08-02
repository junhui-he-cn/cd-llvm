# CD Debug Observability Parity Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a reproducible outer-repository parity gate for `dump`, `trace`, interactive `debug`, and `profile` on LLVM CD artifacts with and without explicit debug metadata.

**Architecture:** Extend the existing `cd_bytecode_parity.py` manifest with an `observability` case that still compiles direct and machine artifacts and validates normal `dump`/`run` behavior, then compares the metadata section, trace, profile, and scripted debugger output. The case declares whether the input is range-backed or metadata-free so the harness checks both parity and the intended presence or absence of source information without changing the Rust VM checkout.

**Tech Stack:** Python 3, LLVM `llc`, the existing `cd_bytecode_parity.py` harness, `cdbc 0.1`, and the sibling Rust VM executable.

---

### Task 1: Add the failing observability cases

**Files:**
- Modify: `llvm/test/CodeGen/CD/cdbc-machine-parity.list`

- [ ] **Step 1: Add one range-backed and one metadata-free case**

Append these manifest records. The semicolon-separated command sequence is fed
to the interactive debugger, and the final field selects the expected metadata
contract:

```text
observability llvm/test/CodeGen/CD/cdbc-debug-ranges.ll "break-range ranges.cd:6-11;continue;quit" ranges
observability llvm/test/CodeGen/CD/cdbc-machine.ll "continue" metadata-free
```

- [ ] **Step 2: Run the old harness and confirm the red baseline**

Run:

```sh
cargo build --manifest-path cd-compiler/vm-rs/Cargo.toml --quiet
python3 llvm/utils/cd_bytecode_parity.py \
  --llc build-cd/bin/llc \
  --vm cd-compiler/vm-rs/target/debug/compiler-design-vm \
  --manifest llvm/test/CodeGen/CD/cdbc-machine-parity.list
```

Expected: the command fails while parsing the new manifest because the old
harness accepts only `artifact`, `behavior`, and `runtime-error` records.

### Task 2: Implement observability parity

**Files:**
- Modify: `llvm/utils/cd_bytecode_parity.py`

- [ ] **Step 1: Parse the new manifest form**

Accept exactly four fields for `observability`: the input path, a quoted
semicolon-separated debugger command sequence, and either `ranges` or
`metadata-free`. Reject other arities or contract names with a stable manifest
error.

- [ ] **Step 2: Compare only the artifact debug sections**

Keep the existing full artifact normalization rules unchanged. For an
observability case, extract the optional `debug_sources:`, `debug_locations:`,
and `debug_ranges:` tail from each VM dump and require the direct and machine
sections to match exactly. Metadata-free artifacts must have no such tail.

- [ ] **Step 3: Run and compare all four VM surfaces**

Run `dump` and `run` as the existing case setup does, then run `trace`,
`profile`, and `debug` on both artifacts. Feed the declared debugger commands
joined by newlines, require all command invocations to succeed, and require
direct/machine output equality for each surface.

- [ ] **Step 4: Assert the declared metadata contract**

For `ranges`, require the dump, trace, profile, and debug output to expose
`ranges.cd`, `s0:6:11`, and the source-range profile record. For
`metadata-free`, require the dump to have no debug sections, trace/debug to use
`<unknown>`, and all four outputs to omit `range=` and `source_range` records.

### Task 3: Document and register the gate

**Files:**
- Modify: `docs/cd-bytecode-machine-backend.md`
- Modify: `docs/superpowers/plans/2026-07-31-cd-bytecode-roadmap.md`

- [ ] **Step 1: Document the manifest syntax and command**

Explain the two `observability` records, the scripted debugger input, and the
fact that the gate remains opt-in and requires the sibling VM path.

- [ ] **Step 2: Close the M5 observability checkbox**

Update the roadmap to record that direct/machine `dump`, `trace`, `debug`, and
`profile` parity is covered for explicit range metadata and metadata-free
artifacts, while leaving module products and CI/release work unchanged.

### Task 4: Verify the slice

**Files:**
- Test: `llvm/test/CodeGen/CD/cdbc-machine-parity.list`
- Test: `llvm/utils/cd_bytecode_parity.py`

- [ ] **Step 1: Run the focused observability gate**

Run the parity command from Task 1 and expect all existing cases plus the two
new cases to pass.

- [ ] **Step 2: Run the LLVM and Rust gates**

Run:

```sh
ninja -C build-cd llc
build-cd/bin/llvm-lit -sv llvm/test/CodeGen/CD
cargo test --manifest-path cd-compiler/vm-rs/Cargo.toml
git diff --check
git -C cd-compiler status --short --branch
```

Expected: the CD lit suite, all Rust test groups, and whitespace checks pass;
the nested checkout remains clean and unmodified.

- [ ] **Step 3: Commit only the outer observability slice**

```sh
git add docs/cd-bytecode-machine-backend.md \
  docs/superpowers/plans/2026-07-31-cd-bytecode-roadmap.md \
  docs/superpowers/plans/2026-08-02-cd-debug-observability.md \
  llvm/test/CodeGen/CD/cdbc-machine-parity.list \
  llvm/utils/cd_bytecode_parity.py
git commit -m "test(cd): cover debug observability parity"
```
