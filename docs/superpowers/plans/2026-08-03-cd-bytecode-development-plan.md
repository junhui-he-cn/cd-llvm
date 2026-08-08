# CD Bytecode LLVM Development Plan 2026-08-03 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Re-sequence the remaining LLVM CD bytecode work into independently
verifiable slices, close the M7 release gate, add the next bounded native string
helpers, then continue with selected callback-native predicate helpers while
stopping at the explicit ABI decisions required for debugger queries and
dynamic-value transport.

**Architecture:** Keep the direct emitter as the compatibility path and the
TableGen machine emitter as an opt-in path sharing `CDValueABI`,
`CDBytecodeFormat`, and the Rust VM as the execution oracle. Preserve the
existing `cdbc 0.1` text contract; add no artifact opcode or field unless the
Rust parser, verifier, runtime, direct path, machine path, and parity harness
are updated together. Treat debugger queries and cross-function dynamic values
as design gates rather than allowing local conveniences to become implicit ABI.

**Tech Stack:** LLVM 24 CD target, LLVM IR and TableGen, C++ typed artifact
model, LLVM lit/FileCheck, Python parity/link harnesses, `cdbc 0.1`, and the
independent `cd-compiler/vm-rs` Rust VM.

---

## Current state and operating rules

The current outer checkout is `main`, ahead of `origin/main` by the local
roadmap and contract commits. The independent, untracked `cd-compiler/`
checkout is not part
of this repository and must remain clean unless a future ABI decision
explicitly requires a separate VM change. The roadmap and this active plan may
be intentionally edited in the outer checkout while the queue is being
reorganized.

The completed boundary is:

~~~
LLVM IR --llc -mtriple=cd-unknown-unknown--> cdbc 0.1 --> cd-compiler Rust VM
~~~

M0-M4 are complete for the bounded value ABI and native allowlist. M5 has
source tables, locations, ranges, runtime diagnostics, trace/profile/debug
parity, `step`/`next`, aliases, help, line-breakpoint deletion, error-pause
parity, and a frozen pause-state contract. M6 module products and linking are
complete. M7 is locally defined and
verified, but hosted workflow execution and the wider release matrix still need
an explicit gate. LLVM 24's upstream `llc -g` rejection remains a documented
driver boundary; this plan does not emulate target-side `-g` support.

These rules apply to every task:

1. `cd-compiler/docs/bytecode-text-format.md` remains the wire-contract authority.
2. Every positive artifact must pass Rust `dump`; runtime fixtures must use
   Rust `run` and compare output or the exact diagnostic.
3. Direct and machine paths must share capability validation and have parity
   coverage for every new operation.
4. Ordinary LLVM pointers, aggregates, globals, allocas, and external calls
   are never inferred to be CD values.
5. Nested VM changes, local commits, remote pushes, merges, and branch cleanup
   are separate actions; no task silently performs the next delivery action.
6. Object files, assembly encodings, JIT execution, binary `.cdbc`, a new
   artifact version, a Clang CD frontend, and garbage-collector layout remain
   outside this plan.

## File and ownership map

| Area | Files | Responsibility |
| --- | --- | --- |
| Target ABI | `llvm/lib/Target/CD/CDValueABI.{h,cpp}` | Recognize explicit `llvm.cd.*` operations, enforce source-shape capability matrices, and validate marked function-value transport. |
| Artifact boundary | `llvm/lib/Target/CD/CDBytecodeFormat.{h,cpp}` | Typed instructions, structural validation, and canonical `cdbc 0.1` serialization. |
| Lowering | `llvm/lib/Target/CD/CDBytecodeEmitter.cpp`, `llvm/lib/Target/CD/CDMachineBytecodeEmitter.cpp` | Direct and opt-in machine lowering into the shared artifact model. |
| Machine description | `llvm/include/llvm/IR/IntrinsicsCD.td`, `llvm/lib/Target/CD/CDInstrInfo.td` | Intrinsic signatures and CD virtual-value pseudos when a slice needs them. |
| Fixtures and parity | `llvm/test/CodeGen/CD/`, `llvm/utils/cd_bytecode_parity.py`, `llvm/test/CodeGen/CD/cdbc-machine-parity.list` | FileCheck, VM dump/run, direct/machine artifact/behavior/error parity. |
| Contract docs | `docs/cd-bytecode-llvm-abi.md`, `docs/cd-bytecode-machine-backend.md`, `llvm/lib/Target/CD/README.md` | Public LLVM-to-CD rules and machine-path boundary. |
| Verification and roadmap | `docs/cd-bytecode-verification.md`, `docs/superpowers/plans/2026-07-31-cd-bytecode-roadmap.md` | Reproducible gates, status, and deferred decisions. |
| VM oracle | `cd-compiler/vm-rs/src/vm.rs`, `cd-compiler/vm-rs/src/main.rs`, `cd-compiler/vm-rs/tests/library_api.rs` | Read-only semantic reference for current slices; separate checkout for any future change. |

