# CD Module Envelope Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Emit LLVM CD module products in the existing `cdbc 0.1` `artifact: module` envelope using explicit LLVM named metadata while preserving program mode by default.

**Architecture:** Add a typed module header to the shared CD artifact model and a parser for `!cd.module` plus `!cd.dependencies`. The direct and opt-in machine emitters receive the same `-cd-artifact=module` mode, parse the same metadata, and serialize through the existing canonical formatter. The Rust VM already parses and validates this envelope, so this slice does not modify the nested `cd-compiler/` checkout or implement multi-product linker graph behavior.

**Tech Stack:** LLVM 24 `NamedMDNode`/`MDNode`/`MDString`/`ConstantAsMetadata`, the C++ `CDArtifact` model, LLVM lit/FileCheck, and the sibling Rust VM's existing `cdbc 0.1` module parser.

---

### Task 1: Add red module fixtures

**Files:**
- Create: `llvm/test/CodeGen/CD/cdbc-modules.ll`
- Create: `llvm/test/CodeGen/CD/cdbc-module-errors.ll`

- [x] **Step 1: Write the positive direct/machine checks first**

Use `-cd-artifact=module` with one valid `!cd.module` record and one ordered
`!cd.dependencies` record. The module record is positional: four operands for
a non-entry module, or five operands with the final entry order for an entry
module.

```llvm
!cd.module = !{!0}
!0 = !{!"entry", !"entry.cd", !"/workspace/entry.cd", i1 true, i64 0}
!cd.dependencies = !{!1}
!1 = !{!"import", !"/workspace/lib.cd", i64 1, !"./lib.cd"}
```

The serialized artifact must contain `artifact: module`, the four module
fields, `entry_order = 0`, the dependency marker, and the normal bytecode
sections. Add separate `DIRECT` and `MACHINE` checks for the two target paths.

- [x] **Step 2: Add malformed metadata cases**

Use `split-file` to cover module mode without `!cd.module`, duplicate module
records, wrong module record arity, non-boolean entry, entry-order on a
non-entry module, malformed dependency shape, unsupported dependency kind, and
negative dependency offset. Each case runs both target paths and checks the
stable direct or machine diagnostic prefix.

- [x] **Step 3: Run the red baseline**

Run:

```sh
build-cd/bin/llvm-lit -sv llvm/test/CodeGen/CD/cdbc-modules.ll \
  llvm/test/CodeGen/CD/cdbc-module-errors.ll
```

Expected: the tests fail because the current `llc` does not recognize
`-cd-artifact=module`; no production module code is written before this red
check is observed.

### Task 2: Add the typed module model and canonical serializer

**Files:**
- Modify: `llvm/lib/Target/CD/CDBytecodeFormat.h`
- Modify: `llvm/lib/Target/CD/CDBytecodeFormat.cpp`

- [x] **Step 1: Define the shared types**

Add `CDModuleDependencyKind::{Import, ReExport}`, `CDModuleDependency` with
`identity`, `kind`, `instructionOffset`, and `requestedPath`, and
`CDModuleMetadata` with `identity`, `path`, `canonicalPath`, `isEntry`,
optional `entryOrder`, and ordered dependencies. Add an optional module field
to `CDArtifact`; its absence must preserve byte-for-byte program output.

- [x] **Step 2: Validate module invariants**

Require non-empty identity/path/canonical path, require `entryOrder` exactly
when `isEntry` is true, require non-empty dependency identity/requested path,
and require nondecreasing dependency offsets no greater than the lowered main
instruction count. Invoke this validation before serialization and include the
module index in stable errors where applicable.

- [x] **Step 3: Serialize the existing VM envelope**

When the optional module field is present, write:

```text
cdbc 0.1

artifact: module

module:
  identity = "..."
  path = "..."
  canonical_path = "..."
  entry = true
  entry_order = 0
  dependencies:
    d0 target="..." kind=import at=1 requested="..."

```

Then reuse the existing constants, names, bodies, and debug sections. When it
is absent, retain the current program envelope exactly.

### Task 3: Parse metadata and wire both emitters

