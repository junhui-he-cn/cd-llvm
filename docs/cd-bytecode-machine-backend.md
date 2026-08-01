# CD bytecode machine-backend design gate

Status: M3 foundation design, 2026-08-01.

This document fixes the boundary between LLVM's machine representation and the
existing `cdbc 0.1` artifact.  It is intentionally a design gate: the current
direct `ModulePass` emitter remains the default compatibility path, and this
document does not claim that an opt-in machine path is available yet.

## Invariants

- The direct `ModulePass` path is the compatibility oracle.  A machine-path
  artifact must match its observable Rust VM behavior before it can become a
  default.
- The machine path is opt-in and may emit only CD pseudo-instructions with a
  stable one-to-one `CDOpcode` mapping.  TableGen describes instruction shape,
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

## TableGen pseudo-instruction mapping

The generated machine opcode names are deliberately explicit.  They describe
the only scalar/control-flow operations that currently have stable
`cdbc 0.1` representations.

| TableGen opcode | Artifact opcode | Machine operands |
| --- | --- | --- |
| `CD_CONSTANT` | `Constant` | destination value, module constant index |
| `CD_MAKE_FUNCTION` | `MakeFunction` | destination value, module function index |
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

`JumpIfTrue` is not part of the first generated set because the direct emitter
does not currently produce it.  It can be added only with a matching direct
path and artifact-bridge test.

## Explicitly deferred

The following are outside this foundation slice and must not be smuggled into
the pseudo-instruction model:

- arrays, maps, strings, structs, variants, arbitrary pointers, and aggregate
  values;
- native calls, globals, exception edges, and source/debug sections;
- object files, assembly encodings, MC emitters, and JIT execution;
- physical register allocation, spills, and a claim that `R0`--`R31` model a
  host ABI;
- module-table ownership changes or serialization that bypasses
  `CDBytecodeFormat` validation.

The next M3 implementation slice may create `MachineFunction` objects and
lower the existing scalar/control-flow subset to these pseudo-operations, but
it must retain the direct path and compare both paths through the typed
artifact model.