## Task 0: Close the M7 release and hosted-CI gate

**Files:**
- Verify: `.github/workflows/cd-bytecode.yml`, `docs/cd-bytecode-verification.md`, `llvm/test/CodeGen/CD/cdbc-driver-options.ll`.
- Modify only when the observed result changes: `docs/cd-bytecode-verification.md`, `llvm/lib/Target/CD/README.md`, `docs/superpowers/plans/2026-07-31-cd-bytecode-roadmap.md`.

- [ ] **Step 1: Reconfirm branch, nested-checkout, and remote state**

~~~
git fetch origin main
git status --short --branch
git rev-parse HEAD origin/main
git -C cd-compiler status --short --branch
git -C cd-compiler rev-parse HEAD origin/master
~~~

Expected: outer `HEAD` equals `origin/main`; any outer worktree entries are
limited to the intentionally edited roadmap/active-plan files plus the
independent untracked `cd-compiler/` directory; and the nested checkout is
clean and equals `origin/master`.

- [ ] **Step 2: Rebuild the LLVM-only tools**

~~~
ninja -C build-cd llc FileCheck count not
~~~

Expected: exit status `0`; the build must not require `llvm-lit` as a Ninja
target because the workflow invokes the lit executable produced by the LLVM
build.

- [ ] **Step 3: Run the LLVM-only release matrix**

~~~
env -u CD_COMPILER_ROOT build-cd/bin/llvm-lit -sv llvm/test/CodeGen/CD
PYTHONDONTWRITEBYTECODE=1 python3 llvm/utils/cd_bytecode_parity_test.py
PYTHONDONTWRITEBYTECODE=1 python3 llvm/utils/cd_module_link_test.py
git diff --check
~~~

Expected at the current fixture set: `83 passed / 1 unsupported` for the CD
lit directory, `14/14` parity-harness unit tests, `5/5` module-link unit tests,
and a clean whitespace check. The one unsupported case is the opt-in VM test
with `CD_COMPILER_ROOT` unset.

- [ ] **Step 4: Run the explicit Rust VM integration matrix**

~~~
cargo test --manifest-path cd-compiler/vm-rs/Cargo.toml
cargo build --manifest-path cd-compiler/vm-rs/Cargo.toml --quiet
PYTHONDONTWRITEBYTECODE=1 python3 llvm/utils/cd_bytecode_parity.py \
  --llc build-cd/bin/llc \
  --vm cd-compiler/vm-rs/target/debug/compiler-design-vm \
  --manifest llvm/test/CodeGen/CD/cdbc-machine-parity.list
PYTHONDONTWRITEBYTECODE=1 python3 llvm/utils/cd_module_link.py \
  --llc build-cd/bin/llc \
  --vm cd-compiler/vm-rs
git -C cd-compiler status --short --branch
~~~

Expected: the existing Rust groups pass (`73 + 3 + 8`), the direct/machine
manifest passes all `58` entries in the current checkout, the module-link
harness passes, and the nested checkout remains clean.

- [ ] **Step 5: Check hosted workflow results without changing workflow scope**

~~~
latest_run="$(gh run list --workflow cd-bytecode.yml --limit 1 \
  --json databaseId --jq '.[0].databaseId')"
gh run view "$latest_run" --json status,conclusion,jobs
~~~

The latest run must contain successful `llvm-only` and `vm-integration` jobs.
Run `31103840045` for `749aef4ba` failed because the workflow omitted LLVM's
`not` test utility from both build commands; the local fix adds `not` and must
be published before rerunning. If a rerun fails, reproduce that job locally
and fix only the CD workflow or its directly covered fixture/documentation; do
not weaken the gate or absorb the nested VM checkout into the outer repository.

