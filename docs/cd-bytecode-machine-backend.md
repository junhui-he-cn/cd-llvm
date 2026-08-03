# CD bytecode machine-backend design gate

Status: M3 complete for the supported scalar/control-flow subset; M4 string,
array-constructor, array-access, array-mutation, map-constructor, record-value,
enum-variant, and bounded native-call slices share the machine artifact bridge;
the M5 explicit debug-source-table, instruction-location, source-backed
runtime-diagnostic, debug-range, and scripted debugger slices also share that
bridge; broader native-call capabilities and broader interactive debugger
behavior remain deferred, 2026-08-03.

This document fixes the boundary between LLVM's machine representation and the
existing `cdbc 0.1` artifact.  It remains a design gate while the current
direct `ModulePass` emitter stays the default compatibility path.  The opt-in
machine path is available for the narrow implementation boundary below and
remains intentionally incomplete.

## Invariants

- The direct `ModulePass` path is the compatibility oracle.  A machine-path
  artifact must match its observable Rust VM behavior before it can become a
  default.
- The machine path is opt-in and may emit only CD pseudo-instructions with a
  stable `cdbc 0.1` mapping.  Lowering pseudos such as `CD_SELECT` expand only
  to a documented sequence of existing artifact instructions.  TableGen describes instruction shape,
  register classes, calling-convention metadata, and subtarget identity; it
  does not own the `.cdbc 0.1` wire spelling.
- `CDValue` registers are VM register identities, not native CPU registers.
  The first machine slice runs before physical register allocation and does
  not emit an object file.  The generated register class exists to give
  `MachineInstr` operands a stable type; a future `TargetRegisterInfo` must
  reserve its descriptor registers before any host register allocation is
  considered, and allocation remains deferred until a VM-register to
  artifact-register contract exists.
- The module owns the constant table, name table, function table, and function
  indexes.  Each `MachineFunction` owns only its body instructions, parameter
  metadata, and symbolic basic-block labels.  No function may independently
  renumber module tables.
- Calls carry a callee VM register and a variadic list of `CDValue` operands.
  The future bridge checks the function arity against the module function
  table before constructing `CDInstruction::call`.
- A later artifact bridge resolves symbolic block labels to instruction offsets,
  constructs `llvm::cd::CDArtifact`, and runs `validateArtifact` before the
  canonical serializer writes any output.

## Current implementation boundary

`llc -mtriple=cd-unknown-unknown -cd-backend=machine` constructs a
`MachineFunction` for the no-argument `@main` entry and every defined helper,
then bridges those bodies to the typed artifact model.  The supported values
and operations are scalar constants, arithmetic, comparisons, scalar casts as
`move`, `fneg`, boolean inversion as `not`, `nil`/`ret void` returns,
`cd_print`/`print`, defined-function calls, scalar function parameters,
single-slot scalar storage through `load_var` and `store_var`, conditional and
unconditional branches, scalar PHI values, and scalar `select`.  Conditional PHI edges use
synthetic machine edge blocks so each predecessor edge stores the correct
incoming value before jumping to the successor; symbolic machine block
targets are patched to artifact instruction offsets before validation.

Constants and function values are materialized at each use site.  This keeps
their VM registers defined on every control-flow path instead of allowing a
register definition in one edge block to leak into an unrelated edge.
The M4 value ABI additionally lowers immutable string tokens to `constant` and
fresh array constructors to `CD_ARRAY`/`array`.  Array operands are collected
before the machine pseudo is inserted so constants materialized for operands
are defined before the array instruction executes.  The reproducible parity
gate is implemented by `llvm/utils/cd_bytecode_parity.py` and the corpus
manifest at `llvm/test/CodeGen/CD/cdbc-machine-parity.list`.

The array-access slice adds `CD_INDEX`, `CD_LEN`, and `CD_ASSERT_ARRAY`. Their
operands stay in the `CDValue` virtual register class, and the bridge emits
the existing `index`, `len`, and `assert_array` artifact operations after
validating that every opaque pointer operand came from an explicit CD
intrinsic. Scalar index constants are materialized before `CD_INDEX`, so the
machine path preserves the direct path's definition-before-use ordering.

The array-mutation slice adds `CD_ASSIGN_INDEX`. Its collection, index, and
assigned-value operands remain CD virtual values; the direct and machine
lowerers share the overloaded intrinsic validator, and the bridge emits the
existing `assign_index` operation. Scalar and pointer overloads both return
the assigned value's register type. No ordinary pointer or LLVM store is
treated as a mutation operation.

The map-constructor slice adds `CD_MAP`. Its alternating key/value operands are
collected and materialized before the pseudo is inserted, preserving
definition-before-use ordering for scalar constants. The bridge validates the
even operand shape and emits the existing `map` artifact operation; duplicate
keys, nested handles, and runtime lookup failures remain Rust VM semantics.

