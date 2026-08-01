# CD bytecode machine-backend design gate

Status: M3 complete for the supported scalar/control-flow subset; M4 string
and array-constructor slices share the machine artifact bridge, 2026-08-01.

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

## Direct/machine parity gate

Build the sibling VM explicitly, then run the manifest from the LLVM checkout:

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
discarding those instruction-shape differences.

## TableGen pseudo-instruction mapping

The generated machine opcode names are deliberately explicit.  They describe
the only scalar/control-flow operations that currently have stable
`cdbc 0.1` representations.

| TableGen opcode | Artifact opcode | Machine operands |
| --- | --- | --- |
| `CD_CONSTANT` | `Constant` | destination value, module constant index |
| `CD_MAKE_FUNCTION` | `MakeFunction` | destination value, module function index |
| `CD_ARRAY` | `Array` | destination value, variadic element values |
| `CD_MOVE` | `Move` | destination value, source value |
| `CD_LOAD_VAR` | `LoadVar` | destination value, module name index |
| `CD_STORE_VAR` | `StoreVar` | module name index, source value |
| `CD_CALL` | `Call` | destination value, callee value, variadic arguments |
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

- maps, structs, variants, arbitrary pointers, and aggregate values;
- native calls, globals, exception edges, and source/debug sections;
- object files, assembly encodings, MC emitters, and JIT execution;
- physical register allocation, spills, and a claim that `R0`--`R31` model a
  host ABI;
- module-table ownership changes or serialization that bypasses
  `CDBytecodeFormat` validation.

The next stage is the M4 CD value ABI design.  It must retain the direct path,
keep the machine path opt-in, and define target-specific operations before any
aggregate or pointer lowering is added.
