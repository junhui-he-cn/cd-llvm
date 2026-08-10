# CD Bytecode in LLVM Roadmap

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this roadmap task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Continue the experimental LLVM `CD` target until LLVM IR can produce verified, executable Compiler Design bytecode while preserving the `cd-compiler` Rust VM's `cdbc 0.1` contract.

**Architecture:** Keep `CD` as a software target selected by `cd-unknown-unknown`. LLVM lowers a deliberately defined subset of LLVM IR into an internal CD artifact model and serializes that model as `.cdbc`; it does not pretend that arbitrary LLVM aggregates or native pointers are already CD values. The Rust VM remains the execution oracle, and every new opcode or metadata field is accepted only after synchronized contract, parser, executor, and end-to-end tests.

**Tech Stack:** LLVM 24 target infrastructure, CMake experimental targets, LLVM IR/ModulePass APIs, LLVM lit/FileCheck, `cdbc 0.1` text artifacts, and the sibling `cd-compiler/vm-rs` Rust VM.

---

## 0. Roadmap status at a glance (2026-08-10)

This document is the high-level roadmap and implementation record.  The
independently executable queue for the next slices lives in the
[2026-08-03 development plan](2026-08-03-cd-bytecode-development-plan.md).
The two documents have different jobs: this one freezes scope and status; the
development plan carries step-by-step work for the active queue.

The current boundary is locally verified in this outer checkout:

```text
LLVM IR --llc -mtriple=cd-unknown-unknown--> cdbc 0.1 --> Rust VM
```

The direct emitter remains the compatibility path.  The TableGen machine
emitter is opt-in and shares the typed artifact model, ABI validation, and
direct/machine parity harness.  The nested `cd-compiler/` checkout remains an
independent VM oracle and is not absorbed into this repository.

### Current stage map

| Stage | Scope | Status | Evidence or next boundary |
| --- | --- | --- | --- |
| M0-M1 | Target bootstrap, typed artifact model, canonical serializer, structural validation | Complete | LLVM target and `cdbc 0.1` boundary are stable |
| M2 | Scalar semantics, control flow, PHI/select, `-O0`/`-O2` behavior | Complete | Unsupported integer semantics fail with target diagnostics |
| M3 | Opt-in TableGen/machine path and direct/machine parity | Complete for the supported subset | Machine path remains opt-in and text-only |
| M4 | Explicit CD values: strings, arrays, maps, records, variants, indexing/mutation, bounded natives including `contains`/`slice`/`copy`, selected `map`/`filter`/`flatMap`/`reduce`/`any`/`all`/`count`/`find`/`findIndex` callbacks | Complete for the implemented bounded ABI | Dynamic CD `select`, PHI, and one-slot storage are supported through existing control-flow and variable operations; future native names stay rejected until separately selected |
| M5 | Source tables, locations/ranges, runtime diagnostics, trace/profile/debug observability | Complete for the current surface; pause-state contract frozen | New query commands and richer debugger state require a follow-on public design |
| M6 | Module envelopes, dependency metadata, linking, linked diagnostics | Complete | Program and module artifacts remain distinct |
| M7-local | Reproducible LLVM-only, VM, parity, and module-link verification | Complete | Latest local gate: 108 lit (107 passed, 1 unsupported), parity 77/77, VM `73 + 3 + 8`, module-link direct/machine passed |
| M7-hosted | GitHub Actions execution of the two-job release matrix | External, non-blocking | The complete eight-tool workflow fix is published in `18a6063fd`; hosted execution is not used as a prerequisite for the locally accepted boundary |
| M8-first | Function-boundary dynamic-value transport for marked parameters and returns | Complete | `cd.value.params`/`cd.value.return` share provenance validation; dynamic CD `select`, PHI, and one-slot storage are proven through existing control-flow and variable operations; all selected callback natives are verified |

### Active queue after M7-local

Work is intentionally ordered by dependency rather than by adding more
opcodes:

1. Treat the locally verified M7 gate as the release boundary; hosted execution
   remains an external check. Keep the LLVM 24 upstream `llc -g` rejection as
   an explicit driver boundary.
2. Keep the public debugger state/query contract frozen before adding commands
   such as `list`, `where`, locals, or breakpoint queries. The current pause
   fields, command markers, and synthetic-entry parity exception are now
   documented and tested.
3. Keep the completed function parameter/return transport slice as the ABI
   foundation; dynamic CD `select`, PHI, and one-slot storage now reuse
   existing control-flow and variable operations.
4. Add bounded native helpers one vertical slice at a time. `contains` and the
   `map`, `filter`, `flatMap`, `reduce`, `any`, `all`, `count`, `find`, and
   `findIndex` slices now have explicit matrices; keep future native names
   rejected until their own contracts are defined.

Do not combine items 2-5 in one implementation commit.  In particular,
callback support must not introduce an implicit function-value or ordinary
pointer ABI.

## 1. Scope and invariants

The active boundary is:

```text
LLVM IR --llc -mtriple=cd-unknown-unknown--> cdbc 0.1 --> cd-compiler Rust VM
```

The following rules apply to every milestone:

1. `cd-compiler/docs/bytecode-text-format.md` is the wire-contract authority. The LLVM target must not invent a spelling that the Rust parser does not accept.
2. A generated artifact must pass the Rust VM's `dump` validation before a test claims it is valid. Runtime tests must use `run` and compare observable output or the exact runtime error.
3. Generic LLVM aggregates, pointers, globals, and external calls are not silently reinterpreted as CD arrays, maps, structs, or native values. They require the explicit CD IR/ABI described in M4.
4. The current target remains text-only: no ELF/COFF/Mach-O object output, assembler syntax, JIT, or native machine-code ABI is part of this roadmap.
5. `cd-compiler/` is an independent Git checkout currently visible as an untracked directory in the outer repository. Cross-repository VM changes must be committed in that checkout separately; the LLVM repository must not absorb its `.git` directory or generated build files.

## 2. Implemented baseline and ownership boundaries

The implementation record below is retained for historical traceability; the
stage table in section 0 is the current status source of truth.

The outer repository contains the experimental target in:

- `llvm/lib/Target/CD/TargetInfo/`: `cd` triple registration.
- `llvm/lib/Target/CD/MCTargetDesc/`: minimal MC descriptions required by `llc`.
- `llvm/lib/Target/CD/CDTargetMachine.{h,cpp}`: text-only target machine and
  direct/machine pass hookup.
- `llvm/lib/Target/CD/CDBytecodeEmitter.{h,cpp}`: direct LLVM IR lowering.
- `llvm/lib/Target/CD/CDMachineBytecodeEmitter.{h,cpp}`: opt-in machine-path
  lowering.
- `llvm/lib/Target/CD/CDBytecodeFormat.{h,cpp}`: typed artifact model,
  validation, and canonical serialization.
- `llvm/lib/Target/CD/CDValueABI.{h,cpp}`: explicit `llvm.cd.*` capability
  validation shared by both paths.
- `llvm/test/CodeGen/CD/`: LLVM lit, malformed-input, runtime, debug,
  module, and direct/machine parity fixtures.
- `llvm/utils/cd_bytecode_parity.py` and `cd_module_link.py`: executable VM
  integration gates.

The implemented value boundary is deliberately explicit.  Strings, arrays,
maps, records, enum variants, indexing/mutation, and the bounded native names
`floor`, `ceil`, `sqrt`, `str`, `typeOf`, `hash`, `contains`, `range`, `substr`, `charAt`,
and the selected `map`/`filter`/`flatMap`/`reduce`/`any`/`all`/`count`/`find`/`findIndex` callbacks use target-specific CD
intrinsics and existing `cdbc 0.1` operations.
Ordinary LLVM pointers,
aggregates, globals, allocas, and external calls are not inferred to be CD
values.  The first marked function parameter/return boundary, dynamic CD
`select` and PHI propagation, and the one-slot dynamic storage rule are
implemented; the remaining callback function values and richer debugger
queries remain future design boundaries.

The current release gate is split deliberately:

- local LLVM-only and VM-integrated verification is complete and reproducible;
- hosted GitHub Actions execution is an external pending gate;
- target-side `-g` support is not emulated while the LLVM 24 `llc` driver
  rejects `-g` before target selection.

## 3. Historical implementation slices

Sections 4-11 preserve the detailed design gates and completed slice records.
Use section 0 for current status and section 12 plus the linked development
plan for what should be implemented next; do not interpret old unchecked
historical steps as a second active queue.

## 4. M0 — Revalidate the existing target

**Purpose:** Establish evidence for the current baseline before changing its lowering behavior.

**Files:**

- Verify: `llvm/lib/Target/CD/*`, `llvm/lib/TargetParser/Triple.cpp`, `llvm/include/llvm/TargetParser/Triple.h`, `llvm/test/CodeGen/CD/*.ll`.
- Read-only integration dependency: `cd-compiler/vm-rs/src/format.rs`, `cd-compiler/vm-rs/src/vm.rs`.

- [x] Configure a focused LLVM build without changing the outer working tree:

```sh
cmake -S llvm -B build-cd -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_TARGETS_TO_BUILD=Native \
  -DLLVM_EXPERIMENTAL_TARGETS_TO_BUILD=CD
ninja -C build-cd llc FileCheck llvm-lit
```

- [x] Run the CD lit directory and confirm the CD fixtures pass:

```sh
build-cd/bin/llvm-lit -sv llvm/test/CodeGen/CD
```

- [x] Materialize and validate the artifact with the sibling VM:

```sh
build-cd/bin/llc -mtriple=cd-unknown-unknown -O0 \
  llvm/test/CodeGen/CD/cdbc-basic.ll -o /tmp/cd-basic.cdbc
cargo run --manifest-path cd-compiler/vm-rs/Cargo.toml --quiet \
  -- dump /tmp/cd-basic.cdbc
```

- [x] Confirm `-filetype=obj` fails and no object file is treated as a valid CD artifact.
- [x] Run `git diff --check` and confirm the nested `cd-compiler/` checkout remains untouched.