- [ ] **Step 6: Record the release boundary and commit the gate**

Update the verification matrix and roadmap only with observed results. Keep
the following driver contract explicit:

~~~
llc -mtriple=cd-unknown-unknown -g ...
  -> upstream llc: Unknown command line argument '-g'
llc -mtriple=cd-unknown-unknown -filetype=obj ...
  -> target does not support generation of this file type
~~~

Mark M7 complete only after both hosted jobs pass; keep target-side `-g`
semantics open. Run `git diff --check`, then commit locally with:

~~~
git add .github/workflows/cd-bytecode.yml \
  docs/cd-bytecode-verification.md \
  docs/superpowers/plans/2026-07-31-cd-bytecode-roadmap.md \
  llvm/lib/Target/CD/README.md
git commit -m "docs(cd): close release verification gate"
~~~

Remote push is a separate delivery action.

## Task 1: Add `substr` and `charAt` through the existing native-call ABI

**Files:**
- Modify: `llvm/lib/Target/CD/CDValueABI.cpp` and `llvm/lib/Target/CD/CDBytecodeFormat.cpp`.
- Verify unchanged generic paths: `llvm/lib/Target/CD/CDBytecodeEmitter.cpp`, `llvm/lib/Target/CD/CDMachineBytecodeEmitter.cpp`, `llvm/include/llvm/IR/IntrinsicsCD.td`, `llvm/lib/Target/CD/CDInstrInfo.td`.
- Create: `llvm/test/CodeGen/CD/cdbc-native-string.ll`.
- Create: `llvm/test/CodeGen/CD/cdbc-native-substr-runtime.ll` and `llvm/test/CodeGen/CD/cdbc-native-char-at-runtime.ll`.
- Modify: `llvm/test/CodeGen/CD/cdbc-native-errors.ll`, `llvm/test/CodeGen/CD/cdbc-machine-parity.list`.
- Modify: `docs/cd-bytecode-llvm-abi.md`, `docs/cd-bytecode-machine-backend.md`, `llvm/lib/Target/CD/README.md`, and `docs/superpowers/plans/2026-07-31-cd-bytecode-roadmap.md`.
- Read only: `cd-compiler/vm-rs/src/vm.rs`, `cd-compiler/docs/bytecode-text-format.md`.

The slice adds no opcode, intrinsic, artifact field, or VM change. It extends
the existing `NativeCall` name allowlist and the shared name-specific validator.
The source contract is:

~~~
substr: llvm.cd.native(ptr name, ptr stringValue, double start, double length) -> ptr
charAt: llvm.cd.native(ptr name, ptr stringValue, double index) -> ptr
~~~

The first operand after the name is an address-space-zero CD dynamic-value
token. The target cannot statically inspect the VM's runtime string tag, so a
non-string CD value is accepted by the explicit token boundary and produces
the Rust VM's runtime type diagnostic. The index, start, and length operands
must be `double`; integer-valuedness, Unicode scalar boundaries, and range
errors remain VM runtime semantics. Both operations return a fresh CD string
value and do not mutate or alias the source string.

- [x] **Step 1: Add the positive UTF-8 fixture**

Use the existing native-call declaration and explicit string intrinsic. The
fixture must include ASCII, an empty result, and multi-byte scalar boundaries:

~~~
@substr_name = private unnamed_addr constant [7 x i8] c"substr\00"
@char_at_name = private unnamed_addr constant [7 x i8] c"charAt\00"
@ascii = private unnamed_addr constant [6 x i8] c"hello\00"
@utf8 = private unnamed_addr constant [11 x i8] c"\E4\BD\A0\F0\9F\99\82e\CC\81\00"

declare ptr @llvm.cd.string(ptr)
declare ptr @llvm.cd.native(ptr, ...)
declare void @cd_print(ptr)