The record-value slice adds `CD_STRUCT`, `CD_FIELD`, and `CD_ASSIGN_FIELD`.
Struct name operands are module name-table indexes while field values and
object/result operands stay in the `CDValue` virtual register class. Name
metadata and dynamic-value operands are collected before the pseudo is inserted,
so a field value produced by another constructor is defined before the struct
instruction executes. The bridge preserves anonymous versus nominal type names,
field order, and the existing `struct`, `field`, and `assign_field` artifact
operations. Ordinary pointers remain rejected by the shared ABI validator.

The enum-variant slice adds `CD_VARIANT`, `CD_VARIANT_TAG`, and
`CD_VARIANT_FIELD`. Enum and variant names are module name-table indexes;
payload, value, and result operands remain `CDValue` virtual registers, while
the payload index is an immediate. The bridge emits the existing `variant`,
`variant_tag`, and `variant_field` artifact operations, preserving payload
order and the Rust VM's false/non-variant and bounds-error behavior. Payload
registers are materialized before `CD_VARIANT` is inserted, so nested explicit
CD constructors retain definition-before-use ordering. Ordinary pointers and
aggregates remain rejected by the shared ABI validator.

The bounded native-call slice covers `floor`, `ceil`, `sqrt`, `str`, `typeOf`,
`hash`, and `range`. Their name-table index is an immediate machine operand,
while arguments and the result remain in the `CDValue` virtual register class.
The `CD_NATIVE_CALL` bridge emits the existing `native_call` artifact operation
after shared name-specific validation; callback natives, unsupported names,
and ordinary pointer arguments remain outside this boundary.

The M5 source-table slice parses the same explicit `!cd.sources` named metadata
as the direct path and moves the validated entries into the shared artifact
model. The follow-on location slice propagates each source instruction's
`DILocation` into the generated CD pseudo and resolves it at the artifact
bridge, so direct and machine paths emit identical sparse `debug_locations`
entries. The explicit `!cd.ranges` table is resolved by the same
`DILocation` identity at the artifact bridge, so both paths emit identical
sparse `debug_ranges` entries after validating source-local byte bounds.
The runtime-error parity gate also confirms that a nested divide-by-zero keeps
the same source line, caret, and call stack through both artifact paths. It also
covers the bounded `sqrt(-1)` native failure with the same source-backed main
call stack. It also covers an out-of-range array index with the same source
line, caret, and main call stack. The `debug-error` parity case additionally
drives the interactive debugger through the entry pause to a source-backed
runtime-error pause, comparing the exact error pause line while allowing
machine-specific synthetic entry locations.

## Direct/machine parity gate

From the repository root, build the sibling VM explicitly and run the manifest:

```sh
cargo build --manifest-path cd-compiler/vm-rs/Cargo.toml --quiet
python3 llvm/utils/cd_bytecode_parity.py \
  --llc build-cd/bin/llc \
  --vm cd-compiler/vm-rs/target/debug/compiler-design-vm \
  --manifest llvm/test/CodeGen/CD/cdbc-machine-parity.list
```

Every entry must produce two artifacts from the same LLVM IR, pass Rust
`dump`, and produce identical Rust VM output.  `artifact` entries additionally
compare the generated text after canonicalizing only constant, name, function,
and virtual-register indices.  `behavior` entries cover machine-specific
control-flow/select expansion and require the same dump/run behavior without
discarding those instruction-shape differences. `runtime-error` entries
require both artifacts to pass `dump`, both `run` commands to fail, and both
diagnostics to contain the declared expected substring and match each other.

The parity manifest also accepts opt-in observability cases:

```text
observability <input> "<semicolon-separated debugger commands>" <ranges|metadata-free|step-next|aliases|line-delete>
```

Run this gate with the same explicitly built sibling VM executable and the
`cargo build` plus `python3 llvm/utils/cd_bytecode_parity.py` commands above;
the gate requires that sibling VM executable and is not an LLVM default or CI
gate.  An observability case compares direct and machine `dump` debug sections,
`run`, `trace`, `profile`, and scripted `debug` output.  The `ranges` contract
uses `ranges.cd`, verifies debug range `s0:6:11`, and checks the trace, profile,
and debug source-range surfaces.  The `metadata-free` contract verifies no
debug tail, `<unknown>` in trace and debug output, and no `range=` or
`source_range` field on any surface.
The `step-next` contract uses the range-backed fixture with `step` followed by
`next` and requires distinct `reason=step` and `reason=next` pauses, along with
the corresponding resume commands and a clean debugger quit.
The `aliases` contract feeds the equivalent short `s`, `n`, and `q` commands
and requires the same canonical step/next pause and resume events.
The `line-delete` contract creates a source-line breakpoint, requires exactly
one breakpoint pause, deletes it, and verifies that execution resumes to the
fixture's final output. A `debug-error` case has the form:

```text
debug-error <input> "<semicolon-separated debugger commands>" "<error-pause substring>"
```

It requires both artifacts to pass `dump`, then compares the unique
`pause reason=error` line and the debugger resume/quit markers. The VM's
ordinary `run` diagnostic remains covered by a separate `runtime-error` entry;
this case exists because `trace` and `profile` intentionally return failure for
the same runtime error.

## TableGen pseudo-instruction mapping

The generated machine opcode names are deliberately explicit.  They describe
the only currently supported operations that have stable `cdbc 0.1`
representations.

| TableGen opcode | Artifact opcode | Machine operands |
| --- | --- | --- |
| `CD_CONSTANT` | `Constant` | destination value, module constant index |
| `CD_MAKE_FUNCTION` | `MakeFunction` | destination value, module function index |
| `CD_ARRAY` | `Array` | destination value, variadic element values |
| `CD_MAP` | `Map` | destination value, variadic key/value values |
| `CD_STRUCT` | `Struct` | destination value, optional type-name index, variadic field name/value operands |
| `CD_VARIANT` | `Variant` | destination value, enum-name index, variant-name index, variadic payload values |
| `CD_VARIANT_TAG` | `VariantTag` | destination value, value, enum-name index, variant-name index |
| `CD_VARIANT_FIELD` | `VariantField` | destination value, value, immediate payload index |
| `CD_FIELD` | `Field` | destination value, object value, field-name index |
| `CD_ASSIGN_FIELD` | `AssignField` | destination value, object value, field-name index, assigned value |
| `CD_INDEX` | `Index` | destination value, collection value, index value |
| `CD_ASSIGN_INDEX` | `AssignIndex` | destination value, collection value, index value, assigned value |
| `CD_LEN` | `Len` | destination value, collection value |
| `CD_ASSERT_ARRAY` | `AssertArray` | destination value, value |
| `CD_MOVE` | `Move` | destination value, source value |
| `CD_LOAD_VAR` | `LoadVar` | destination value, module name index |
| `CD_STORE_VAR` | `StoreVar` | module name index, source value |
| `CD_CALL` | `Call` | destination value, callee value, variadic arguments |
| `CD_NATIVE_CALL` | `NativeCall` | destination value, native-name index, variadic arguments |
| `CD_PRINT` | `Print` | source value |
| `CD_RETURN` | `Return` | source value |
| `CD_NEGATE` | `Negate` | destination value, source value |
| `CD_NOT` | `Not` | destination value, source value |
| `CD_ADD` | `Add` | destination value, left value, right value |
| `CD_SUBTRACT` | `Subtract` | destination value, left value, right value |
| `CD_MULTIPLY` | `Multiply` | destination value, left value, right value |
| `CD_DIVIDE` | `Divide` | destination value, left value, right value |
| `CD_EQUAL` | `Equal` | destination value, left value, right value |
| `CD_NOT_EQUAL` | `NotEqual` | destination value, left value, right value |
| `CD_GREATER` | `Greater` | destination value, left value, right value |
| `CD_GREATER_EQUAL` | `GreaterEqual` | destination value, left value, right value |
| `CD_LESS` | `Less` | destination value, left value, right value |
| `CD_LESS_EQUAL` | `LessEqual` | destination value, left value, right value |
| `CD_JUMP` | `Jump` | symbolic target block label |
| `CD_JUMP_IF_FALSE` | `JumpIfFalse` | condition value, symbolic target block label |
| `CD_SELECT` | `jump_if_false` + `move` + `jump` + `move` | destination value, condition value, true value, false value |

`JumpIfTrue` is not part of the first generated set because the direct emitter
does not currently produce it.  It can be added only with a matching direct
path and artifact-bridge test.

## Explicitly deferred

The following are outside this foundation slice and must not be smuggled into
the pseudo-instruction model:

- arbitrary pointers and aggregate values;
- unbounded/callback native calls beyond the bounded allowlist;
- globals and exception edges;
- broader source/debug sections not covered by the M5 slices;
- object files, assembly encodings, MC emitters, and JIT execution;
- physical register allocation, spills, and a claim that `R0`--`R31` model a
  host ABI;
- module-table ownership changes or serialization that bypasses
  `CDBytecodeFormat` validation.

The current M4 map, record, enum-variant, and bounded native-call slices retain
the direct path, keep the machine path opt-in, and define collection/value
construction separately from aggregate or ordinary-pointer lowering. Native
calls beyond the allowlist above still require a separate name-specific
capability matrix before new pseudos are implemented.