**Exit criteria:** `llc` recognizes `cd-unknown-unknown`, both lit fixtures pass, Rust `dump` accepts the generated `cdbc 0.1`, object generation is rejected, and the exact commands/results are recorded in the implementation commit.

## 5. M1 — Stabilize the artifact boundary

**Purpose:** Stop growing the emitter as an ad-hoc collection of output strings. Build a typed in-memory artifact first, then serialize and validate it once.

**Files:**

- Create: `llvm/lib/Target/CD/CDBytecodeFormat.h`.
- Create: `llvm/lib/Target/CD/CDBytecodeFormat.cpp`.
- Modify: `llvm/lib/Target/CD/CDBytecodeEmitter.h`.
- Modify: `llvm/lib/Target/CD/CDBytecodeEmitter.cpp`.
- Modify: `llvm/lib/Target/CD/CMakeLists.txt`.
- Create: `llvm/test/CodeGen/CD/cdbc-determinism.ll`.
- Create: `llvm/test/CodeGen/CD/cdbc-errors.ll`.
- Create: `llvm/test/CodeGen/CD/cdbc-invalid-shape.ll`.
- Create: `llvm/unittests/Target/CD/CDBytecodeFormatTest.cpp` and its CMake file.
- Modify: `llvm/lib/Target/CD/README.md`.

The internal model must contain explicit constants, names, functions, bodies, instructions, registers, branch targets, and optional metadata. The serializer must be the only code that knows the exact text indentation, reference prefixes, quoting, and section order. Keep first-use tables deterministic while using lookup maps for deduplication.

The model validator must reject before writing:

- out-of-range constant, name, function, and register references;
- non-contiguous function indexes or invalid function arity/parameter records;
- branch targets outside the current body;
- instructions with missing required operands or impossible result registers;
- non-finite number constants and unsupported constant expressions;
- a module with no defined `main`, a parameterized `main`, or unsupported global state.

- [x] Move the current `CDConstant`, body, and instruction construction into the typed model without changing the emitted text.
- [x] Add a canonical serializer and run the same LLVM input twice, checking byte-for-byte identical output.
- [x] Add negative `llc` tests for one unsupported instruction and one invalid input shape; diagnostics must name the CD target and the LLVM operation.
- [x] Run Rust `dump` on every positive artifact in this milestone.

**Exit criteria:** The serializer produces byte-for-byte stable output, the LLVM-side validator catches malformed references before the VM does, and all M0 output remains unchanged except for intentional canonicalization documented in the target README.

## 6. M2 — Complete the supported scalar/control-flow slice

**Purpose:** Make the current scalar subset semantically explicit and usable after normal LLVM optimization passes.

**Files:**

- Modify: `llvm/lib/Target/CD/CDBytecodeEmitter.cpp`.
- Modify: `llvm/lib/Target/CD/CDTargetMachine.{h,cpp}` only if the pass pipeline needs target-specific configuration.
- Create: `llvm/test/CodeGen/CD/cdbc-scalar.ll`.
- Create: `llvm/test/CodeGen/CD/cdbc-fneg.ll` and `llvm/test/CodeGen/CD/cdbc-not.ll` for the first unary slice.
- Create: `llvm/test/CodeGen/CD/cdbc-control-flow.ll`.
- Create: `llvm/test/CodeGen/CD/cdbc-optimization.ll`.
- Modify: `llvm/lib/Target/CD/README.md`.
- Contract review: `cd-compiler/docs/bytecode-text-format.md` and `cd-compiler/vm-rs/src/vm.rs`.

Freeze the scalar policy before adding opcodes:

- `i1` maps to CD `bool`.
- finite `float`/`double` values map to CD `number`.
- integer values map to CD `number` only where the chosen width/sign semantics are documented and exactly representable; the target must not claim native integer wraparound when the VM stores an IEEE number.
- signed and unsigned integer predicates must not be merged silently. Unsupported unsigned overflow/order behavior is rejected until a CD representation exists.
- `fneg` maps to `negate`, boolean inversion maps to `not`, and only semantically equivalent casts map to `move`.
- `select`, `switch`, `indirectbr`, exception edges, poison/undef, and overflow-sensitive integer operations receive an explicit lowering or an explicit diagnostic; they are never serialized as an unrelated opcode.

- [x] Add the first unary operations with direct `cdbc 0.1` equivalents: `fneg` to `negate`, and `xor i1 <value>, true` (in either operand order) to `not`; reject other XOR shapes.
- [x] Add loop, multiple-predecessor PHI, critical-edge, and recursive-call fixtures.
- [x] Lower scalar `select` to conditional bytecode control flow so normal LLVM optimization can form selects without changing the CD value model.
- [x] Run direct `llc` emission at `-O0` and `-O2`, then run an explicit LLVM `default<O2>` middle-end pipeline; verify that mem2reg, constant folding, dead-code elimination, and CFG reshaping do not change observable CD behavior.
- [x] Add FileCheck failure fixtures for unsigned division and unsigned integer ordering, whose LLVM semantics cannot be represented by the current CD number policy.
- [x] Run positive artifacts through Rust `dump` and `run`, comparing output with a hand-written expected result.

Current M2 progress (2026-08-01): `cdbc-fneg.ll` covers `fneg` to `negate`,
`cdbc-not.ll` covers the restricted boolean inversion form, and
`cdbc-control-flow.ll` covers loop PHIs, multiple predecessors, a critical
edge, and recursive calls.  The unary and control-flow artifacts are accepted
by Rust `dump`; their `-O0`/`-O2` Rust `run` outputs match (`-2.5`, `true`, and
`10`, `1`, `2`, `120` respectively).  The broader optimization and unsupported
semantic-boundary fixtures now reject `udiv` and unsigned ordering predicates.
`cdbc-optimization.ll` covers alloca promotion, constant folding, dead-code
elimination, scalar-select lowering, and direct `llc -O0`/`-O2` emission plus
an explicit `default<O2>` pipeline; both optimized and unoptimized artifacts
are accepted by Rust `dump` and produce `42` and `7`.

**Exit criteria:** The documented scalar subset passes at `-O0` and `-O2`, loop/PHI/call behavior is covered, and every unsupported semantic boundary has a stable diagnostic test.

## 7. M3 — Introduce the TableGen-backed machine path

**Purpose:** Add a real LLVM machine-backend path only after the direct IR emitter's artifact and scalar semantics are stable. This stage is the answer to the TableGen question: TableGen describes CD machine instructions and register classes, while `CDBytecodeFormat` remains responsible for the `.cdbc 0.1` wire format.

The direct `ModulePass` path remains the default compatibility path. The new machine path is selected explicitly, for example with a target option such as `-mllvm -cd-backend=machine`, and its canonical output is compared with the direct path before it becomes the default. No empty `.td` scaffolding is accepted without a defined machine model.

**Design gate:** Before adding generated instruction files, record decisions for the CD virtual-value register class, constant/name table ownership, function and call representation, branch target patching, `ret void`/nil behavior, and the point at which a `MachineFunction` becomes a `cdbc` body. CD bytecode uses dynamic values and VM registers rather than physical CPU registers, so ordinary native register allocation cannot be assumed to solve this model. The recorded gate is [cd-bytecode-machine-backend.md](../cd-bytecode-machine-backend.md).

**Files:**

- Create: `llvm/lib/Target/CD/CD.td`.
- Create: `llvm/lib/Target/CD/CDInstrInfo.td`.
- Create: `llvm/lib/Target/CD/CDRegisterInfo.td`.
- Create: `llvm/lib/Target/CD/CDSubtarget.td`.
- Create: `llvm/lib/Target/CD/CDCallingConv.td` with the explicitly supported VM calling convention.
- Create: `llvm/lib/Target/CD/CDInstrInfo.{h,cpp}`.
- Create: `llvm/lib/Target/CD/CDRegisterInfo.{h,cpp}`.
- Create: `llvm/lib/Target/CD/CDSubtarget.{h,cpp}`.
- Create: `llvm/lib/Target/CD/CDISelLowering.{h,cpp}` and `CDISelDAGToDAG.cpp` for the first instruction-selection path.
- Create: `llvm/lib/Target/CD/CDAsmPrinter.{h,cpp}` or an equivalent machine-instruction serializer that consumes `CDBytecodeFormat`.
- Modify: `llvm/lib/Target/CD/CDTargetMachine.{h,cpp}` and `llvm/lib/Target/CD/CMakeLists.txt`.
- Modify: `llvm/lib/Target/CD/MCTargetDesc/CDMCTargetDesc.cpp` to use generated instruction/register/subtarget descriptions.
- Create: `llvm/test/CodeGen/CD/cdbc-machine.ll`.
- Create: `llvm/test/CodeGen/CD/Machine/*.mir`.
- Create: `llvm/utils/cd_bytecode_parity.py` and its unit test.
- Create: `llvm/test/CodeGen/CD/cdbc-machine-parity.list`.

- [x] Record the CD virtual-value register, module-table ownership, call/branch, and artifact-bridge decisions before creating generated instruction files.
- [x] Add `CDCommonTableGen` and generated `CDGenInstrInfo.inc`, `CDGenRegisterInfo.inc`, and `CDGenSubtargetInfo.inc` dependencies in CMake.
- [x] Define only the machine operations that have a stable `cdbc 0.1` mapping; do not use TableGen to hide unsupported arrays, maps, globals, or native calls.
- [x] Lower the existing scalar/control-flow subset to `MachineInstr` and serialize it through the typed artifact model from M1.
- [x] Add MIR tests for virtual-value registers, calls, branches, PHI lowering, function boundaries, and constant/name table ownership.
- [x] Produce direct and machine-path artifacts from the same LLVM IR, normalize only permitted table/index differences, and require identical VM behavior.
- [x] Keep `-filetype=obj` rejected and keep the machine path text-only until an object format is deliberately designed.