define i32 @main() {
entry:
  %ascii_source = call ptr @llvm.cd.string(ptr @ascii)
  %empty = call ptr (ptr, ...) @llvm.cd.native(
      ptr @substr_name, ptr %ascii_source, double 5.0, double 0.0)
  %source = call ptr @llvm.cd.string(ptr @utf8)
  %part = call ptr (ptr, ...) @llvm.cd.native(
      ptr @substr_name, ptr %source, double 1.0, double 2.0)
  %character = call ptr (ptr, ...) @llvm.cd.native(
      ptr @char_at_name, ptr %source, double 1.0)
  call void @cd_print(ptr %empty)
  call void @cd_print(ptr %part)
  call void @cd_print(ptr %character)
  ret i32 0
}
~~~

Add direct and machine FileCheck expectations for three `native_call` operations
and assert that `dump`/`run` produce an empty string, `🙂e`, and `🙂`, proving
Unicode scalar rather than byte indexing.

- [x] **Step 2: Extend the shared native capability matrix**

Add the two names to `isSupportedNativeName` and add these branches to
`validateNativeCall` before the unsupported-name diagnostic:

~~~
if (NativeName == "substr") {
  if (Call.arg_size() != 4 || !isCDValue(*Call.getArgOperand(1)) ||
      !Call.getArgOperand(2)->getType()->isDoubleTy() ||
      !Call.getArgOperand(3)->getType()->isDoubleTy() || !HasCDPointerResult) {
    Error = "llvm.cd.native substr requires a CD string value, two double "
            "arguments, and a ptr result";
    return false;
  }
  return true;
}
if (NativeName == "charAt") {
  if (Call.arg_size() != 3 || !isCDValue(*Call.getArgOperand(1)) ||
      !Call.getArgOperand(2)->getType()->isDoubleTy() || !HasCDPointerResult) {
    Error = "llvm.cd.native charAt requires a CD string value, one double "
            "argument, and a ptr result";
    return false;
  }
  return true;
}
~~~

Keep the existing generic direct and machine lowering path. The artifact
validator must accept `substr` and `charAt` in `native_call`, while the Rust VM
continues to own exact arity, runtime string type, integer-index, Unicode
scalar, and bounds errors.

- [x] **Step 3: Add malformed coverage for both backends**

Extend `cdbc-native-errors.ll` with direct and machine checks for wrong arity,
non-`double` indexes, scalar/non-CD pointer arguments, and non-pointer results.
The stable diagnostics must distinguish shape errors from the existing
unsupported-name error. Keep callback names such as `map` rejected.

- [x] **Step 4: Add runtime and parity coverage**

Keep `cdbc-native-runtime.ll` as the existing `sqrt(-1)` runtime fixture. Add
one `substr` runtime-error fixture and one `charAt` runtime-error fixture so
each parity entry has one deterministic failing program. The focused Rust VM
tests already exercise the complete negative, fractional, and out-of-range
matrix; the LLVM fixtures must cover the representative target-to-VM path and
document all VM-owned messages:

~~~
substr start offset out of bounds
substr length out of bounds
substr expects integer start offset
charAt index out of bounds
charAt expects integer index
~~~

Add an `artifact` entry for `cdbc-native-string.ll` and `runtime-error` entries
for `cdbc-native-substr-runtime.ll` and
`cdbc-native-char-at-runtime.ll` in `cdbc-machine-parity.list`. Require both
artifacts to pass `dump`, both runs to fail for the error cases, and the
diagnostics to match exactly.

- [x] **Step 5: Run the focused and complete gates**

~~~
ninja -C build-cd llc FileCheck count not
build-cd/bin/llvm-lit -sv \
  llvm/test/CodeGen/CD/cdbc-native-string.ll \
  llvm/test/CodeGen/CD/cdbc-native-errors.ll \
  llvm/test/CodeGen/CD/cdbc-native-substr-runtime.ll \
  llvm/test/CodeGen/CD/cdbc-native-char-at-runtime.ll
cargo build --manifest-path cd-compiler/vm-rs/Cargo.toml --quiet
PYTHONDONTWRITEBYTECODE=1 python3 llvm/utils/cd_bytecode_parity.py \
  --llc build-cd/bin/llc \
  --vm cd-compiler/vm-rs/target/debug/compiler-design-vm \
  --manifest llvm/test/CodeGen/CD/cdbc-machine-parity.list
env -u CD_COMPILER_ROOT build-cd/bin/llvm-lit -q llvm/test/CodeGen/CD
git diff --check
git -C cd-compiler status --short --branch
~~~

