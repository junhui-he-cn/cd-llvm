# CD Native Call Slice Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Lower a bounded set of non-callback `llvm.cd.native` calls to the
existing `cdbc 0.1` `native_call` operation through both LLVM CD emitters.

**Architecture:** The intrinsic takes a private constant UTF-8 name global and
variadic value operands. `CDValueABI` validates the name-specific signature
before either emitter constructs a typed `CDInstruction::nativeCall`; the
machine path uses a `CD_NATIVE_CALL` pseudo and the same artifact bridge. The
first allowlist is `floor`, `ceil`, `sqrt`, `str`, `typeOf`, `hash`, and
`range`; callback natives and collection/string slicing helpers remain
compile-time unsupported until a separate capability decision.

**Tech Stack:** LLVM TableGen intrinsics and pseudos, C++ CD artifact model,
LLVM lit/FileCheck, the existing Rust `cdbc 0.1` VM and parity harness.

---

### Task 1: Record the bounded native ABI and red fixtures

**Files:**
- Modify: `docs/cd-bytecode-llvm-abi.md`
- Modify: `docs/cd-bytecode-machine-backend.md`
- Modify: `llvm/lib/Target/CD/README.md`
- Modify: `docs/superpowers/plans/2026-07-31-cd-bytecode-roadmap.md`
- Create: `llvm/test/CodeGen/CD/cdbc-native.ll`
- Create: `llvm/test/CodeGen/CD/cdbc-native-values.ll`
- Create: `llvm/test/CodeGen/CD/cdbc-native-errors.ll`
- Create: `llvm/test/CodeGen/CD/cdbc-native-runtime.ll`

- [x] Add the exact intrinsic shape and capability matrix:

```text
llvm.cd.native(ptr name, ...) -> scalar or address-space-zero CD value

floor, ceil, sqrt: one double -> double
str: one scalar or CD value -> ptr string
typeOf: one scalar or CD value -> ptr string
hash: one scalar or CD value -> double
range: one to three doubles -> ptr range
```

  Names must be private, constant, non-empty UTF-8 byte globals. `map`,
  `filter`, `flatMap`, `any`, `all`, `count`, `find`, `findIndex`, `reduce`,
  `substr`, `charAt`, and collection mutation names stay outside this slice.
- [x] Add positive coverage using all seven names across the double-return and
  pointer-return fixtures, including a range consumed by `len` and `index`,
  and add FileCheck expectations for direct and machine output containing
  `native_call`.
- [x] Add malformed cases for unknown/callback names, wrong arity/types,
  ordinary pointer arguments, invalid name globals, and invalid return types.
- [x] Add a runtime case for `sqrt(-1.0)` and record the exact VM diagnostic in
  the parity manifest after the opcode exists.
- [x] Run the new positive fixtures against the old `llc`; both fail with exit
  134 and `CD target only supports globals used by CD string/name intrinsics`,
  because native name globals are not yet recognized by the target.

### Task 2: Add the intrinsic and shared signature validation

**Files:**
- Modify: `llvm/include/llvm/IR/IntrinsicsCD.td`
- Modify: `llvm/lib/Target/CD/CDValueABI.h`
- Modify: `llvm/lib/Target/CD/CDValueABI.cpp`

- [x] Register this intrinsic:

```tablegen
def int_cd_native : DefaultAttrsIntrinsic<
    [llvm_any_ty], [llvm_ptr_ty, llvm_vararg_ty]>;
```

- [x] Add `isNativeIntrinsic`, `isCDValue` propagation for address-space-zero
  pointer results, and `validateNativeCall`.
- [x] Make validation read the name global and enforce the exact matrix from
  Task 1: `floor`/`ceil`/`sqrt` require one `double` and return `double`;
  `str`/`typeOf`/`hash` require one scalar or CD value and return `ptr`/`ptr`/
  `double`; `range` requires one to three `double` operands and returns `ptr`.
- [x] Reject all other names with a stable diagnostic that distinguishes an
  unsupported native name from a malformed intrinsic shape.
- [ ] Re-run the positive and malformed lit fixture and verify the positive
  case now reaches the backend lowering boundary rather than the old ordinary
  call rejection.

### Task 3: Add typed artifact and direct-emitter lowering

**Files:**
- Modify: `llvm/lib/Target/CD/CDBytecodeFormat.h`
- Modify: `llvm/lib/Target/CD/CDBytecodeFormat.cpp`
- Modify: `llvm/lib/Target/CD/CDBytecodeEmitter.cpp`

- [x] Add `CDOpcode::NativeCall` and this constructor:

```cpp
static CDInstruction nativeCall(unsigned destination, unsigned name,
                                std::vector<unsigned> arguments);
```

  Store the name-table index in `reference`, the argument registers in
  `operands`, and the result in `result`.
- [x] Validate the name-table reference, result register, and variadic argument
  registers; reject native names outside the bounded allowlist before
  serialization.
- [x] Serialize exactly `rD = native_call nName [rArg0, ...]` and return
  `native_call` from `opcodeName`.
- [x] In `CDFunctionEmitter::emitCall`, validate `llvm.cd.native`, add the
  name with `nameRegister`, collect all value operands, and append
  `CDInstruction::nativeCall` before the ordinary-call fallback.
- [x] Run direct lit emission plus Rust VM `dump` and `run` for the positive and
  runtime fixtures.

### Task 4: Add the TableGen machine path and parity fixtures

**Files:**
- Modify: `llvm/lib/Target/CD/CDInstrInfo.td`
- Modify: `llvm/lib/Target/CD/CDMachineBytecodeEmitter.cpp`
- Modify: `llvm/test/CodeGen/CD/cdbc-machine-parity.list`

- [x] Add `CD_NATIVE_CALL` with destination `CDValue`, immediate name index,
  and variadic `CDValue` arguments.
- [x] Lower the intrinsic after collecting/materializing every argument and
  before inserting the pseudo, preserving definition-before-use ordering.
- [x] Bridge the pseudo to `CDInstruction::nativeCall` after checking its
  destination, immediate name index, and register operands.
- [x] Add artifact, behavior, and runtime-error parity entries for the native
  fixtures and run the complete manifest.

### Task 5: Synchronize docs and complete verification

**Files:**
- Modify: `docs/cd-bytecode-llvm-abi.md`
- Modify: `docs/cd-bytecode-machine-backend.md`
- Modify: `llvm/lib/Target/CD/README.md`
- Modify: `docs/superpowers/plans/2026-07-31-cd-bytecode-roadmap.md`

- [x] Mark only the bounded native-call slice complete and keep callback and
  unimplemented native names explicitly deferred.
- [x] Run:

```sh
ninja -C build-cd llc
build-cd/bin/llvm-lit -s llvm/test/CodeGen/CD
cargo build --manifest-path cd-compiler/vm-rs/Cargo.toml --quiet
cargo test --manifest-path cd-compiler/vm-rs/Cargo.toml
python3 llvm/utils/cd_bytecode_parity.py \
  --llc build-cd/bin/llc \
  --vm cd-compiler/vm-rs/target/debug/compiler-design-vm \
  --manifest llvm/test/CodeGen/CD/cdbc-machine-parity.list
git diff --check
```

- [x] Confirm `git -C cd-compiler status --short --branch` is unchanged.
- [x] Commit only the outer native-call slice with
  `feat(cd): lower bounded native calls`; do not push.

Verification: focused native lit `4/4`, complete CD lit `55/55`, direct/machine
parity `33/33`, and Rust VM cargo tests `73 + 3 + 8` all passed. The final
commit is performed after the remaining diff and hygiene checks in this task.