Current M3 progress (2026-08-01): the opt-in machine path bridges the
no-argument `@main` body and defined helper `MachineFunction` bodies through
the typed artifact model.  It covers scalar constants, arithmetic,
comparisons, scalar casts, `fneg`, boolean inversion, scalar `select`,
`nil`/`ret void`, `cd_print`/`print`, defined-function calls, scalar
parameters, and single-slot scalar storage.  Conditional and unconditional
branches now lower to TableGen-defined jump pseudo-instructions; PHI incoming
values are stored on unconditional edges or synthetic conditional edge blocks,
and symbolic machine block targets are patched to artifact instruction
offsets.  The new `cdbc-machine-control-flow.ll` fixture covers loop PHIs,
multiple predecessors, critical edges, repeated constants across edges, and
Rust VM output; the MIR fixture covers virtual-value registers and branch
operands.  The parity manifest now validates every supported machine fixture:
artifact-mode cases compare only canonicalized table/register indices, while
the machine-specific control-flow and `select` expansions use behavior-mode
checks.  Every case passes Rust `dump` and direct/machine `run` output parity.

**Exit criteria:** LLVM TableGen generates usable CD instruction/register/subtarget descriptions; `llc` can select the machine path explicitly; MIR and FileCheck tests pass; Rust `dump` accepts the result; and direct and machine paths agree on `cdbc 0.1` execution for the supported subset.

## 8. M4 — Define and implement the CD value ABI

**Purpose:** Add high-level bytecode operations without guessing how arbitrary LLVM pointers and aggregates should behave at runtime.

**Design decision:** Use target-specific CD IR operations, preferably `llvm.cd.*` intrinsics declared in `llvm/include/llvm/IR/IntrinsicsCD.td`, for dynamic CD values. Both the direct emitter and the TableGen machine path recognize those operations and reject ordinary aggregate/pointer instructions used as substitutes. The exact intrinsic signatures and ownership rules must be written in `docs/cd-bytecode-llvm-abi.md` before implementation.

The first design gate is recorded in
[`docs/cd-bytecode-llvm-abi.md`](../cd-bytecode-llvm-abi.md).  It fixes the
opaque CD-value token boundary and the `llvm.cd.string` constant contract
before any collection or pointer lowering is added.

**Files:**

- Create: `docs/cd-bytecode-llvm-abi.md`.
- Create or modify: `llvm/include/llvm/IR/IntrinsicsCD.td` and its include registration in `llvm/include/llvm/IR/Intrinsics.td`.
- Create: `llvm/lib/Target/CD/CDValueABI.{h,cpp}` if the intrinsic-to-bytecode mapping does not fit the artifact emitter.
- Modify: `llvm/lib/Target/CD/CDBytecodeFormat.{h,cpp}` and `CDBytecodeEmitter.{h,cpp}`.
- Create: `llvm/test/CodeGen/CD/cdbc-array.ll`,
  `llvm/test/CodeGen/CD/cdbc-array-errors.ll`, and the array-access fixtures.
- Create: `llvm/test/CodeGen/CD/cdbc-records.ll`.
- Create: `llvm/test/CodeGen/CD/cdbc-native.ll`.
- Synchronize, in the independent checkout: `cd-compiler/docs/bytecode-text-format.md`, `cd-compiler/vm-rs/src/format.rs`, `cd-compiler/vm-rs/src/bytecode.rs`, and `cd-compiler/vm-rs/src/vm.rs`.

Implement in this order:

1. Arrays, maps, `index`, `assign_index`, `len`, `assert_array`, and string constants.
2. Anonymous/named structs, `field`, `assign_field`, and the `typeOf` name-table contract.
3. Enum variants, `variant_tag`, and `variant_field` with an explicit payload order.
4. `native_call` with a name-table allowlist and an argument/result capability matrix shared by LLVM tests and the Rust VM.

- [ ] Define operand/result types, ownership/aliasing behavior, failure behavior, and constant/string encoding for every intrinsic.
- [ ] Add positive LLVM IR fixtures and malformed/unsupported intrinsic fixtures for each group.
- [ ] Update the C++ and Rust artifact validators before enabling execution.
- [ ] Run `llc -> dump -> run` for each group and compare with equivalent `.cd` programs emitted by the existing C++ compiler where the language feature has an equivalent.

Current M4 progress (2026-08-01): the first string group is implemented. The
`llvm.cd.string` intrinsic accepts private-linkage immutable byte globals,
including LLVM's canonical one-byte zero initializer for the empty string; it
validates the terminator, embedded-zero, and UTF-8 rules and lowers the payload
through the existing `constant` instruction. Direct and TableGen machine paths
share the helper and artifact model. Positive ASCII, UTF-8, escaping,
deduplication, and empty-string coverage, plus direct/machine malformed-input
parity, are present. String tokens are intentionally limited to local
materialization and `print` in this slice; function returns, PHI/`select`
propagation, and parameters remain a later ABI sub-slice.

The first collection constructor is now implemented as the `llvm.cd.array`
intrinsic.  LLVM's variadic intrinsic boundary uses an immediate `i32` element
count followed by payload operands; the count must match exactly, and the
variadic call type is explicit in textual IR.  Scalar, `nil`, string-token, and
array-token payloads lower to the existing `array` instruction.  Empty, mixed,
and nested arrays are covered by `cdbc-array.ll`; mismatched counts, ordinary
pointers, vectors, foreign-address-space nulls, poison, and non-immediate
counts are covered by `cdbc-array-errors.ll`.  The direct and machine paths
share `CDValueABI` validation, `CDBytecodeFormat`, and the Rust VM parity gate;
machine operand constants are materialized before `CD_ARRAY` so `dump` and
`run` observe the same values on both paths.  Array results may now be printed,
nested, indexed, measured, or asserted locally; mutation, return/parameter,
PHI/`select`, and ordinary pointer operations remain deferred.

The next access ABI gate fixes `llvm.cd.index(ptr, double) -> ptr`,
`llvm.cd.len(ptr) -> double`, and `llvm.cd.assert.array(ptr) -> ptr`.  The
collection operand must be an explicit CD dynamic-value token (or CD nil),
ordinary pointers remain rejected, and results stay local until the
function-value ABI is specified.  The access group is implemented only after
positive, malformed, Rust `dump`/`run`, runtime-error, and direct/machine parity
tests agree with the existing `index`, `len`, and `assert_array` VM operations.

The array-access group is now implemented.  `llvm.cd.index` returns a local
dynamic-value token, `llvm.cd.len` returns a `double` CD number, and
`llvm.cd.assert.array` emits the existing `assert_array` operation.  Positive
coverage in `cdbc-array-access.ll` exercises scalar and nested indexing,
length, assertion, direct/machine FileCheck, and the parity manifest; malformed
ordinary-pointer and intrinsic-signature cases are in
`cdbc-array-access-errors.ll`.  The nil assertion fixture preserves the Rust VM
runtime error (`for-in expects array, range, or map`) on both paths.  Maps and
dynamic-value function boundaries remain deferred.

The array-mutation slice now implements the overloaded
`llvm.cd.assign.index` intrinsic. Pointer and scalar value overloads lower to
the existing `assign_index` operation through both direct and machine paths;
ordinary pointers and aggregate substitutes remain rejected. Positive
dynamic-pointer and scalar fixtures, malformed capability/signature fixtures,
and an out-of-bounds runtime fixture are covered. The parity harness has an
explicit `runtime-error` mode that validates both VM diagnostics, and the
assignment fixtures participate in direct/machine behavior parity. The nested
Rust VM checkout is unchanged because its existing parser, verifier, and
runtime already implement `assign_index`.

**Exit criteria:** Every newly emitted opcode is documented, parsed, verified, and executed by the Rust VM; collection mutation and failure behavior are covered; ordinary LLVM aggregates remain rejected unless they use the defined CD ABI.

### Narrow M4 slice: `llvm.cd.assign.index` (2026-08-01)

**Goal:** Lower only the overloaded array/map assignment intrinsic to the
existing `cdbc 0.1` `assign_index` operation in the direct and opt-in machine
paths, while preserving the explicit CD-value ABI and Rust VM contract.

**Files:**

- Modify: `llvm/include/llvm/IR/IntrinsicsCD.td`.
- Modify: `llvm/lib/Target/CD/CDValueABI.{h,cpp}`.
- Modify: `llvm/lib/Target/CD/CDBytecodeFormat.{h,cpp}`.
- Modify: `llvm/lib/Target/CD/CDBytecodeEmitter.cpp`.
- Modify: `llvm/lib/Target/CD/CDInstrInfo.td`.
- Modify: `llvm/lib/Target/CD/CDMachineBytecodeEmitter.cpp`.
- Modify: `llvm/utils/cd_bytecode_parity.py` and
  `llvm/utils/cd_bytecode_parity_test.py`.
- Modify: `llvm/test/CodeGen/CD/cdbc-array-assign.ll` and
  `llvm/test/CodeGen/CD/cdbc-machine-parity.list`.
- Create: `llvm/test/CodeGen/CD/cdbc-array-assign-scalar.ll`,
  `llvm/test/CodeGen/CD/cdbc-array-assign-errors.ll`, and
  `llvm/test/CodeGen/CD/cdbc-array-assign-runtime.ll`.
- Modify: `llvm/test/CodeGen/CD/cdbc-array-access-runtime.ll` only if needed to
  make its VM error expectation explicit in the parity manifest.
- Modify: `docs/cd-bytecode-llvm-abi.md`,
  `docs/cd-bytecode-machine-backend.md`, and `llvm/lib/Target/CD/README.md`.

The intrinsic declaration is:

```tablegen
def int_cd_assign_index : DefaultAttrsIntrinsic<
    [llvm_any_ty], [llvm_ptr_ty, llvm_double_ty, LLVMMatchType<0>]>;
```

The textual IR tests use the base intrinsic spelling accepted by LLVM's
intrinsic parser. LLVM APIs may materialize a type suffix for an overloaded
declaration; that suffix still identifies the same intrinsic ID. The two
positive modules remain separate so that each textual declaration has one
unambiguous overload.

The lowering validator must enforce all of these conditions before constructing
`CDInstruction::assignIndex`: exactly three arguments; an explicit CD token or
address-space-zero null collection; a `double` index; a scalar, address-space-
zero null, or explicit CD-token value; and a scalar result with exactly the
value type or an address-space-zero pointer result for a dynamic value.  An
ordinary pointer, aggregate, vector, foreign-address-space null, mismatched
result, or malformed signature remains a target diagnostic.  An assignment
result is a CD token only for the pointer overload, so it participates in the
existing local CD-value propagation rules without widening ordinary pointer
support.