Expected: focused direct/machine fixtures pass, the full CD suite remains
green, the parity manifest passes all `49` entries, and the nested VM is
unchanged. Commit only the outer slice:

~~~
git add llvm/lib/Target/CD/CDValueABI.cpp \
  llvm/lib/Target/CD/CDBytecodeFormat.cpp \
  llvm/test/CodeGen/CD/cdbc-native-string.ll \
  llvm/test/CodeGen/CD/cdbc-native-errors.ll \
  llvm/test/CodeGen/CD/cdbc-native-substr-runtime.ll \
  llvm/test/CodeGen/CD/cdbc-native-char-at-runtime.ll \
  llvm/test/CodeGen/CD/cdbc-machine-parity.list \
  docs/cd-bytecode-llvm-abi.md \
  docs/cd-bytecode-machine-backend.md \
  llvm/lib/Target/CD/README.md \
  docs/superpowers/plans/2026-07-31-cd-bytecode-roadmap.md \
  docs/superpowers/plans/2026-08-03-cd-bytecode-development-plan.md
git commit -m "feat(cd): lower string native helpers"
~~~

Completed on 2026-08-03. Focused lit passed `4/4`; the full CD suite passed
with `72` supported tests and `1` expected unsupported VM integration test;
direct/machine parity passed all `49` entries; the positive fixture produced an
empty line, `🙂e`, and `🙂`; and the nested checkout remained clean.

## Task 2: Define the public debugger query contract before adding commands

**Files:**
- Read only: `cd-compiler/vm-rs/src/vm.rs`, `cd-compiler/vm-rs/src/main.rs`, `cd-compiler/vm-rs/tests/library_api.rs`, and the debugger-related Rust tests.
- Create: `docs/cd-bytecode-debugger-contract.md`.
- Create: `llvm/test/CodeGen/CD/cdbc-debug-contract.ll`.
- Modify: `llvm/test/CodeGen/CD/cdbc-machine-parity.list`,
  `llvm/utils/cd_bytecode_parity_test.py`, and verify with
  `llvm/utils/cd_bytecode_parity.py`.
- Modify: `docs/cd-bytecode-machine-backend.md`, `docs/cd-bytecode-verification.md`, and `docs/superpowers/plans/2026-07-31-cd-bytecode-roadmap.md` only after the contract is written and reviewed locally.

The existing command surface is already covered for `continue`/`c`,
`step`/`s`, `next`/`n`, `break`, `break-range`, `delete`, `help`, and
`quit`/`q`. Do not add `list`, `where`, frame inspection, local inspection, or
breakpoint listing as ad-hoc commands.

- [x] **Step 1: Inventory the existing state machine and output surfaces**

Record the exact states and transitions exercised by the current VM:

~~~
entry pause -> continue -> running -> exit
entry pause -> step/next -> source pause -> continue -> exit
entry pause -> continue -> runtime-error pause -> quit
~~~

For each pause, record the function, module, instruction, source location or
`<unknown>`, stack representation, breakpoint identity, reason, resume marker,
and exit status. Treat current direct/machine parity tolerances for synthetic
entry locations as explicit contract text, not implicit harness behavior.

- [x] **Step 2: Specify deterministic query and mutation semantics**

The new contract document must define, before implementation, the exact output
grammar and error behavior for state queries, source listing, stack queries,
locals, breakpoint listing, invalid commands, EOF, and commands issued while
paused on an error. Every field must have a direct/machine parity rule: exact
equality, normalized register identity, or an explicitly allowed synthetic
location difference.

- [x] **Step 3: Add contract-only fixtures and self-checks**

Create `cdbc-debug-contract.ll` with explicit `!cd.sources`, `!dbg`, and
`!cd.ranges` metadata for one entry pause, one source pause, and one runtime
failure. Add it to `cdbc-machine-parity.list` as a `state` case using only
commands already supported by the VM. The fixture and parity output cover every
documented current-state field; a future command or field is not considered
part of the public contract until it has a fixture and an exact direct/machine
comparison rule.

- [x] **Step 4: Set the implementation boundary**

If the contract requires new public VM output or query state, create a
separate nested-checkout implementation task and a corresponding outer parity
fixture. The contract is expressible through existing output, so this slice
adds only the outer fixture, harness contract, and documentation. No
dynamic-value transport or callback work is included.

