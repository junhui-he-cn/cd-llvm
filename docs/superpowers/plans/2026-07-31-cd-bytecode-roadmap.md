# CD Bytecode in LLVM Roadmap

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this roadmap task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Continue the experimental LLVM `CD` target until LLVM IR can produce verified, executable Compiler Design bytecode while preserving the `cd-compiler` Rust VM's `cdbc 0.1` contract.

**Architecture:** Keep `CD` as a software target selected by `cd-unknown-unknown`. LLVM lowers a deliberately defined subset of LLVM IR into an internal CD artifact model and serializes that model as `.cdbc`; it does not pretend that arbitrary LLVM aggregates or native pointers are already CD values. The Rust VM remains the execution oracle, and every new opcode or metadata field is accepted only after synchronized contract, parser, executor, and end-to-end tests.

**Tech Stack:** LLVM 24 target infrastructure, CMake experimental targets, LLVM IR/ModulePass APIs, LLVM lit/FileCheck, `cdbc 0.1` text artifacts, and the sibling `cd-compiler/vm-rs` Rust VM.

---

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

## 2. Current baseline: M0, already present

The outer repository already contains an experimental target in:

- `llvm/lib/Target/CD/TargetInfo/`: `cd` triple registration.
- `llvm/lib/Target/CD/MCTargetDesc/`: minimal MC descriptions required by `llc`.
- `llvm/lib/Target/CD/CDTargetMachine.{h,cpp}`: assembly-file-only target machine and module pass hookup.
- `llvm/lib/Target/CD/CDBytecodeEmitter.{h,cpp}`: direct LLVM IR to text lowering.
- `llvm/lib/Target/CD/README.md`: current target boundary.
- `llvm/test/CodeGen/CD/cdbc-basic.ll`: arithmetic, comparison, call, print, branch, PHI, and object-output rejection coverage.
- `llvm/test/CodeGen/CD/cdbc-parameters.ll`: function parameter metadata and unnamed-parameter naming coverage.

The implemented subset currently covers scalar integer/floating values, finite constants, arithmetic, comparisons, scalar casts as `move`, direct single-slot `alloca` load/store, direct calls to defined functions, `cd_print`/`print`, conditional and unconditional branches, PHI edge stores, and returns. The target README correctly leaves arrays, maps, structs, variants, general globals, native calls, and source-backed debug sections outside the current boundary.

This baseline is source-present but must be freshly verified in the current checkout before the next implementation slice is selected.

## 3. Milestone map

| Milestone | Outcome | Depends on | Status |
| --- | --- | --- | --- |
| M0 | Reproducible build, lit tests, Rust `dump`, and explicit object rejection for the existing target | — | Complete; verified in the current checkout |
| M1 | Typed CD artifact model, canonical serializer, and pre-VM reference validation | M0 | Complete; direct emitter now uses the typed boundary |
| M2 | Well-defined scalar/control-flow semantics and `-O0`/`-O2` compatibility | M1 | Complete; scalar and control-flow subset verified |
| M3 | TableGen-backed machine instruction path with parity against the direct emitter | M1, M2 | Complete; supported scalar/control-flow parity verified |
| M4 | Explicit CD value ABI for arrays, maps, strings, structs, variants, indexing, and native calls | M1, M2, M3 | In progress; string ABI design gate recorded |
| M5 | Source locations, source ranges, call-stack diagnostics, and trace parity | M1, M2, M3 | Planned |
| M6 | Program versus module artifacts, dependency metadata, and VM linker integration | M1, M3, M4, M5 | Planned |
| M7 | Reproducible CI/integration harness, documentation, and release-quality boundary | M0-M6 | Planned |

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
- Create: `llvm/test/CodeGen/CD/cdbc-array.ll` and
  `llvm/test/CodeGen/CD/cdbc-array-errors.ll`.
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
`run` observe the same values on both paths.  Array results remain limited to
printing and nested construction; indexing, mutation, `len`, return/parameter,
PHI/`select`, and ordinary pointer operations remain deferred.

**Exit criteria:** Every newly emitted opcode is documented, parsed, verified, and executed by the Rust VM; collection mutation and failure behavior are covered; ordinary LLVM aggregates remain rejected unless they use the defined CD ABI.

## 9. M5 — Add source-backed debug metadata

**Purpose:** Preserve source locations and runtime diagnostics across LLVM lowering without fabricating source text that is not present in LLVM IR.

**Files:**

- Create: `llvm/lib/Target/CD/CDDebugInfo.{h,cpp}`.
- Modify: `llvm/lib/Target/CD/CDBytecodeFormat.{h,cpp}` and `CDBytecodeEmitter.cpp`.
- Create: `llvm/test/CodeGen/CD/cdbc-debug.ll`.
- Modify: `llvm/lib/Target/CD/README.md` and `docs/cd-bytecode-llvm-abi.md`.
- Synchronize, in the independent checkout: `cd-compiler/vm-rs/src/format.rs`, `cd-compiler/vm-rs/src/vm.rs`, and debug/trace tests.

Use LLVM `DILocation`/`DIFile` for line and column identity. Because ordinary LLVM debug metadata does not contain the original source bytes, emit `debug_sources` only when a defined `!cd.sources` named-metadata record supplies the display path, canonical module identity, and exact source text. Without that record, the target may emit no debug sections while retaining correct program execution.