The typed artifact shape is:

```text
rD = assign_index rCollection, rIndex, rValue
```

`CDOpcode::AssignIndex`, its factory, validator, serializer, `CD_ASSIGN_INDEX`
pseudo, direct lowering, machine lowering, and machine artifact bridge must all
use that exact three-operand/result shape.  The nested `cd-compiler/` checkout
is read-only for this slice because its parser, verifier, and runtime already
implement the opcode.

The parity manifest gains this narrow syntax in addition to `artifact` and
`behavior`:

```text
runtime-error <input.ll> "<expected-error-substring>"
```

For a runtime-error entry, the harness emits direct and machine artifacts,
requires both VM `dump` commands to succeed, requires both VM `run` commands
to fail, and compares the exact diagnostic after checking the expected
substring.  Add the existing `cdbc-array-access-runtime.ll` with
`for-in expects array, range, or map`, and add the new assignment bounds
fixture with `array index out of range`.  This keeps LLVM lit tests isolated
from the sibling checkout while making VM runtime parity an explicit opt-in
integration gate.

- [x] Record the red baseline by running `build-cd/bin/llc -mtriple=cd-unknown-unknown llvm/test/CodeGen/CD/cdbc-array-assign.ll`; the current unsupported declaration is treated as an ordinary call and fails with `CD target does not support LLVM instruction: call` (exit 134).
- [x] Add the overloaded intrinsic and verify its generated enum/name and
  pointer/scalar textual spellings with LLVM IR parsing before changing the
  target emitters.
- [x] Add the direct and machine typed `assign_index` shape, shared ABI
  validation, CD-value propagation, and both lowering paths.
- [x] Add dynamic-pointer, scalar, malformed-signature/capability, and runtime
  bounds fixtures; add positive assignment to the parity manifest.
- [x] Extend and unit-test the parity harness's explicit `runtime-error` mode,
  including the existing array-access runtime fixture.
- [x] Run focused lit, Rust VM `dump`/`run`, direct/machine artifact and
  runtime parity, and `git diff --check` gates without modifying the nested
  checkout.

**Remaining boundaries after this slice:** map assignment is accepted only as
far as an explicit CD dynamic token can reach the intrinsic; ordinary pointers,
aggregates, records, native calls, function/parameter/PHI/select propagation,
and implicit mutation through LLVM stores remain rejected.  No new Rust opcode,
artifact version, object output, or default machine backend is introduced.

### Narrow M4 slice: `llvm.cd.map` (2026-08-01)

**Goal:** Lower only the explicit map constructor to the existing `cdbc 0.1`
`map` operation through the direct and opt-in machine paths.

**ABI gate:** `llvm.cd.map(i32 entryCount, ...) -> ptr` uses an immediate entry
count and exactly two variadic operands per entry in source order. Keys are
scalar, CD nil, or explicit string tokens; values use the existing scalar,
nil, string, array, map, and local dynamic-value capability matrix. Duplicate
runtime-equal keys preserve the Rust VM's last-value-wins/first-position
ordering. The result is a local opaque CD map token and ordinary LLVM pointers
remain invalid.

**Files:** modify the CD intrinsic/ABI/artifact/direct/machine layers and
documentation; create `cdbc-map.ll`, malformed map fixtures, and runtime
resource/error fixtures; extend the parity manifest for positive and
`runtime-error` cases. The nested Rust VM checkout is read-only unless a
contract gap is proven.

- [x] Record the map key/value, duplicate-key, aliasing, ordering, and failure
  behavior gate in the ABI and roadmap documents.
- [x] Add the red constructor fixture and verify it fails before lowering.
- [x] Add `llvm.cd.map`, `CDOpcode::Map`, `CD_MAP`, shared validation, and both
  direct/machine artifact paths.
- [x] Add positive, malformed, duplicate-key, nested-value, and runtime
  error parity coverage. The positive fixture covers empty maps, nil/string/
  scalar keys, nested array/map values, and last-value-wins duplicate keys;
  the runtime fixture preserves the VM's missing-key error.
- [x] Run focused lit, Rust `dump`/`run`, full direct/machine parity, and
  `git diff --check`, then commit the slice.

The map slice is now implemented. Both lowering paths share the map ABI
validator and artifact serializer, and the machine path materializes all
constant operands before inserting `CD_MAP` so execution order remains valid.
The direct/machine parity manifest covers the positive constructor and the
missing-key runtime error; the nested Rust VM checkout remains unchanged.

### Narrow M4 slice: record values (2026-08-02)

**Goal:** Lower the first record-value ABI group through the existing
`cdbc 0.1` `struct`, `field`, and `assign_field` operations in the direct and
opt-in machine paths.

**ABI gate:** `llvm.cd.struct(ptr typeNameOrNull, i32 fieldCount, ...) -> ptr`
uses an anonymous address-space-zero `ptr null` or a private, constant,
non-empty UTF-8 type-name global, followed by exactly one private field-name
global and value operand per field. Field values are scalar, CD nil, or values
produced by explicit CD intrinsics. `llvm.cd.field` reads a field with a scalar
or address-space-zero CD-value result; `llvm.cd.assign.field` has an overloaded
result that must match its assigned value and mutates the existing struct.
Names are name-table metadata, not CD string values, and ordinary pointers are
never inferred to be records.

**Files:** modify the CD intrinsic/ABI/artifact/direct/machine layers and the
three ABI documents; create positive named/anonymous, dynamic-value, runtime,
and malformed record fixtures; extend the direct/machine parity manifest. The
nested Rust VM checkout is read-only because its existing parser, verifier, and
runtime already implement the three artifact operations.

- [x] Record the record operand/result, name-table, field-order, mutation, and
  runtime-failure behavior in the ABI and roadmap documents.
- [x] Add `llvm.cd.struct`, `llvm.cd.field`, and `llvm.cd.assign.field`, shared
  ABI validation, `Struct`/`Field`/`AssignField` artifact shapes, and both
  direct/machine lowering paths.
- [x] Add named/anonymous positive fixtures, nested dynamic field values,
  ordinary-pointer/name/value rejection fixtures, and a missing-field runtime
  fixture.
- [x] Add record artifact, behavior, and runtime-error entries to the parity
  manifest; verify the exact shared VM diagnostic.
- [x] Run focused lit, Rust VM `dump`/`run`, direct/machine parity, and
  `git diff --check` gates without modifying the nested checkout.

The record-value slice is now implemented. The machine path materializes field
value registers before inserting `CD_STRUCT`, preserving definition-before-use
ordering for nested arrays and other dynamic values. The Rust VM checkout and
the `cdbc 0.1` version remain unchanged. Native calls and dynamic values
crossing ordinary function boundaries remain future M4 decisions.

### Narrow M4 slice: enum variant values (2026-08-02)

**Goal:** Lower the explicit enum-variant value ABI through the existing
`cdbc 0.1` `variant`, `variant_tag`, and `variant_field` operations in the
direct and opt-in machine paths.

**ABI gate:** `llvm.cd.variant(ptr enumName, ptr variantName, i32 fieldCount,
...) -> ptr` requires private, constant, non-empty UTF-8 name globals, an
immediate payload count, and exactly one scalar, CD nil, or explicit CD-value
payload per field. `llvm.cd.variant.tag(ptr value, ptr enumName, ptr
variantName) -> i1` checks an explicit CD value against the two names.
`llvm.cd.variant.field(ptr value, i32 index)` is overloaded for scalar or
address-space-zero CD-value results and requires an immediate index. Name
operands are name-table metadata; ordinary pointers and LLVM aggregates are
never inferred to be variants.

**Files:** modify the CD intrinsic/ABI/artifact/direct/machine layers and the
three ABI documents; create positive, dynamic-value, malformed, and runtime
variant fixtures; extend the direct/machine parity manifest. The nested Rust
VM checkout is read-only because its parser, verifier, and runtime already
implement the three artifact operations.

- [x] Add positive and dynamic fixtures first, then run the old target to
  record the missing-intrinsic/lowering red baseline.
- [x] Add `llvm.cd.variant*`, shared ABI validation, `Variant`/`VariantTag`/
  `VariantField` artifact shapes, and both direct/machine lowering paths.
- [x] Reject count mismatches, ordinary-pointer payload/value operands,
  non-name globals, non-immediate field indexes, and invalid result types with
  target diagnostics.
- [x] Add artifact/behavior parity entries and runtime-error entries for
  non-variant access and out-of-bounds payload access.
- [x] Run focused lit, Rust VM `dump`/`run`, direct/machine parity, the full CD
  lit suite, `git diff --check`, and the relevant Rust VM tests without
  modifying the nested checkout.

The enum-variant slice is now implemented. Both lowering paths share the
variant ABI validator and artifact bridge, and the machine path materializes
payload registers before inserting `CD_VARIANT`. Positive, nested dynamic,
malformed, non-variant, and out-of-bounds fixtures pass direct/machine parity
against the existing Rust VM contract. The nested Rust VM checkout and the
`cdbc 0.1` version remain unchanged.

### Narrow M4 slice: bounded native calls (2026-08-02)

**Goal:** Lower the non-callback native stdlib operations whose LLVM argument
and result types have an explicit capability matrix:
`floor`, `ceil`, `sqrt`, `str`, `typeOf`, `hash`, and `range`.

**ABI gate:** `llvm.cd.native(ptr name, ...)` requires a direct private,
constant, non-empty UTF-8 name global. `floor`, `ceil`, and `sqrt` take one
`double` and return `double`; `str` and `typeOf` take one scalar or CD value
and return an address-space-zero CD string pointer; `hash` takes one scalar or
CD value and returns `double`; `range` takes one to three `double` values and
returns an address-space-zero CD range pointer. The wire form is
`rD = native_call nName [rArg0, ...]`. Callback helpers, collection mutation,
`substr`, `charAt`, callback names, unknown names, and ordinary pointers remain
rejected.