Completed on 2026-08-03. Focused debugger lit passed `5/5`; the full CD lit
directory passed `73` tests with `1` expected VM unsupported case; direct/machine
parity passed `50/50`; parity self-tests passed `14/14`; module-link self-tests
passed `5/5`; and Rust VM tests passed `73 + 3 + 8`. The direct/machine state
exception is limited to the synthetic entry location and its optional `main 0`
debug-location record. The next task is the separate dynamic-value transport
ABI decision.

## Task 3: Design the dynamic-value transport ABI as a separate decision gate

**Files:**
- Read: `llvm/lib/Target/CD/CDValueABI.{h,cpp}`, `llvm/lib/Target/CD/CDBytecodeEmitter.cpp`, `llvm/lib/Target/CD/CDMachineBytecodeEmitter.cpp`, `llvm/lib/Target/CD/CDBytecodeFormat.{h,cpp}`, and the matching Rust VM value/call-frame code.
- Create: `docs/superpowers/plans/2026-08-03-cd-dynamic-value-transport-decision.md`.
- Modify only after the decision: `docs/cd-bytecode-llvm-abi.md`, `docs/cd-bytecode-machine-backend.md`, `llvm/lib/Target/CD/README.md`, and the relevant target sources/tests.

The current ABI permits dynamic CD values only inside explicit local
`llvm.cd.*` consumers. The next boundary must decide how a value crosses each
of these edges without treating every `ptr` as a CD value:

~~~
explicit intrinsic result -> defined function parameter
defined function return -> caller result
explicit value -> PHI/select
explicit value -> one-slot local storage
function value -> callback native argument
~~~

- [x] **Step 1: Write the provenance matrix**

For each edge, record accepted producers, accepted LLVM types, nil behavior,
ordinary-pointer rejection, aliasing behavior, and whether the value may be
observed after a mutating `assign_field` or `assign_index` operation. The
matrix must keep `ptr addrspace(0)` as an opaque representation rather than a
native address.

- [x] **Step 2: Compare the three boundary mechanisms**

Evaluate an explicit boundary intrinsic/attribute, a provenance-checked
address-space-zero pointer convention, and a new artifact-level value type.
Choose the smallest mechanism that preserves `cdbc 0.1`, does not accept
ordinary pointers, and can be implemented identically by direct and machine
paths. Record the rejected alternatives and why they fail the current VM
semantics.

- [x] **Step 3: Define the minimal first transport slice**

The decision record must choose one independently testable slice: function
parameter/return transport, PHI/select propagation, or one-slot local storage.
It must explicitly defer the other two until the first slice has direct/machine
parity. It must also define whether a new wire operation is required; the
default assumption is reuse of existing `Call`, `Move`, `LoadVar`, `StoreVar`,
and `Return` operations with no artifact-version change.

- [x] **Step 4: Stop before implementation until the decision is complete**

Run the existing baseline gates and commit the decision record separately. Do
not modify `CDBytecodeFormat`, emitters, or the nested VM merely to make a
pointer-shaped test compile. The next implementation plan must name the exact
chosen boundary, its fixture grammar, and its Rust VM oracle checks.

Completed on 2026-08-03: the decision record chooses the explicit
`cd.value.params`/`cd.value.return` function attributes plus address-space-zero
provenance checks, with function parameter/return transport as the first
implementation slice. PHI/select, one-slot local storage, and callback
function-value transport remain deferred. The slice reuses `Call` and
`Return` with no artifact-format or nested-VM change. Baseline verification
passed: CD lit `73 passed / 1 unsupported`, parity unit tests `14/14`,
module-link unit tests `5/5`, Rust VM tests `73 + 3 + 8`, `git diff --check`,
and a clean nested checkout.

## Task 4: Implement the first dynamic-value transport slice

**Files:**
- Modify: `llvm/lib/Target/CD/CDValueABI.{h,cpp}`.
- Modify: `llvm/lib/Target/CD/CDBytecodeEmitter.cpp` and
  `llvm/lib/Target/CD/CDMachineBytecodeEmitter.cpp`.
- Modify: `llvm/test/CodeGen/CD/cdbc-machine-nil-return.ll` and
  `llvm/test/CodeGen/CD/cdbc-machine-parity.list`.