**Files:**
- Create: `llvm/lib/Target/CD/CDModuleInfo.h`
- Create: `llvm/lib/Target/CD/CDModuleInfo.cpp`
- Modify: `llvm/lib/Target/CD/CMakeLists.txt`
- Modify: `llvm/lib/Target/CD/CDBytecodeEmitter.h`
- Modify: `llvm/lib/Target/CD/CDBytecodeEmitter.cpp`
- Modify: `llvm/lib/Target/CD/CDMachineBytecodeEmitter.h`
- Modify: `llvm/lib/Target/CD/CDMachineBytecodeEmitter.cpp`
- Modify: `llvm/lib/Target/CD/CDTargetMachine.cpp`

- [x] **Step 1: Parse the named metadata contract**

Implement `parseCDModuleMetadata(const Module &, CDModuleMetadata &,
std::string &)` using exactly one `!cd.module` record and an optional ordered
`!cd.dependencies` list. Validate strings as UTF-8, accept only `i1` for the
entry flag and non-negative 64-bit integers for entry/dependency offsets, and
reject extra records or operands. An absent module node returns a failure in
module mode; an absent dependency node yields an empty list.

- [x] **Step 2: Add explicit artifact-mode selection**

Add hidden `-cd-artifact=program|module`, defaulting to `program`, and pass the
selected mode into both emitter passes. Program mode remains the compatibility
default; if module metadata appears without module mode, reject it instead of
silently dropping it. Module mode requires valid module metadata and stores it
in the shared artifact model.

- [x] **Step 3: Keep direct and machine lowering symmetric**

Parse module metadata before lowering functions in both paths, assign the
typed header to `CDArtifact::module`, and continue to use the existing shared
validator/serializer. Do not inspect or mutate `cd-compiler/`; ordinary
`!llvm.dbg.*` metadata and the existing `!cd.sources`/`!cd.ranges` contracts
remain unchanged.

### Task 4: Document the boundary and defer graph linking

**Files:**
- Modify: `docs/cd-bytecode-llvm-abi.md`
- Modify: `llvm/lib/Target/CD/README.md`
- Modify: `docs/superpowers/plans/2026-07-31-cd-bytecode-roadmap.md`
- Modify: this plan

- [x] **Step 1: Document the LLVM metadata and `llvm-link` rule**

Record the positional `!cd.module` and `!cd.dependencies` shapes, the
`-cd-artifact=module` opt-in, and the rule that `llvm-link`-merged multiple
module records are rejected rather than synthesized into one product. State
that module products use the Rust VM's existing `artifact: module` envelope.

- [x] **Step 2: Record the completed M6 foundation**

Mark only the module-envelope foundation complete in the roadmap. Keep
missing-dependency, cycle, entry-order, multi-product linking, and linked
runtime diagnostics as the next M6 slice.

### Task 5: Verify and commit the outer slice

**Files:**
- Test: `llvm/test/CodeGen/CD/cdbc-modules.ll`
- Test: `llvm/test/CodeGen/CD/cdbc-module-errors.ll`

- [x] **Step 1: Run focused and full LLVM gates**

Run `ninja -C build-cd llc`, the two focused module fixtures, and
`build-cd/bin/llvm-lit -sv llvm/test/CodeGen/CD`. Verification on 2026-08-02:
focused module lit `2/2`, complete CD lit `65/65`, and the incremental `llc`
build passed.

- [x] **Step 2: Run Rust parser/link boundary checks without editing the nested checkout**

Build and test the sibling VM, compile the positive fixture through both
LLVM paths, run `dump` on both artifacts, confirm direct `run` rejects the
module envelope, and run `git -C cd-compiler status --short --branch`.
Verification: Rust Cargo `73 + 3 + 8`, parity manifest `41/41`, parity unit
tests `8/8`, both direct/machine `dump` checks passed, both direct/machine
`run` checks rejected the unlinked module, and the nested checkout remained
`master` clean.

- [x] **Step 3: Commit only the outer repository files**

After `git diff --check` and staged whitespace validation pass, commit the
outer slice as:

```sh
git commit -m "feat(cd): emit module artifact envelopes"
```

Do not push, modify, or stage any path under `cd-compiler/` in this slice.

Committed in the outer repository as `feat(cd): emit module artifact
envelopes` after the verification gates above.