**Files:** modify the CD intrinsic/ABI/artifact/direct/machine layers and the
three target ABI documents; create positive, malformed, and runtime native
fixtures; extend the direct/machine parity manifest. The nested Rust VM
checkout is read-only because its existing native allowlist and executor
already implement these names.

- [x] Record the name-table identity, exact scalar/CD-value capability matrix,
  callback boundary, and runtime failure behavior in the ABI documents.
- [x] Run the old target against the positive fixtures and record its red
  baseline: native name globals are rejected as unused CD string/name globals
  before native lowering begins.
- [x] Add `llvm.cd.native`, shared name-specific validation, the
  `NativeCall` artifact shape, and both direct/machine lowering paths.
- [x] Reject unknown/callback names, wrong arity/result types, ordinary
  pointers, and invalid name globals with target diagnostics.
- [x] Add artifact/behavior parity and runtime-error entries for the positive
  and `sqrt` failure fixtures.
- [x] Run focused/full CD lit, Rust `dump`/`run`, cargo tests, direct/machine
  parity, and `git diff --check` without modifying the nested checkout.

Verification on 2026-08-02: focused native lit `4/4`, complete CD lit `55/55`,
direct/machine parity `33/33`, Rust VM cargo tests `73 + 3 + 8`, and the
nested checkout remained clean.

### Narrow M4 follow-up: string native helpers (2026-08-03)

**Goal:** Extend the existing bounded `native_call` ABI with `substr` and
`charAt` without adding a wire opcode, intrinsic, artifact field, or nested VM
change.

**ABI gate:** `substr` takes one explicit CD dynamic-value token and two
`double` operands and returns an address-space-zero CD string pointer;
`charAt` takes one explicit CD dynamic-value token and one `double` operand and
returns the same pointer type. The target validates only the token and LLVM
shape. The Rust VM owns runtime string typing, integer-valuedness, Unicode
scalar indexing, and bounds diagnostics.

- [x] Extend the shared native capability matrix and artifact allowlist.
- [x] Add UTF-8 positive coverage, including empty substrings and multi-byte
  scalar boundaries.
- [x] Add malformed direct/machine coverage for arity, operand, and result
  shapes.
- [x] Add direct/machine runtime-error parity for substring length and
  character-index failures.
- [x] Update the ABI, machine-backend, target README, and roadmap contracts.

Verification on 2026-08-03: focused string-native fixtures passed; the full CD
lit suite passed with `72` supported tests and `1` expected unsupported VM
integration test; the expanded direct/machine parity manifest passed all `49`
entries; Rust VM cargo tests remained `73 + 3 + 8`; and the nested checkout
remained clean.

### Narrow M4/M8 follow-up: first callback native, `map` (2026-08-04)

**Goal:** Admit only the Rust VM's `map` callback helper through the existing
`native_call` artifact operation. The slice does not add a callback opcode,
artifact field, or nested VM change.

**Callback ABI gate:** `llvm.cd.native(ptr name, ptr value, ptr callback) ->
ptr` requires a proven CD token for `value` and a direct defined LLVM function
for `callback`. The callback must have exactly one address-space-zero pointer
parameter marked by `cd.value.params="0"` and an address-space-zero pointer
return marked by `cd.value.return`. Declarations, casts, indirect function
pointers, `@main`, ordinary pointer values, and the other callback names remain
rejected. The VM performs the runtime array/type check and callback arity check.

The Rust matrix used for this choice is: `map` requires two arguments and a
one-argument callback, returns a fresh array, returns an empty fresh array for
empty input, snapshots input elements, preserves shared dynamic handles passed
to the callback, charges one native checkpoint per element plus callback body
instructions, charges one output array plus its elements, and checks
cancellation before instruction/resource growth. `flatMap`, `any`, `all`,
`count`, `find`, `findIndex`, and `reduce` remain rejected.

- [x] Audit the Rust dispatch, callback frame, budget, cancellation, empty-input,
  and fresh-output behavior before selecting the helper.
- [x] Add shared direct/machine validation for `map` and its marked callback
  function shape; materialize the callback with `make_function`.
- [x] Add positive empty/non-empty, malformed callback/shape, runtime type-error,
  and direct/machine parity fixtures.
- [x] Update the ABI, machine-backend, target README, and parity records
  without changing `cdbc 0.1` or the nested checkout.

Verification on 2026-08-04: focused callback lit passed `3/3`; direct and
machine artifacts both dumped and produced `[]` then `[one, two]`; the runtime
type-error parity case matched `map expects array as first argument`; the Rust
callback budget test remained covered by the unchanged nested VM; and the
nested checkout remained clean.

### Narrow M4/M8 follow-up: callback native, `filter` (2026-08-04)

**Goal:** Admit only the Rust VM's `filter` callback helper through the existing
`native_call` artifact operation. This follow-up adds no callback opcode,
artifact field, or nested VM change.

**Callback ABI gate:** `llvm.cd.native(ptr name, ptr value, ptr callback) ->
ptr` requires a proven CD token for `value` and a direct defined LLVM function
for `callback`. The callback must have exactly one address-space-zero pointer
parameter marked by `cd.value.params="0"` and an exact `i1` return. It must not
carry `cd.value.return`, which is reserved for CD pointer returns. Declarations,
casts, indirect function pointers, `@main`, ordinary pointer values, and the
remaining callback names stay rejected. The VM performs the runtime array/type,
predicate-result, and callback-arity checks.

The Rust matrix used for this choice is: `filter` requires two arguments and a
one-argument boolean predicate, returns a fresh shallow array, returns an empty
fresh array for empty input or an always-false predicate, snapshots input
elements, preserves the original handles for retained elements, and invokes
the predicate from left to right with the existing callback budget and
cancellation behavior.

- [x] Reuse the shared direct/machine callback validation and function-value
  materialization path with the filter-specific `i1` result rule.
- [x] Add positive empty/all/none, malformed callback/shape, runtime type-error,
  and direct/machine parity fixtures.
- [x] Update the ABI, machine-backend, target README, verification matrix, and
  active development plan without changing `cdbc 0.1` or the nested checkout.

Verification on 2026-08-04: focused filter fixtures passed; the full local
gate passed with 80 lit tests (79 passed, 1 unsupported), parity 55/55, parity
unit 14/14, module-link unit 5/5, and Rust VM tests `73 + 3 + 8`; the nested
checkout remains clean.

### Narrow M4/M8 follow-up: predicate callbacks, `any` and `all` (2026-08-07)

**Goal:** Admit the Rust VM's boolean predicate helpers `any` and `all`
through the existing `native_call` artifact operation. This slice adds no
callback opcode, artifact field, or nested VM change.

**Callback ABI gate:**
`llvm.cd.native(ptr name, ptr value, ptr predicate) -> i1` requires a proven
CD token for `value` and a direct defined LLVM function with exactly one
address-space-zero pointer parameter marked by `cd.value.params="0"`. The
predicate returns exactly `i1` and must not carry `cd.value.return`.
Declarations, casts, indirect function pointers, `@main`, ordinary pointer
values, and `flatMap`, `count`, `find`, `findIndex`, and `reduce` remain
rejected. The VM performs the runtime array/type, callback-arity, predicate
result, budget, cancellation, and short-circuit checks.

The Rust matrix used for this choice is: `any` returns `false` for an empty
array and stops at the first true predicate result; `all` returns `true` for
an empty array and stops at the first false predicate result. Both snapshot
the input elements, invoke predicates left to right, and use the existing
native checkpoint and callback frame behavior.

- [x] Reuse the shared direct/machine callback validation and function-value
  materialization path with exact `i1` native results.
- [x] Add positive empty/matching/rejecting, malformed shape/pointer/callback,
  runtime type-error, and direct/machine parity fixtures.
- [x] Update the ABI, machine-backend, target README, verification matrix, and
  active development plan without changing `cdbc 0.1` or the nested checkout.

Verification on 2026-08-07: focused predicate lit passed `4/4`; the local
suite passed `83` tests with `1` expected unsupported VM integration case;
direct/machine parity passed `58/58`; parity unit tests passed `14/14`,
module-link unit tests passed `5/5`, Rust VM tests passed `73 + 3 + 8`, and
the nested checkout remained clean. The hosted gate remains pending the
workflow fix that builds LLVM's `not` test utility.

### Narrow M4/M8 follow-up: predicate callback, `count` (2026-08-08)

**Goal:** Admit the Rust VM's boolean predicate helper `count` through the
existing `native_call` artifact operation. This slice adds no callback opcode,
artifact field, or nested VM change.

**Callback ABI gate:**
`llvm.cd.native(ptr name, ptr value, ptr predicate) -> double` requires a
proven CD token for `value` and a direct defined LLVM function with exactly one
address-space-zero pointer parameter marked by `cd.value.params="0"`. The
predicate returns exactly `i1` and must not carry `cd.value.return`.
Declarations, casts, indirect function pointers, `@main`, ordinary pointer
values, and `flatMap`, `find`, `findIndex`, and `reduce` remain rejected. The VM
performs the runtime array/type, callback-arity, predicate-result, budget, and
full-traversal checks.

The Rust matrix used for this choice is: `count` returns `0` for an empty
array, snapshots input elements, invokes the predicate left to right for every
element, increments the numeric result for each `true`, and uses the existing
native checkpoint and callback frame behavior.

- [x] Reuse the shared direct/machine callback validation and function-value
  materialization path with an exact `double` native result.
- [x] Add positive empty/all/none, malformed shape/pointer/callback,
  non-array runtime-error, and direct/machine parity fixtures.
- [x] Update the ABI, machine-backend, target README, verification matrix, and
  active development plan without changing `cdbc 0.1` or the nested checkout.

Verification on 2026-08-08: focused count/predicate lit passed `3/3`; the full
local suite passed `85` tests with `1` expected unsupported VM integration case;
direct/machine parity passed `60/60`; parity unit tests passed `14/14`,
module-link unit tests passed `5/5`, Rust VM tests passed `73 + 3 + 8`, and the
nested checkout remained clean. The hosted M7 gate remains pending publication
of the workflow fix that builds LLVM's `not` test utility.