- Create: `llvm/test/CodeGen/CD/cdbc-function-values.ll` and
  `llvm/test/CodeGen/CD/cdbc-function-value-errors.ll`.
- Modify: `docs/cd-bytecode-llvm-abi.md`, `docs/cd-bytecode-machine-backend.md`,
  `llvm/lib/Target/CD/README.md`, and
  `docs/superpowers/plans/2026-07-31-cd-bytecode-roadmap.md`.
- Do not modify: `CDBytecodeFormat`, the `.cdbc 0.1` format, or the nested VM.

The first implementation slice uses the decision in
`docs/superpowers/plans/2026-08-03-cd-dynamic-value-transport-decision.md`:
`"cd.value.params"` marks a sorted list of address-space-zero pointer
parameters and `"cd.value.return"` marks a pointer return. The shared validator
must reject omitted or malformed markers, ordinary pointer provenance, and
indirect/declaration calls before either emitter lowers a boundary.

- [x] Add shared attribute grammar and function ABI preflight.
- [x] Recognize marked `Argument` values and direct defined marked-return call
  results in `isCDValue`.
- [x] Reuse existing parameter metadata, `Call`, and `Return` lowering in both
  paths while preserving scalar calls and ordinary pointer rejection.
- [x] Add positive identity/mutation/nil and malformed direct/machine fixtures.
- [x] Add the positive fixture to direct/machine parity and verify Rust
  dump/run behavior without changing the nested checkout.
- [x] Update all public ABI, machine, target README, and roadmap records.

Completed on 2026-08-03. Focused function-value lit passed `3/3`; the full CD
lit and expanded parity manifest passed after the slice was added; Rust VM
behavior produced `7` and `nil` on both paths; and the nested checkout remained
clean and unchanged. PHI/select, dynamic local storage, and function-value
callback transport remain explicitly deferred.

## Task 5: Roll out selected callback native helpers

This dependent lane follows the debugger/runtime contract and the dynamic
function-value transport contract. The selected helpers are `map` and its
follow-on `filter` slice; the remaining callback names stay outside the
allowlist until their own matrices are defined.

**Files to audit before writing the implementation plan:**
- Read: `cd-compiler/vm-rs/src/vm.rs` native dispatch and callback frame code, `cd-compiler/vm-rs/tests/library_api.rs`, `cd-compiler/docs/bytecode-text-format.md`, `llvm/lib/Target/CD/CDValueABI.cpp`, and `llvm/test/CodeGen/CD/cdbc-machine-parity.list`.
- Future outer files: `llvm/lib/Target/CD/CDValueABI.cpp`, the two emitters, `CDBytecodeFormat` only if the chosen contract needs a new operation, callback fixtures, the parity harness, and the three ABI documents.

- [x] **Step 1: Capture the Rust callback matrix**

The Rust dispatch audit records exact arity, callback result type/truthiness,
empty-input result, mutation/aliasing behavior, instruction-budget accounting,
cancellation ordering, and error text for:

~~~
map, filter, flatMap,
any, all, count, find, findIndex,
reduce
~~~

`map` requires two arguments and a one-argument callback, returns a fresh array,
returns an empty fresh array for empty input, snapshots input elements, and
preserves shared dynamic handles passed to the callback. Each input element
gets a native instruction checkpoint before the callback; callback body
instructions are charged normally, and the output allocation charges one array
plus its elements. Cancellation is checked before budget/resource growth. The
other helpers' matrices remain documented by the unchanged Rust tests but are
not admitted by this slice.

- [x] **Step 2: Select one vertical slice**

`map` is represented as:

~~~
llvm.cd.native(ptr name, ptr value, ptr callback) -> ptr
~~~

The value is a proven CD token whose runtime tag must be an array. The callback
is a direct defined LLVM function with exactly one address-space-zero pointer
parameter marked `cd.value.params="0"` and an address-space-zero pointer return
marked `cd.value.return`. The target materializes it with the existing
`make_function` operation before `native_call`; no new artifact operation is
needed. Positive empty/non-empty, malformed shape/callback, runtime type-error,
resource-budget, cancellation, direct/machine, and nested-VM checks are
covered by the target fixtures plus the unchanged Rust callback tests.

- [x] **Step 3: Keep the remaining helpers rejected**

