# CD Debug Source Tables Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Lower explicitly supplied LLVM `!cd.sources` metadata into the
optional `debug_sources` section of `cdbc 0.1` without inferring source bytes
from ordinary LLVM debug metadata.

**Architecture:** A small `CDDebugInfo` parser owns the LLVM named-metadata
contract and validates UTF-8, source identity, and record shape. The typed
`CDArtifact` owns the validated source table and the existing serializer emits
it canonically for both direct and opt-in machine paths. This slice deliberately
does not map `DILocation` to instructions and does not emit `debug_locations` or
`debug_ranges`; those remain the next M5 boundary.

**Tech Stack:** LLVM `NamedMDNode`/`MDNode`/`MDString`, C++ CD artifact model,
LLVM lit/FileCheck, the existing Rust `cdbc 0.1` parser/verifier, and the
direct/machine parity harness.

---

### Task 1: Define the source metadata contract and red fixtures

**Files:**
- Modify: `docs/cd-bytecode-llvm-abi.md`
- Modify: `llvm/lib/Target/CD/README.md`
- Create: `llvm/test/CodeGen/CD/cdbc-debug-sources.ll`
- Create: `llvm/test/CodeGen/CD/cdbc-debug-source-errors.ll`

- [x] Document the exact named metadata shape:

```llvm
!cd.sources = !{!0, !1}
!0 = !{!"demo.cd", !"print 1;\0A"}
!1 = !{!"/workspace/lib.cd", !"lib.cd", !"fun fail() { return 1 / 0; }\0A"}
```

  A two-string record is `path,text`; a three-string record is
  `module,path,text`. The module identity is optional only by record arity,
  path is non-empty, and all three strings must be valid UTF-8.
- [x] Add a positive fixture with one path-only and one module-aware source,
  and FileCheck expectations for `debug_sources`, `path=`, `module=`, and the
  escaped source text in both direct and machine output.
- [x] Add malformed records for one/four operands, non-string operands, empty
  paths, empty module identities, invalid UTF-8, and duplicate `(module,path)`
  identities; check stable direct/machine target diagnostics.
- [x] Run the new fixtures before implementation and confirm the positive
  fixture does not emit `debug_sources` yet while malformed metadata is not
  accepted by the target.

### Task 2: Add the typed source-table model and parser

**Files:**
- Create: `llvm/lib/Target/CD/CDDebugInfo.h`
- Create: `llvm/lib/Target/CD/CDDebugInfo.cpp`
- Modify: `llvm/lib/Target/CD/CDBytecodeFormat.h`
- Modify: `llvm/lib/Target/CD/CDBytecodeFormat.cpp`
- Modify: `llvm/lib/Target/CD/CMakeLists.txt`

- [x] Add `CDDebugSource { std::optional<std::string> module; std::string path; std::string text; }` and `CDArtifact::debugSources`.
- [x] Add `parseCDSources(const Module &, std::vector<CDDebugSource> &, std::string &)` that returns an empty table when `!cd.sources` is absent and rejects every malformed case from Task 1.
- [x] Detect duplicate identities using module identity plus path, treating an omitted module as a distinct empty identity; do not deduplicate valid source entries.
- [x] Serialize `debugSources` after function bodies as canonical `debug_sources:` entries, reusing the artifact string escaping and omitting the optional `module=` field when absent.
- [x] Validate the source table before serialization: module identities and paths must be non-empty, and all stored strings must remain valid UTF-8.

### Task 3: Wire both emitters through the shared source parser

**Files:**
- Modify: `llvm/lib/Target/CD/CDBytecodeEmitter.cpp`
- Modify: `llvm/lib/Target/CD/CDMachineBytecodeEmitter.cpp`

- [x] Parse `!cd.sources` before lowering functions in each module emitter and
  report errors with the existing direct or machine target prefix.
- [x] Move the validated table into `CDArtifact::debugSources` before calling
  `serializeArtifact`; do not inspect `DILocation`, `DIFile`, or ordinary
  `llvm.dbg.*` metadata in this slice.
- [x] Keep metadata-free artifacts byte-for-byte unchanged except for the
  already committed native-call behavior.

### Task 4: Verify the source-table boundary

**Files:**
- Modify: `llvm/test/CodeGen/CD/cdbc-machine-parity.list`
- Modify: `docs/superpowers/plans/2026-07-31-cd-bytecode-roadmap.md`

- [x] Build `llc`, run the focused debug lit fixtures, and verify the complete
  CD lit directory.
- [x] Run Rust `dump` and `run` on the positive artifact and compare direct /
  machine output through the parity harness.
- [x] Run Rust VM cargo tests and `git diff --check`; confirm the nested
  `cd-compiler` checkout remains clean.
- [x] Mark only the M5 source-table foundation complete in the ABI/README and
  roadmap documentation; keep instruction locations, ranges, and inferred
  source text explicitly deferred.
- [x] Commit the outer slice as `feat(cd): add debug source tables`; do not
  push or modify the nested checkout.

Verification on 2026-08-02: focused debug lit `2/2`, complete CD lit `57/57`,
direct/machine `dump` and `run` matched for the positive artifact,
direct/machine parity `34/34`, Rust VM cargo tests `73 + 3 + 8`, and the nested
checkout remained clean.