### Narrow M4/M8 follow-up: predicate callback, `find` (2026-08-08)

**Goal:** Admit the Rust VM's first-match predicate helper `find` through the
existing `native_call` artifact operation. This slice adds no callback opcode,
artifact field, or nested VM change.

**Callback ABI gate:**
`llvm.cd.native(ptr name, ptr value, ptr predicate) -> ptr` requires a proven CD
token for `value` and a direct defined LLVM function with exactly one
address-space-zero pointer parameter marked by `cd.value.params="0"`. The
predicate returns exactly `i1` and must not carry `cd.value.return`; the native
result is an address-space-zero `ptr` CD value. Declarations, casts, indirect
function pointers, `@main`, ordinary pointer values, and `flatMap`, `findIndex`,
and `reduce` remain rejected. The VM performs the runtime array/type,
callback-arity, predicate-result, snapshot, budget, and first-match checks.

The Rust matrix used for this choice is: `find` snapshots the input elements,
invokes the predicate from left to right, returns the first matching element,
and returns `nil` for an empty or unmatched array. It checks the existing native
checkpoint before each callback and preserves the VM's callback frame and
cancellation behavior.

- [x] Reuse the shared direct/machine callback validation and function-value
  materialization.
- [x] Add positive empty/first-match/no-match, malformed shape/pointer/callback,
  non-array runtime-error, and direct/machine parity fixtures.
- [x] Update the ABI, machine-backend, target README, verification matrix, and
  active development plan without changing `cdbc 0.1` or the nested checkout.

Verification on 2026-08-08: focused find/predicate lit passed `3/3`; the full
local suite passed `87` tests with `1` expected unsupported VM integration case;
direct/machine parity passed `62/62`; parity unit tests passed `14/14`,
module-link unit tests passed `5/5`, the explicit VM integration lit passed
`1/1`, Rust VM tests passed `73 + 3 + 8`, and the nested checkout remained
clean. The hosted M7 gate remains pending publication of the workflow fix that
builds LLVM's `not` test utility.

### Narrow M4/M8 follow-up: predicate callback, `findIndex` (2026-08-08)

**Goal:** Admit the Rust VM's zero-based first-match helper `findIndex` through
the existing `native_call` artifact operation. This slice adds no callback
opcode, artifact field, or nested VM change.

**Callback ABI gate:**
`llvm.cd.native(ptr name, ptr value, ptr predicate) -> double` requires a proven
CD token for `value` and a direct defined LLVM function with exactly one
address-space-zero pointer parameter marked by `cd.value.params="0"`. The
predicate returns exactly `i1` and must not carry `cd.value.return`; the native
result is an exact `double`. Declarations, casts, indirect function pointers,
`@main`, ordinary pointer values, and `flatMap` and `reduce` remain rejected.
The VM performs the runtime array/type, callback-arity, predicate-result,
snapshot, budget, and zero-based first-match checks.

The Rust matrix used for this choice is: `findIndex` snapshots the input
elements, invokes the predicate from left to right, returns the zero-based index
of the first matching element, and returns `-1` for an empty or unmatched array.
It checks the existing native checkpoint before each callback and preserves the
VM's callback frame and cancellation behavior.

- [x] Reuse the shared direct/machine callback validation and function-value
  materialization.
- [x] Add positive empty/first-match/no-match, malformed shape/pointer/callback,
  non-array runtime-error, and direct/machine parity fixtures.
- [x] Update the ABI, machine-backend, target README, verification matrix, and
  active development plan without changing `cdbc 0.1` or the nested checkout.

Verification on 2026-08-08: focused findIndex/predicate lit passed `3/3`; the
full local suite passed `89` tests with `1` expected unsupported VM integration
case; direct/machine parity passed `64/64`; parity unit tests passed `14/14`,
module-link unit tests passed `5/5`, Rust VM tests passed `73 + 3 + 8`, and the
nested checkout remained clean. The hosted M7 gate remains pending publication
of the workflow fix that builds LLVM's `not` test utility.

### Narrow M4/M8 follow-up: callback native, `flatMap` (2026-08-08)

**Goal:** Admit the Rust VM's one-level array-flattening helper `flatMap`
through the existing `native_call` artifact operation. This slice adds no
callback opcode, artifact field, `.cdbc 0.1` version, or nested VM change.

**Callback ABI gate:**
`llvm.cd.native(ptr name, ptr value, ptr callback) -> ptr` requires a proven CD
token for `value` and a direct defined LLVM function with exactly one
address-space-zero pointer parameter marked by `cd.value.params="0"`. The
callback returns an address-space-zero pointer marked by `cd.value.return`, and
the native result is an exact address-space-zero `ptr`. Declarations, casts,
indirect function pointers, `@main`, ordinary pointer values, and `reduce`
remain rejected.

The Rust matrix used for this choice is: `flatMap` snapshots the input array,
invokes the callback from left to right, requires each callback result to be an
array, appends its elements exactly one level deep, and returns a fresh array.
It checks the existing native checkpoint before each callback and flattened
element, preserving the VM's callback frame, budget, cancellation, and failure
behavior.

- [x] Reuse the shared direct/machine callback validation and function-value
  materialization.
- [x] Add positive empty/nested-array, malformed shape/pointer/callback,
  non-array runtime-error, and direct/machine parity fixtures.
- [x] Update the ABI, machine-backend, target README, verification matrix, and
  active development plan without changing `cdbc 0.1` or the nested checkout.

Verification on 2026-08-08: focused flatMap lit passed `3/3`; the full local CD
suite passed `92` tests with `1` expected unsupported VM integration case;
direct/machine parity passed `66/66`; parity unit tests passed `14/14`,
module-link unit tests passed `5/5`, Rust VM tests passed `73 + 3 + 8`, and the
nested checkout remained clean. The hosted M7 gate remains pending publication
of the workflow fix that builds LLVM's `not` test utility.

### Narrow M4/M8 follow-up: callback native, `reduce` (2026-08-08)

**Goal:** Admit the Rust VM's accumulator-threading helper `reduce` through the
existing `native_call` artifact operation. This slice adds no callback opcode,
artifact field, `.cdbc 0.1` version, or nested VM change.

**Callback ABI gate:**
`llvm.cd.native(ptr name, ptr array, value initial, ptr callback) -> ptr` requires
a proven CD array token, a scalar or proven CD dynamic-value initial operand,
and a direct defined LLVM function with exactly two address-space-zero pointer
parameters marked by `cd.value.params="0,1"`. The callback returns an
address-space-zero pointer marked by `cd.value.return`, and the native result is
an exact address-space-zero `ptr`. Declarations, casts, indirect callbacks,
`@main`, ordinary pointer operands, and incomplete parameter markers remain
rejected.

The Rust matrix used for this choice is: `reduce` snapshots the input array,
returns the initial value for empty input, invokes the callback left to right
with `(accumulator, item)`, and threads each callback result into the next
iteration. Callback frames, native checkpoints, budget, cancellation, and
runtime array checks remain VM-owned.

- [x] Generalize shared callback validation to the two marked CD parameters and
  reuse direct/machine function-value materialization.
- [x] Add positive empty/left-to-right, malformed shape/pointer/callback,
  non-array runtime-error, and direct/machine parity fixtures.
- [x] Update the ABI, machine-backend, target README, verification matrix, and
  active development plan without changing `cdbc 0.1` or the nested checkout.

Verification on 2026-08-08: focused reduce lit passed `4/4`; the full local CD
suite and parity counts are refreshed by the final gate for this slice. Hosted
run `31245584718` for the preceding `flatMap` baseline failed during clean
runner test setup because `llvm-config`, `llvm-readobj`, and `split-file` were
not built; the workflow fix is locally verified and awaits publication/rerun.

### Narrow M8 follow-up: dynamic CD `select` (2026-08-08)

**Goal:** Propagate proven dynamic CD tokens through an LLVM SSA `select`
without adding a wire opcode, a new artifact field, a `.cdbc 0.1` version, or
a nested VM change.

The accepted shape is an `i1` condition selecting between two identical
address-space-zero `ptr` arms with proven CD provenance. The result inherits
that provenance and lowers through existing `jump_if_false`, `move`, and
`jump` artifact operations in both emitters. Ordinary pointers, foreign address
spaces, mixed proven/unproven arms, PHI propagation, and dynamic local storage
remain rejected.

- [x] Extend shared provenance recognition to dynamic `select` results.
- [x] Reuse direct and machine select lowering without changing the artifact.
- [x] Add positive direct/machine behavior parity and ordinary/foreign/mixed
  pointer diagnostics.
- [x] Update the ABI, machine-backend, target README, verification matrix, and
  active development plan.

Verification on 2026-08-08: focused dynamic-select lit passed `2/2`; the full
local gate and parity manifest were refreshed to `98` lit fixtures (`97 passed,
1 unsupported`) and `69/69` parity entries. The nested VM checkout remained
clean; hosted run `31245584718` remains a failed pre-fix baseline pending
publication of the tool-closure workflow commit.

### Narrow M8 follow-up: dynamic CD PHI (2026-08-08)

**Goal:** Propagate proven dynamic CD tokens through LLVM SSA `phi` nodes while
reusing the existing edge-store and block-entry-load artifact operations.

The accepted shape is a non-empty address-space-zero `ptr` PHI whose every
incoming value has proven CD provenance. This includes CD nil, explicit
intrinsic results, marked parameters, marked-return calls, dynamic `select`
results, and loop-carried PHIs. Ordinary pointers, foreign address spaces,
`undef`, poison, and mixed proven/unproven incoming values remain target errors.
The shared recursive classifier tracks back-edges without treating an
unproven terminal value as a CD token.

- [x] Extend shared provenance recognition to dynamic PHI values with
  recursion protection for loop-carried edges.
- [x] Allow direct and machine PHI allocation and edge lowering to carry proven
  dynamic values through existing `store_var`/`load_var` operations.
- [x] Add branch, loop-carried, ordinary, foreign, mixed, `undef`, and poison
  direct/machine fixtures, plus behavior parity for the positive case.