Unknown and not-yet-selected callback names continue to fail with the stable
bounded-ABI diagnostic. No generic external-call escape hatch or callback
pseudo bypasses `CDBytecodeFormat` validation.

- [x] **Step 4: Add the `filter` callback helper**

`filter` reuses the same transport:

~~~
llvm.cd.native(ptr name, ptr value, ptr predicate) -> ptr
~~~

The value is a proven CD token whose runtime tag must be an array. The
predicate is a direct defined function with one address-space-zero pointer
parameter marked `cd.value.params="0"` and an exact `i1` return; it does not use
`cd.value.return`. Both emitters materialize it with `make_function` before the
existing `native_call`. The Rust VM owns the snapshot, fresh shallow-array,
left-to-right predicate, boolean-result, resource-budget, cancellation, and
runtime type semantics. Positive empty/all/none cases, the non-array runtime
error, direct/machine parity, and malformed callback/shape diagnostics are
covered by `cdbc-native-filter*.ll` and `cdbc-native-errors.ll`.

The filter slice adds no opcode, artifact field, `.cdbc 0.1` version, or nested
VM change.

Completed on 2026-08-04 for the selected `map`/`filter` callback lane. The
full local gate passed with 80 lit tests (79 passed, 1 unsupported), parity
55/55, parity unit 14/14, module-link unit 5/5, and Rust VM tests `73 + 3 + 8`.
The unchanged Rust VM callback budget test continues to cover instruction
charging, and the nested checkout remains clean.

## Task 6: Extend the predicate callback lane with `any` and `all`

This follow-on reuses the completed function-value transport and `filter`
predicate ABI. It adds no opcode, artifact field, `.cdbc 0.1` version, or
nested VM change.

**Files:**
- Modify: `llvm/lib/Target/CD/CDValueABI.cpp` and
  `llvm/lib/Target/CD/CDBytecodeFormat.cpp`.
- Create: `llvm/test/CodeGen/CD/cdbc-native-any-all.ll`,
  `llvm/test/CodeGen/CD/cdbc-native-any-runtime.ll`,
  `llvm/test/CodeGen/CD/cdbc-native-all-runtime.ll`, and
  `llvm/test/CodeGen/CD/cdbc-native-predicate-errors.ll`.
- Modify: `llvm/test/CodeGen/CD/cdbc-machine-parity.list`, the ABI/machine/
  README/verification documents, and the two roadmap records.
- Modify: `.github/workflows/cd-bytecode.yml` so clean CI builds the lit
  `not` utility used by CD diagnostics.

- [x] Admit `any` and `all` only for a proven CD token, a direct defined
  one-parameter predicate marked by `cd.value.params="0"`, and an exact `i1`
  result; keep `cd.value.return` forbidden for the predicate.
- [x] Reuse `make_function` and `native_call` in both direct and machine paths.
- [x] Cover empty-array identities, positive predicate results, runtime
  non-array failures, malformed shape/pointer/callback diagnostics, and
  direct/machine parity.
- [x] Keep `flatMap`, `count`, `find`, `findIndex`, and `reduce` rejected.

Completed on 2026-08-07. Focused predicate lit passed `4/4`; the local CD
suite passed `83` tests with `1` expected unsupported VM integration case;
direct/machine parity passed `58/58`; parity unit tests passed `14/14`,
module-link unit tests passed `5/5`, Rust VM tests passed `73 + 3 + 8`, and
the nested checkout remained clean. Hosted CI remains pending publication of
the `not` build fix.

## Completion and delivery gates

A task is complete only when implementation, ABI docs, README, roadmap status,
fixtures, and parity rules agree. The applicable checks are:

~~~
ninja -C build-cd llc FileCheck count not
env -u CD_COMPILER_ROOT build-cd/bin/llvm-lit -sv llvm/test/CodeGen/CD
PYTHONDONTWRITEBYTECODE=1 python3 llvm/utils/cd_bytecode_parity_test.py
PYTHONDONTWRITEBYTECODE=1 python3 llvm/utils/cd_module_link_test.py
cargo test --manifest-path cd-compiler/vm-rs/Cargo.toml
git diff --check
git -C cd-compiler status --short --branch
~~~

Each implementation slice ends with a local commit containing only its outer
checkout changes. Push, merge, PR creation, and branch cleanup are reported as
separate follow-up actions rather than being implied by a successful test run.