- [ ] Define the `!cd.sources` record and reject malformed source indexes, duplicate entries, and invalid UTF-8/byte ranges.
- [ ] Map main/function instruction locations deterministically after branch patching.
- [ ] Emit optional half-open byte ranges only when the source metadata supplies exact byte offsets.
- [ ] Verify divide-by-zero, invalid index, failed native call, and nested-function errors through the Rust VM's location and call-stack reporting.
- [ ] Compare `dump`, `trace`, `debug`, and `profile` behavior for artifacts with and without metadata.

**Exit criteria:** Debug metadata is additive and backward-compatible with metadata-free `cdbc 0.1`; runtime errors identify the original source when source bytes were explicitly supplied.

## 10. M6 — Support module products and linking

**Purpose:** Make LLVM-produced artifacts composable without conflating a module product with an executable linked program.

**Files:**

- Modify: `llvm/lib/Target/CD/CDTargetMachine.{h,cpp}` to select program versus module artifact mode.
- Modify: `llvm/lib/Target/CD/CDBytecodeFormat.{h,cpp}` and `CDBytecodeEmitter.cpp`.
- Create: `llvm/test/CodeGen/CD/cdbc-modules.ll`.
- Create: `llvm/test/CodeGen/CD/cdbc-module-errors.ll`.
- Modify: `docs/cd-bytecode-llvm-abi.md` and `llvm/lib/Target/CD/README.md`.
- Synchronize, in the independent checkout: module parsing/linking tests under `cd-compiler/tests/` and the existing Rust linker implementation.

The recommended input boundary is named metadata for module identity, entry order, dependency identity, dependency kind, and source-order insertion points. Program mode remains the default and emits the current linked-program envelope. Module mode emits `artifact: module` only when all required metadata is present; missing or inconsistent metadata is a target error.

- [ ] Define stable LLVM module metadata and how `llvm-link` affects it.
- [ ] Preserve deterministic function, constant, name, debug-source, and dependency ordering through module emission.
- [ ] Add tests for duplicate identities, missing dependencies, cycles, invalid insertion offsets, and non-contiguous entry order.
- [ ] Produce module products, run the Rust `link` command, then execute the linked output with `run`.
- [ ] Confirm a module product is rejected by direct VM `run` until linking.

**Exit criteria:** Program and module artifacts are unambiguous, the Rust linker accepts valid LLVM-produced module products, invalid dependency metadata fails before execution, and linked runtime diagnostics retain source/module identity.

## 11. M7 — Tooling, CI, and release boundary

**Files:**

- Modify: `llvm/lib/Target/CD/README.md`.
- Create: `llvm/test/CodeGen/CD/cdbc-vm-integration.py` when the external VM path is configured.
- Modify: LLVM CMake/test registration only as needed for the focused CD target suite.
- Create: `docs/cd-bytecode-verification.md`.

- [ ] Keep LLVM-only tests runnable with `llvm-lit` and no sibling checkout.
- [ ] Make VM integration opt-in through an explicit `CD_COMPILER_ROOT` path; never discover or mutate the untracked nested checkout implicitly.
- [ ] Add a focused CI job that builds `CD`, `llc`, `FileCheck`, and the CD lit suite, then a separate job that installs Rust and runs VM integration.
- [ ] Record exact LLVM and Rust toolchain versions, artifact format version, and verification commands.
- [ ] Test `llc -mtriple=cd-unknown-unknown`, `-O0`, `-O2`, `-g`, metadata-free output, object-output rejection, invalid IR, and invalid CD ABI operations.
- [ ] Run `git diff --check`, focused lit tests, VM dump/run/link tests, and the repository's normal LLVM checks before claiming a milestone complete.

The following remain explicit non-goals unless a separate design request changes scope: compiling `.cd` source through Clang, adding a Clang CD language frontend, native object files, assembler/disassembler syntax, JIT support, binary `.cdbc` encoding, garbage-collector layout, and a new artifact version.

## 12. Recommended next execution order

The next development session should execute only this narrow sequence:

1. Record the collection ABI gate: constructor operand capabilities,
   ownership/aliasing, mutation failure behavior, and the `cdbc 0.1` array/map
   operation bridge.
2. Keep ordinary LLVM aggregates and pointers rejected while the collection
   gate is being reviewed; do not infer an array or map from `alloca`, globals,
   or aggregate instructions.
3. Implement only the first explicit collection intrinsic after the gate has
   positive, malformed, Rust `dump`/`run`, and direct/machine parity tests.

Do not begin M4 collection lowering until the CD value ABI document has been reviewed, because choosing an implicit pointer/aggregate representation would make later Rust VM and module-linking work incompatible.

## 13. Completion gates

A milestone is complete only when all of the following are true:

- implementation, contract documentation, and tests describe the same behavior;
- LLVM-only tests pass without `cd-compiler/` present;
- every positive artifact passes Rust parser/verifier checks when integration is enabled;
- runtime output, runtime errors, debug locations, and module-link results are checked at their applicable milestone;
- unsupported behavior fails with a stable diagnostic instead of emitting a partially valid artifact;
- `git diff --check` is clean and delivery state is reported separately from verification state.