- [x] Update the ABI, machine-backend, target README, verification matrix, and
  active development plan without changing `cdbc 0.1` or the nested VM.

Verification on 2026-08-08: focused dynamic-PHI lit passed `2/2`; the full
local gate passed `99/100` fixtures with one expected unsupported VM case;
direct/machine parity passed `70/70`; the Rust VM groups passed `73 + 3 + 8`;
module-link direct/machine integration passed; and the nested VM checkout
remained clean. Hosted run `31245584718` remains the failed pre-fix baseline
pending publication and rerun of the tool-closure workflow fix.

### Narrow M8 follow-up: dynamic CD one-slot local storage (2026-08-09)

**Goal:** Carry proven dynamic CD values through one explicitly bounded LLVM
`alloca` slot using the existing `load_var` and `store_var` artifact operations.

The accepted shape is a single-element, address-space-zero `alloca ptr` with
only direct non-volatile, non-atomic load/store users. Every store value must
have proven CD provenance. A CFG definite-initialization walk must prove that
each load follows a proven store on every path; the alloca address itself is
storage and never a CD token. Branch-complete initialization is accepted while
uninitialized and partially initialized loads remain rejected. Escaped
allocas, GEP/bitcast aliases, ordinary pointers, and unsupported access modes
remain target errors.

- [x] Add shared storage-shape and definite-store provenance validation.
- [x] Allow direct and machine dynamic pointer load/store lowering through the
  existing `load_var`/`store_var` operations.
- [x] Add straight-line, replacement, branch-complete, uninitialized,
  partial-init, unproven, escaped, self-address, and volatile fixtures plus
  direct/machine behavior parity.
- [x] Update the ABI, machine-backend, target README, verification matrix, and
  active development/decision records without changing `cdbc 0.1` or the
  nested VM.

Verification on 2026-08-09: focused dynamic-storage lit passed `2/2`;
direct/machine parity passed `71/71`; the Rust VM groups remained `73 + 3 + 8`;
and the nested VM checkout remained clean. The full local gate and module-link
integration are refreshed by the final gate for this slice. Hosted run
`31245584718` remains the failed pre-fix baseline pending publication and rerun
of the tool-closure workflow fix.

## 9. M5 — Add source-backed debug metadata

**Purpose:** Preserve source locations and runtime diagnostics across LLVM lowering without fabricating source text that is not present in LLVM IR.

**Files:**

- Create: `llvm/lib/Target/CD/CDDebugInfo.{h,cpp}`.
- Modify: `llvm/lib/Target/CD/CDBytecodeFormat.{h,cpp}` and `CDBytecodeEmitter.cpp`.
- Create: `llvm/test/CodeGen/CD/cdbc-debug-sources.ll`,
  `llvm/test/CodeGen/CD/cdbc-debug-locations.ll`, and
  `llvm/test/CodeGen/CD/cdbc-debug-runtime.ll`.
- Modify: `llvm/lib/Target/CD/README.md` and `docs/cd-bytecode-llvm-abi.md`.
- Synchronize, in the independent checkout: `cd-compiler/vm-rs/src/format.rs`, `cd-compiler/vm-rs/src/vm.rs`, and debug/trace tests.

Use LLVM `DILocation`/`DIFile` for line and column identity. Because ordinary LLVM debug metadata does not contain the original source bytes, emit `debug_sources` only when a defined `!cd.sources` named-metadata record supplies the display path, canonical module identity, and exact source text. Without that record, the target may emit no debug sections while retaining correct program execution.

- [x] Define the `!cd.sources` record and reject malformed records, duplicate source identities, empty paths/module identities, and invalid UTF-8. The source-table foundation intentionally defers instruction indexes and byte ranges.
- [x] Map main/function instruction locations deterministically after branch
  patching, resolving explicit source paths through both direct and machine
  artifact paths.
- [x] Emit optional half-open byte ranges only when the explicit `!cd.ranges`
  metadata supplies exact source-local byte offsets; never infer them from
  line/column locations.
- [x] Verify a nested-function divide-by-zero error through the Rust VM's
  source location, caret, and call-stack reporting on both artifact paths.
- [x] Verify a failed `sqrt(-1)` native call through the Rust VM's source
  location, caret, and main call-stack reporting on both artifact paths.
- [x] Verify invalid-index errors through the Rust VM's location and call-stack
  reporting on both artifact paths.
- [x] Compare `dump`, `trace`, `debug`, and `profile` behavior for artifacts with and without metadata.
- [x] Compare the source-backed runtime-error pause and call stack through the
  interactive debugger on direct and machine artifacts.

**Exit criteria:** Debug metadata is additive and backward-compatible with metadata-free `cdbc 0.1`; runtime errors identify the original source when source bytes were explicitly supplied.

### Narrow M5 slices: explicit source tables, locations, and runtime diagnostics (2026-08-02)

The explicit `!cd.sources` foundation, the follow-on `DILocation` mapping
slice, and the explicit `!cd.ranges` byte-range slice are complete. Source
records, sparse `debug_locations`, and sparse `debug_ranges` are validated and
emitted through both direct and machine artifact paths, and the Rust VM
accepts the resulting artifacts. A nested divide-by-zero fixture now verifies
source-backed runtime diagnostics and call-stack parity for divide-by-zero,
failed native calls, and invalid indexes. Scripted debug parity is complete;
the source-backed runtime-error pause is also covered by an opt-in
`debug-error` parity case. More complete interactive debugger behavior and
other uncovered observability capabilities remain deferred as later M5 work.

The direct/machine observability parity slice is now covered by the opt-in
manifest `observability` case. It compares dump debug sections, run, trace,
profile, and scripted debug output for an artifact with explicit range
metadata and for a metadata-free artifact. The ranges contract uses
`ranges.cd`, debug range `s0:6:11`, and source-range output across trace,
profile, and debug; the metadata-free contract verifies no debug tail,
  `<unknown>` trace/debug locations, and no `range=` or `source_range` field on
  any surface. M6 module product/link integration is complete; M7 remains
  planned.

The follow-on interactive debugger slice adds a `step-next` observability
contract. It uses the same explicit-range artifact and verifies the entry,
step, and next pause reasons plus their resume commands on both direct and
machine artifacts. Command aliases and more complete interactive session
behavior remain deferred.

The line-breakpoint slice extends the contract with `line-delete`: it verifies
source-line breakpoint creation and one matching pause, deletion by ID, and
successful execution after resuming. Command aliases and other interactive
session behavior remain deferred.

The source-backed error-pause slice adds a `debug-error` manifest case for the
nested divide-by-zero fixture. It drives both artifacts through `continue` to
the Rust VM's error pause, compares the unique source location and call-stack
line, and quits the session. Synthetic entry locations may differ between the
direct and machine paths; the runtime-error pause itself must match exactly.

The alias slice adds an `aliases` observability case using `s`, `n`, and `q` on
the range-backed fixture. It verifies that both artifact paths accept the short
commands and report the same canonical step, next, and quit events.

The help slice adds a `help` observability case. It checks that both artifact
paths expose the interactive command reference and can quit the session cleanly.

The 2026-08-03 contract-only debugger slice adds
`docs/cd-bytecode-debugger-contract.md`, the `cdbc-debug-contract.ll` fixture,
and the `state` parity manifest form. It freezes the ordered pause fields
`reason`, `function`, `instruction`, `module`, `location`, `stack`, `locals`,
and optional `range`, together with breakpoint/resume/quit markers and invalid
command, EOF, and error-pause behavior. Direct and machine outputs must match
exactly except for the machine path's synthetic entry location and the
corresponding optional `main 0` debug-location record. The fixture covers entry,
source-breakpoint, and error pauses; parity is `50/50`.

## 10. M6 — Support module products and linking

**Purpose:** Make LLVM-produced artifacts composable without conflating a module product with an executable linked program.

**Files:**

- Modify: `llvm/lib/Target/CD/CDTargetMachine.{h,cpp}` to select program versus module artifact mode.
- Modify: `llvm/lib/Target/CD/CDBytecodeFormat.{h,cpp}`, `CDModuleInfo.{h,cpp}`, and both bytecode emitters.
- Create: `llvm/test/CodeGen/CD/cdbc-modules.ll`.
- Create: `llvm/test/CodeGen/CD/cdbc-module-errors.ll`.
- Create: `llvm/test/CodeGen/CD/cdbc-module-fallthrough.ll` and the opt-in
  `llvm/utils/cd_module_link.py` harness with its unit test and fixtures.
- Create: `llvm/test/CodeGen/CD/cdbc-module-link-diagnostic-entry.ll` and
  `cdbc-module-link-diagnostic-dependency.ll`.
- Modify: `docs/cd-bytecode-llvm-abi.md` and `llvm/lib/Target/CD/README.md`.
- Synchronize, in the independent checkout: module parsing/linking tests under `cd-compiler/tests/` and the existing Rust linker implementation.

The recommended input boundary is named metadata for module identity, entry order, dependency identity, dependency kind, and source-order insertion points. Program mode remains the default and emits the current linked-program envelope. Module mode emits `artifact: module` only when all required metadata is present; missing or inconsistent metadata is a target error.

- [x] Define stable LLVM module metadata and how `llvm-link` affects it. The
  `!cd.module` and `!cd.dependencies` records are explicit, and multiple
  module records produced by `llvm-link` are rejected rather than merged.
- [x] Preserve deterministic function, constant, name, debug-source, and
  dependency ordering through the shared module serializer.
- [x] Add tests for duplicate identities, missing dependencies, cycles, invalid insertion offsets, and non-contiguous entry order through the opt-in Rust linker harness.
- [x] Produce direct and machine module products, run the Rust `link` command, then execute the linked output with `run`.
- [x] Confirm each LLVM module product is rejected by direct VM `run` until linking.
- [x] Emit non-entry module `main` bodies as fall-through fragments so
  dependency expansion reaches the importing module's remaining instructions.
- [x] Preserve source-backed linked runtime diagnostics and dependency module
  identity through direct and machine products.

**Exit criteria:** Program and module artifacts are unambiguous, the Rust linker accepts valid LLVM-produced module products, invalid dependency metadata fails before execution, and linked runtime diagnostics retain source/module identity.

## 11. M7 — Tooling, CI, and release boundary

**Files:**

- Modify: `llvm/lib/Target/CD/README.md`.
- Create: `.github/workflows/cd-bytecode.yml` with separate LLVM-only and VM
  integration jobs.
- Create: `llvm/test/CodeGen/CD/cdbc-vm-integration.py` when the external VM path is configured.
- Create: `llvm/test/CodeGen/CD/lit.local.cfg` for the opt-in `cd-vm` feature.
- Modify: LLVM CMake/test registration only as needed for the focused CD target suite.
- Create: `docs/cd-bytecode-verification.md`.

- [x] Keep LLVM-only tests runnable with `llvm-lit` and no sibling checkout.
- [x] Make VM integration opt-in through an explicit `CD_COMPILER_ROOT` path; never discover or mutate the untracked nested checkout implicitly.
- [x] Add a focused CI job that builds `CD`, `llc`, `FileCheck`, and the CD lit suite, then a separate job that installs Rust and runs VM integration.
- [x] Record exact LLVM and Rust toolchain versions, artifact format version, and verification commands.
- [x] Test `llc -mtriple=cd-unknown-unknown`, `-O0`, `-O2`, metadata-free output, object-output rejection, invalid IR, and invalid CD ABI operations. The `-g` driver boundary is explicitly tested as an upstream rejection in `cdbc-driver-options.ll`.
- [ ] Add target-side `-g` semantics after the upstream `llc` driver accepts the flag; CD debug coverage currently uses explicit `!dbg` and `!cd.sources` metadata.
- [x] Run `git diff --check`, focused lit tests, VM dump/run/link tests, and the repository's applicable LLVM checks before claiming a milestone complete.

The supported M7 matrix entries are covered by the existing CD fixtures,
`cdbc-driver-options.ll`, and the direct/machine parity manifest. The `-g`
subcase is now an executable negative boundary because the LLVM 24 `llc`
driver rejects `-g` before target selection; CD debug output therefore
continues to use explicit `!dbg` and `!cd.sources` metadata. Target-side `-g`
semantics remain open and are not emulated locally.

The focused workflow is now defined in `.github/workflows/cd-bytecode.yml`.
Its LLVM-only job runs without a VM checkout, while its separate integration
job pins the sibling VM checkout and sets `CD_COMPILER_ROOT` explicitly. The
workflow definition and its two-job structure were validated locally. The
LLVM-only job also runs the parity/module-link Python unit tests and the
whitespace gate; the VM job builds the pinned binary and runs the complete
direct/machine parity manifest, including the opt-in observability cases.
Hosted CI execution, the broader release matrix, and target-side `-g`
semantics remain separate work.

The following remain explicit non-goals unless a separate design request changes scope: compiling `.cd` source through Clang, adding a Clang CD language frontend, native object files, assembler/disassembler syntax, JIT support, binary `.cdbc` encoding, garbage-collector layout, and a new artifact version.

### Narrow M8-first slice: function-boundary dynamic values (2026-08-03)

**Goal:** Carry proven CD dynamic values across defined function parameters and
returns while preserving the existing `cdbc 0.1` `Call` and `Return` operations.

**ABI gate:** A function uses `"cd.value.params"="0,2"` to mark a sorted,
comma-separated set of address-space-zero `ptr` parameters and
`"cd.value.return"` to mark an address-space-zero pointer return. Every pointer
parameter must be marked and every pointer return must carry the marker. The
shared `CDValueABI` validator recognizes only CD nil, explicit intrinsic
results, marked parameters, and direct calls to defined marked-return functions
as provenance. Ordinary addresses, pointer operations, indirect calls, and
unmarked interfaces remain target errors.

- [x] Add shared function attribute parsing, parameter/return ABI validation,
  call-site validation, and marked-argument/call-result provenance.
- [x] Allow proven CD values through direct and machine `Call`/`Return` lowering
  without changing `CDBytecodeFormat`, the artifact version, or the nested VM.
- [x] Cover identity, mixed scalar/CD parameters, mutation visibility, and nil
  transport with direct/machine FileCheck and Rust VM behavior parity.
- [x] Cover malformed attributes, omitted markers, ordinary alloca/global
  arguments, and ordinary pointer returns with matched target diagnostics.
- [x] Update the LLVM ABI, machine-backend, target README, fixtures, and parity
  manifest.

Verification on 2026-08-03: the focused function-value lit fixtures passed;
direct and machine artifacts passed Rust `dump` and produced identical `7` /
`nil` output; the expanded parity manifest passed `51/51`; and the nested VM
checkout remained unchanged. At that boundary, PHI/select propagation,
dynamic local storage, and function-value callback transport remained separate
decisions.

### Narrow M4 follow-up: bounded native `contains` (2026-08-10)

**Goal:** Admit the Rust VM's existing `contains` helper through the shared
`llvm.cd.native` ABI without adding a wire operation, artifact field, or
changing the nested VM.

The accepted shape is a proven CD dynamic-value collection, a scalar or proven
CD dynamic-value needle, and an exact `i1` result. The VM owns array element,
map key, and integer range membership semantics; ordinary LLVM pointers remain
rejected by shared provenance validation.

- [x] Add the name-specific capability matrix to `CDValueABI` and the typed
  artifact native-name allowlist.
- [x] Add positive array/map membership, runtime non-collection, and malformed
  ordinary-pointer fixtures for both direct and machine paths.
- [x] Add behavior/runtime-error entries to the direct/machine parity manifest.
- [x] Update the ABI, machine-backend, target README, verification matrix, and
  active development plan.

Verification on 2026-08-10: focused lit passed `3/3`; the full local CD suite
passed `104` tests with `103` passed and `1` expected unsupported VM case;
direct/machine parity passed `73/73`; parity unit tests passed `14/14`,
module-link unit tests passed `5/5`, module-link direct/machine integration
passed, Rust VM tests passed `73 + 3 + 8`, and the nested checkout remained
clean.

### Narrow M4 follow-up: bounded native `slice` (2026-08-10)

**Goal:** Admit the Rust VM's existing `slice` helper through the shared
`llvm.cd.native` ABI without adding a wire operation, artifact field, or
changing the nested VM.

The accepted shape is a proven CD dynamic-value token, two `double` start and
length operands, and an exact address-space-zero `ptr` result. The VM owns the
runtime array check, integer-valuedness, start/length bounds, snapshot, and
fresh shallow-array allocation; ordinary LLVM pointers remain rejected by
shared provenance validation.

- [x] Add the name-specific capability matrix to `CDValueABI` and the typed
  artifact native-name allowlist.
- [x] Add positive empty/middle/tail slices, runtime non-array, and malformed
  direct/machine fixtures.
- [x] Add behavior/runtime-error entries to the direct/machine parity manifest.
- [x] Update the ABI, machine-backend, target README, verification matrix, and
  active development plan.

Verification on 2026-08-10: focused slice/error lit passed `5/5`; the full local
CD suite passed `106` tests with `105` passed and `1` expected unsupported VM
case; direct/machine parity passed `75/75`; parity unit tests passed `14/14`,
module-link unit tests passed `5/5`, module-link direct/machine integration
passed, Rust VM tests passed `73 + 3 + 8`, and the nested checkout remained
clean.

### Narrow M4 follow-up: bounded native `copy` (2026-08-10)

**Goal:** Admit the Rust VM's existing `copy` helper through the shared
`llvm.cd.native` ABI without adding a wire operation, artifact field, or
changing the nested VM.

The accepted shape is a proven CD dynamic-value token and an exact
address-space-zero `ptr` result. The VM owns the runtime array check, snapshot,
and fresh shallow-array allocation; ordinary LLVM pointers remain rejected by
shared provenance validation.

- [x] Add the name-specific capability matrix to `CDValueABI` and the typed
  artifact native-name allowlist.
- [x] Add positive empty/non-empty copies, runtime non-array, and malformed
  direct/machine fixtures.
- [x] Add behavior/runtime-error entries to the direct/machine parity manifest.
- [x] Update the ABI, machine-backend, target README, verification matrix, and
  active development plan.

Verification on 2026-08-10: focused copy/error lit passed `3/3`; the full local
CD suite passed `108` tests with `107` passed and `1` expected unsupported VM
case; direct/machine parity passed `77/77`; parity unit tests passed `14/14`,
module-link unit tests passed `5/5`, module-link direct/machine integration
passed, Rust VM tests passed `73 + 3 + 8`, and the nested checkout remained
clean.

## 12. Recommended next execution order

The detailed active sequence is maintained in
[the 2026-08-03 development plan](2026-08-03-cd-bytecode-development-plan.md).
The order is deliberately dependency-driven:

1. Close the M7 hosted LLVM-only and Rust-VM workflow gate, while retaining the
   upstream `llc -g` rejection as an explicit boundary.
2. Keep richer debugger query commands behind the now-frozen public state
   contract; adding `list`, `where`, locals, or breakpoint queries requires a
   separate design and fixture.
3. Keep the completed marked function parameter/return boundary as the base;
   dynamic CD `select`, PHI, and one-slot dynamic storage now have parity
   slices; define any broader storage or pointer ABI separately.
4. Add callback native helpers one vertical slice at a time only after dynamic
   value and callback contracts have direct/machine/Rust parity.

Do not combine steps 2-5 in one implementation commit. In particular, callback
native work must not smuggle in an implicit function-value or pointer ABI.

Do not infer source text from ordinary LLVM debug metadata: `DIFile` and
`DILocation` identify locations, but only explicit `!cd.sources` records provide
the source bytes and stable source indexes.

## 13. Completion gates

A milestone is complete only when all of the following are true:

- implementation, contract documentation, and tests describe the same behavior;
- LLVM-only tests pass without `cd-compiler/` present;
- every positive artifact passes Rust parser/verifier checks when integration is enabled;
- runtime output, runtime errors, debug locations, and module-link results are checked at their applicable milestone;
- unsupported behavior fails with a stable diagnostic instead of emitting a partially valid artifact;
- `git diff --check` is clean and delivery state is reported separately from verification state.
